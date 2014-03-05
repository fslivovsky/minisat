/***************************************************************************************[Solver.cc]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT
OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
**************************************************************************************************/

#include <math.h>

#include "mtl/Sort.h"
#include "core/Solver.h"
#include "core/ProofVisitor.h"

using namespace Minisat;

//=================================================================================================
// Options:


static const char* _cat = "CORE";

static DoubleOption  opt_var_decay         (_cat, "var-decay",   "The variable activity decay factor",            0.95,     DoubleRange(0, false, 1, false));
static DoubleOption  opt_clause_decay      (_cat, "cla-decay",   "The clause activity decay factor",              0.999,    DoubleRange(0, false, 1, false));
static DoubleOption  opt_random_var_freq   (_cat, "rnd-freq",    "The frequency with which the decision heuristic tries to choose a random variable", 0, DoubleRange(0, true, 1, true));
static DoubleOption  opt_random_seed       (_cat, "rnd-seed",    "Used by the random variable selection",         91648253, DoubleRange(0, false, HUGE_VAL, false));
static IntOption     opt_ccmin_mode        (_cat, "ccmin-mode",  "Controls conflict clause minimization (0=none, 1=basic, 2=deep)", 0, IntRange(0, 2));
static IntOption     opt_phase_saving      (_cat, "phase-saving", "Controls the level of phase saving (0=none, 1=limited, 2=full)", 2, IntRange(0, 2));
static BoolOption    opt_rnd_init_act      (_cat, "rnd-init",    "Randomize the initial activity", false);
static BoolOption    opt_luby_restart      (_cat, "luby",        "Use the Luby restart sequence", true);
static IntOption     opt_restart_first     (_cat, "rfirst",      "The base restart interval", 100, IntRange(1, INT32_MAX));
static DoubleOption  opt_restart_inc       (_cat, "rinc",        "Restart interval increase factor", 2, DoubleRange(1, false, HUGE_VAL, false));
static DoubleOption  opt_garbage_frac      (_cat, "gc-frac",     "The fraction of wasted memory allowed before a garbage collection is triggered",  HUGE_VAL, DoubleRange(0, false, HUGE_VAL, false));

static BoolOption    opt_valid             (_cat, "valid",    "Validate UNSAT answers", true);


//=================================================================================================
// Constructor/Destructor:


Solver::Solver() :

    // Parameters (user settable):
    //
    verbosity        (0)
  , log_proof (opt_valid)
  , var_decay        (opt_var_decay)
  , clause_decay     (opt_clause_decay)
  , random_var_freq  (opt_random_var_freq)
  , random_seed      (opt_random_seed)
  , luby_restart     (opt_luby_restart)
  , ccmin_mode       (opt_ccmin_mode)
  , phase_saving     (opt_phase_saving)
  , rnd_pol          (false)
  , rnd_init_act     (opt_rnd_init_act)
  , garbage_frac     (opt_garbage_frac)
  , restart_first    (opt_restart_first)
  , restart_inc      (opt_restart_inc)

    // Parameters (the rest):
    //
  , learntsize_factor((double)1/(double)3), learntsize_inc(1.1)

    // Parameters (experimental):
    //
  , learntsize_adjust_start_confl (100)
  , learntsize_adjust_inc         (1.5)

    // Statistics: (formerly in 'SolverStats')
    //
  , solves(0), starts(0), decisions(0), rnd_decisions(0), propagations(0), conflicts(0)
  , dec_vars(0), clauses_literals(0), learnts_literals(0), max_literals(0), tot_literals(0)

  , ok                 (true)
  , cla_inc            (1)
  , var_inc            (1)
  , watches            (WatcherDeleted(ca))
  , qhead              (0)
  , simpDB_assigns     (-1)
  , simpDB_props       (0)
  , order_heap         (VarOrderLt(activity))
  , progress_estimate  (0)
  , remove_satisfied   (true)

    // Resource constraints:
    //
  , conflict_budget    (-1)
  , propagation_budget (-1)
  , asynch_interrupt   (false)
  , currentPart (1)
  , start(0)
{}


Solver::~Solver()
{
}


// === Validation

bool Solver::validate ()
{
  assert (log_proof);
  assert (!ok);
  assert (proof.size () > 0);


  // -- final conflict clause is in the core
  Clause &last = ca [proof.last ()];
  last.core (1);
  // -- mark all reasons for the final conflict as core
  for (int i = 0; i < last.size (); i++)
    {
      // -- validate that the clause is really a conflict clause
      if (value (last [i]) != l_False) return false;
      Var x = var (last [i]);
      ca [reason (x)].core (1);
    }

  int trail_sz = trail.size ();
  ok = true;
  // -- move back through the proof, shrinking the trail and
  // -- validating the clauses
  for (int i = proof.size () - 2; i >= 0; i--)
    {
      if (verbosity >= 2) fflush (stdout);
      CRef cr = proof [i];
      assert (cr != CRef_Undef);
      Clause &c = ca [cr];

      //if (verbosity >= 2) printf ("Validating lemma #%d ... ", i);

      // -- resurect deleted clauses
      if (c.mark () == 1)
        {
          // -- undelete
          c.mark (0);
          Var x = var (c[0]);

          // -- if non-unit clause, attach it
          if (c.size () > 1) attachClause (cr);
          else // -- if unit clause, enqueue it
            {
              bool res = enqueue (c[0], cr);
              assert (res);
            }
          if (verbosity >= 2) printf ("^");
          continue;
        }

      assert (c.mark () == 0);
      // -- detach the clause
      if (locked (c))
        {
          // -- undo the bcp
          while (trail[trail_sz - 1] != c[0])
            {
              Var x = var (trail [trail_sz - 1]);
              assigns [x] = l_Undef;
              insertVarOrder (x);
              trail_sz--;

              CRef r = reason (x);
              assert (r != CRef_Undef);
              // -- mark literals of core clause as core
              if (ca [r].core ())
                {
                  Clause &rc = ca [r];
                  for (int j = 1; j < rc.size (); ++j)
                    {
                      Var x = var (rc [j]);
                      ca [reason (x)].core (1);
                    }
                }
            }
          assert (c[0] == trail [trail_sz - 1]);
          // -- unassign the variable
          assigns [var (c[0])] = l_Undef;
          // -- put it back in order heap in case we want to restart
          // -- solving in the future
          insertVarOrder (var (c[0]));
          trail_sz--;
        }
      // -- unit clauses don't need to be detached from watched literals
      if (c.size () > 1) detachClause (cr);
      // -- mark clause deleted
      c.mark (1);


      if (c.core () == 1)
        {
          assert (value (c[0]) == l_Undef);
          // -- put trail in a good state
          trail.shrink (trail.size () - trail_sz);
          qhead = trail.size ();
          if (trail_lim.size () > 0) trail_lim [0] = trail.size ();
          if (verbosity >= 2) printf ("V");
          if (!validateLemma (cr)) return false;
        }
      else if (verbosity >= 2) printf ("-");


    }
  if (verbosity >= 2) printf ("\n");


  // update trail and qhead
  trail.shrink (trail.size () - trail_sz);
  qhead = trail.size ();
  if (trail_lim.size () > 0) trail_lim [0] = trail.size ();

  // find core clauses in the rest of the trail
  for (int i = trail.size () - 1; i >= 0; --i)
    {
      assert (reason (var (trail [i])) != CRef_Undef);
      Clause &c = ca [reason (var (trail [i]))];
      // -- if c is core, mark all clauses it depends as core
      if (c.core () == 1)
        for (int j = 1; j < c.size (); ++j)
          {
            Var x = var (c[j]);
            ca[reason (x)].core (1);
          }

    }

  if (verbosity >= 1) printf ("VALIDATED\n");
  return true;
}

bool Solver::validateLemma (CRef cr)
{
  assert (decisionLevel () == 0);
  assert (ok);

  Clause &lemma = ca [cr];
  assert (lemma.core ());
  assert (!locked (lemma));

  // -- go to decision level 1
  newDecisionLevel ();

  for (int i = 0; i < lemma.size (); ++i)
    enqueue (~lemma [i]);

  // -- go to decision level 2
  newDecisionLevel ();

  CRef confl = propagate ();
  if (confl == CRef_Undef)
    {
      if (verbosity >= 2) printf ("FAILED: No Conflict from propagate()\n");
      return false;
    }
  Clause &conflC = ca [confl];
  conflC.core (1);
  for (int i = 0; i < conflC.size (); ++i)
    {
      Var x = var (conflC [i]);
      // -- if the variable got value by propagation,
      // -- mark it to be unrolled
      if (level (x) > 1) seen [x] = 1;
      else if (level (x) <= 0) ca [reason(x)].core (1);
    }

  for (int i = trail.size () - 1; i >= trail_lim[1]; i--)
    {
      Var x = var (trail [i]);
      if (!seen [x]) continue;

      seen [x] = 0;
      assert (reason (x) != CRef_Undef);
      Clause &c = ca [reason (x)];
      c.core (1);

      assert (value (c[0]) == l_True);
      // -- for all other literals in the reason
      for (int j = 1; j < c.size (); ++j)
        {
          Var y = var (c [j]);
          assert (value (c [j]) == l_False);

          // -- if the literal is assigned at level 2,
          // -- mark it for processing
          if (level (y) > 1) seen [y] = 1;
          // -- else if the literal is assigned at level 0,
          // -- mark its reason clause as core
          else if (level (y) <= 0)
            // -- mark the reason for y as core
            ca[reason (y)].core (1);
        }
    }
  // reset
  cancelUntil (0);
  ok = true;
  return true;
}

void Solver::replay (ProofVisitor& v)
{
  assert (log_proof);
  assert (proof.size () > 0);
  if (verbosity >= 2) printf ("REPLAYING: ");
  CRef confl = propagate (true);
  // -- assume that initial clause database is consistent
  assert (confl == CRef_Undef);

  labelLevel0(v);

  for (int i = 0; i < proof.size (); ++i)
    {
      if (verbosity >= 2) fflush (stdout);

      CRef cr = proof [i];
      assert (cr != CRef_Undef);
      Clause &c = ca [cr];

      // -- delete clause that was deleted before 
      // -- except for locked and core clauses
      if (c.mark () == 0 && !locked (c) && !c.core ())
        {
          if (c.size () > 1) detachClause (cr);
          c.mark (1);
          if (verbosity >= 2) printf ("-");
          continue;
        }
      // -- if current clause is not core or already present, continue
      if (c.core () == 0 || c.mark () == 0)
        {
          if (verbosity >= 2) printf ("-");
          continue;
        }


      if (verbosity >= 2) printf ("v");

      // -- at least one literal must be undefined
      assert (value (c[0]) == l_Undef);

      newDecisionLevel (); // decision level 1
      for (int j = 0; j < c.size (); ++j) enqueue (~c[j]);
      newDecisionLevel (); // decision level 2
      CRef p = propagate (true);
      assert (p != CRef_Undef);
      // -- XXX Here can run analyze() to rebuild the resolution
      // -- proof, extract interpolants, etc.
      // -- trail at decision level 0 is implied by the database
      // -- trail at decision level 1 are the decision forced by the clause
      // -- trail at decision level 2 is derived from level 1

      // -- undelete the clause and attach it to the database
      // -- unless the learned clause is already in the database
      if (traverseProof (v, cr, p))
      {
        cancelUntil (0);
        c.mark (0);
        // -- if unit clause, add to trail and propagate
        if (c.size () <= 1 || value (c[1]) == l_False)
        {
          assert (value (c[0]) == l_Undef);
          uncheckedEnqueue (c[0], cr);
          confl = propagate (true);
          labelLevel0(v);
          // -- if got a conflict at level 0, bail out
          if (confl != CRef_Undef)
          {
            labelFinal(v, confl);
            break;
          }
        }
        else attachClause (cr);
      }
      else cancelUntil (0);
      
    }

  if (proof.size () == 1) labelFinal (v, proof [0]);
  if (verbosity >= 2)
    {
      printf ("\n");
      fflush (stdout);
    }

  if (verbosity >= 1 && confl != CRef_Undef) printf ("Replay SUCCESS\n");
}

void Solver::labelFinal(ProofVisitor& v, CRef confl)
{
    // The conflict clause is the clause with which we resolve.
    const Clause& source = ca[confl];

    v.chainClauses.clear();
    v.chainPivots.clear();

    v.chainClauses.push(confl);
    // The clause is false, and results in the empty clause,
    // all are therefore seen and resolved.
    for (int i = 0; i < source.size (); ++i)
      v.chainPivots.push(~source [i]);
    
    v.visitChainResolvent(CRef_Undef);
}

bool Solver::traverseProof(ProofVisitor& v, CRef proofClause, CRef confl)
{
    // The clause with which we resolve.
    const Clause& proof = ca[proofClause];

    // The conflict clause
    const Clause& conflC = ca[confl];

    int pathC = conflC.size ();
    for (int i = 0; i < conflC.size (); ++i)
    {
        Var x = var (conflC [i]);
        seen[x] = 1;
    }

    v.chainClauses.clear();
    v.chainPivots.clear();

    v.chainClauses.push(confl);
    // Now walk up the trail.
    for (int i = trail.size () - 1; pathC > 0; i--)
    {
        assert (i >= 0);
        Var x = var (trail [i]);
        if (!seen [x]) continue;

        seen [x] = 0;
        pathC--;

        if (level(x) == 1) continue;

        // --The pivot variable is x.

        assert (reason (x) != CRef_Undef);

        v.chainPivots.push(trail [i]);
        if (level(x) > 0)
        {
          CRef r = reason(x);
          v.chainClauses.push(r);
        }
        else
            continue;

        Clause &r = ca [reason (x)];

        assert (value (r[0]) == l_True);
        // -- for all other literals in the reason
        for (int j = 1; j < r.size (); ++j)
        {
            if (seen [var (r [j])] == 0)
            {
                seen [var (r [j])] = 1;
                pathC++;
            }
        }
    }
    
    if (v.chainPivots.size () == 0) return false;
    v.visitChainResolvent(proofClause);
    return true;
}

void Solver::labelLevel0(ProofVisitor& v)
{
    // -- Walk the trail forward
    vec<Lit> lits;
    int size = trail.size() - 1;
    for (int i = start; i <= size; i++)
    {
        lits.clear();
        Var x = var(trail[i]);
        if (reason(x) == CRef_Undef || ca[reason(x)].size() == 1) continue;

        Clause& c = ca[reason(x)];
        int size = c.size();

        // -- The number of resolution steps at this point is size-1
        // -- where size is the number of literals in the reason clause
        // -- for the unit that is currently on the trail.
        if (size == 2)
        {
          // -- Binary resolution
          v.visitResolvent(trail [i], ~c[1], reason(x));
        }
        else
        {
          v.chainClauses.clear();
          v.chainPivots.clear();

          v.chainClauses.push(reason(x));
          // -- The first literal (0) is the result of resolution, start from 1.
          for (int i=1; i < size; i++)
          {
            v.chainPivots.push(~c[i]);
          }
          v.visitChainResolvent(trail[i]);
        }
    }
    start = size;
}

//=================================================================================================
// Minor methods:


// Creates a new SAT variable in the solver. If 'decision' is cleared, variable will not be
// used as a decision variable (NOTE! This has effects on the meaning of a SATISFIABLE result).
//
Var Solver::newVar(bool sign, bool dvar)
{
    int v = nVars();
    watches  .init(mkLit(v, false));
    watches  .init(mkLit(v, true ));
    assigns  .push(l_Undef);
    vardata  .push(mkVarData(CRef_Undef, 0));
    //activity .push(0);
    activity .push(rnd_init_act ? drand(random_seed) * 0.00001 : 0);
    seen     .push(0);
    polarity .push(sign);
    decision .push();
    trail    .capacity(v+1);
    setDecisionVar(v, dvar);

    partInfo.push(Range ());
    trail_part.push (Range ());

    return v;
}


bool Solver::addClause_(vec<Lit>& ps, Range part)
{
    assert(decisionLevel() == 0);
    assert (!log_proof || !part.undef ());
    
    if (!ok) return false;

    // Check if clause is satisfied and remove false/duplicate literals:
    sort(ps);
    Lit p; int i, j;
    if (log_proof)
      {
        // -- remove duplicates
        for (i = j = 0, p = lit_Undef; i < ps.size(); i++)
          if (value(ps[i]) == l_True || ps[i] == ~p)
            return true;
          else if (ps[i] != p)
            ps[j++] = p = ps[i];
        ps.shrink(i - j);

        // -- move false literals to the end
        int sz = ps.size ();
        for (i = 0; i < sz; ++i)
          {
            if (value (ps [i]) == l_False)
              {
                // -- push current literal to the end
                Lit l = ps[i];
                ps[i] = ps[sz-1];
                ps[sz-1] = l;
                sz--;
                i--;
              }
          }
      }
    else
      {
        // -- AG: original minisat code
        for (i = j = 0, p = lit_Undef; i < ps.size(); i++)
          if (value(ps[i]) == l_True || ps[i] == ~p)
            return true;
          else if (value(ps[i]) != l_False && ps[i] != p)
            ps[j++] = p = ps[i];
        ps.shrink(i - j);
      }

    if (ps.size() == 0)
        return ok = false;
    else if (log_proof && value (ps[0]) == l_False)
      {
        // -- this is the conflict clause, log it as the last clause in the proof
        CRef cr = ca.alloc (ps, false);
        Clause &c = ca[cr];
        c.part ().join (part);
        proof.push (cr);
        for (int i = 0; part.singleton () && i < ps.size (); ++i)
          partInfo [var (ps[i])].join (part);
        return ok = false;
      }
    else if (ps.size() == 1 || (log_proof && value (ps[1]) == l_False)){
      if (log_proof)
        {
          CRef cr = ca.alloc (ps, false);
          Clause &c = ca[cr];
          c.part ().join (part);
          clauses.push (cr);
          totalPart.join (part);
          uncheckedEnqueue (ps[0], cr);
        }
      else
        uncheckedEnqueue(ps[0]);

      // -- mark variables as shared if necessary
      for (int i = 0; part.singleton () && i < ps.size (); ++i)
        partInfo [var (ps[i])].join (part);
      return ok = (propagate() == CRef_Undef);
    }else{
        CRef cr = ca.alloc(ps, false);
        Clause& c = ca[cr];
        c.part ().join (part);
        clauses.push(cr);
        totalPart.join (part);
        attachClause(cr);

        for (i = 0; part.singleton () && i < ps.size(); i++)
          partInfo[var (ps[i])].join (part);
        
    }

    return true;
}


void Solver::attachClause(CRef cr) {
    const Clause& c = ca[cr];
    assert(c.size() > 1);
    watches[~c[0]].push(Watcher(cr, c[1]));
    watches[~c[1]].push(Watcher(cr, c[0]));
    if (c.learnt()) learnts_literals += c.size();
    else            clauses_literals += c.size(); }


void Solver::detachClause(CRef cr, bool strict) {
    const Clause& c = ca[cr];
    assert(c.size() > 1);

    if (strict){
        remove(watches[~c[0]], Watcher(cr, c[1]));
        remove(watches[~c[1]], Watcher(cr, c[0]));
    }else{
        // Lazy detaching: (NOTE! Must clean all watcher lists before garbage collecting this clause)
        watches.smudge(~c[0]);
        watches.smudge(~c[1]);
    }

    if (c.learnt()) learnts_literals -= c.size();
    else            clauses_literals -= c.size(); }


void Solver::removeClause(CRef cr) {
    if (log_proof) proof.push (cr);
    Clause& c = ca[cr];
    if (c.size () > 1) detachClause(cr);
    // Don't leave pointers to free'd memory!
    if (locked(c) && !log_proof) vardata[var(c[0])].reason = CRef_Undef;
    c.mark(1);
    if (!log_proof) ca.free(cr);
}


bool Solver::satisfied(const Clause& c) const {
    for (int i = 0; i < c.size(); i++)
        if (value(c[i]) == l_True)
            return true;
    return false; }


// Revert to the state at given level (keeping all assignment at 'level' but not beyond).
//
void Solver::cancelUntil(int level) {
    if (decisionLevel() > level){
        for (int c = trail.size()-1; c >= trail_lim[level]; c--){
            Var      x  = var(trail[c]);
            assigns [x] = l_Undef;
            if (phase_saving > 1 || (phase_saving == 1) && c > trail_lim.last())
                polarity[x] = sign(trail[c]);
            insertVarOrder(x); }
        qhead = trail_lim[level];
        trail.shrink(trail.size() - trail_lim[level]);
        trail_lim.shrink(trail_lim.size() - level);
    } }


//=================================================================================================
// Major methods:


Lit Solver::pickBranchLit()
{
    Var next = var_Undef;

    // Random decision:
    if (drand(random_seed) < random_var_freq && !order_heap.empty()){
        next = order_heap[irand(random_seed,order_heap.size())];
        if (value(next) == l_Undef && decision[next])
            rnd_decisions++; }

    // Activity based decision:
    while (next == var_Undef || value(next) != l_Undef || !decision[next])
        if (order_heap.empty()){
            next = var_Undef;
            break;
        }else
            next = order_heap.removeMin();

    return next == var_Undef ? lit_Undef : mkLit(next, rnd_pol ? drand(random_seed) < 0.5 : polarity[next]);
}


/*_________________________________________________________________________________________________
|
|  analyze : (confl : Clause*) (out_learnt : vec<Lit>&) (out_btlevel : int&)  ->  [void]
|
|  Description:
|    Analyze conflict and produce a reason clause.
|
|    Pre-conditions:
|      * 'out_learnt' is assumed to be cleared.
|      * Current decision level must be greater than root level.
|
|    Post-conditions:
|      * 'out_learnt[0]' is the asserting literal at level 'out_btlevel'.
|      * If out_learnt.size() > 1 then 'out_learnt[1]' has the greatest decision level of the
|        rest of literals. There may be others from the same level though.
|
|________________________________________________________________________________________________@*/
void Solver::analyze(CRef confl, vec<Lit>& out_learnt, int& out_btlevel,
                     Range &part)
{
    int pathC = 0;
    Lit p     = lit_Undef;

    // Generate conflict clause:
    //
    out_learnt.push();      // (leave room for the asserting literal)
    int index   = trail.size() - 1;

    if (log_proof) part = ca [confl].part ();

    do{
        assert(confl != CRef_Undef); // (otherwise should be UIP)
        Clause& c = ca[confl];

        if (log_proof) part.join (c.part ());

        if (c.learnt())
            claBumpActivity(c);

        for (int j = (p == lit_Undef) ? 0 : 1; j < c.size(); j++){
            Lit q = c[j];

            if (!seen[var(q)]){
              if (level(var(q)) > 0)
                {
                  varBumpActivity(var(q));
                  seen[var(q)] = 1;
                  if (level(var(q)) >= decisionLevel())
                    pathC++;
                  else
                    out_learnt.push(q);
                }
              else if (log_proof)
                {
                  assert (!trail_part[var (q)].undef ());
                  // update part based on partition of var(q)
                  part.join (trail_part [var (q)]);
                }
            }
        }

        // Select next clause to look at:
        while (!seen[var(trail[index--])]);
        p     = trail[index+1];
        confl = reason(var(p));
        seen[var(p)] = 0;
        pathC--;

    }while (pathC > 0);
    out_learnt[0] = ~p;

    // Simplify conflict clause:
    //
    int i, j;
    out_learnt.copyTo(analyze_toclear);
    if (ccmin_mode == 2){
        uint32_t abstract_level = 0;
        for (i = 1; i < out_learnt.size(); i++)
            abstract_level |= abstractLevel(var(out_learnt[i])); // (maintain an abstraction of levels involved in conflict)

        for (i = j = 1; i < out_learnt.size(); i++)
          if (reason(var(out_learnt[i])) == CRef_Undef || !litRedundant(out_learnt[i], abstract_level, part))
                out_learnt[j++] = out_learnt[i];

    }else if (ccmin_mode == 1){
      assert (!log_proof);
        for (i = j = 1; i < out_learnt.size(); i++){
            Var x = var(out_learnt[i]);

            if (reason(x) == CRef_Undef)
                out_learnt[j++] = out_learnt[i];
            else{
                Clause& c = ca[reason(var(out_learnt[i]))];
                for (int k = 1; k < c.size(); k++)
                    if (!seen[var(c[k])] && level(var(c[k])) > 0){
                        out_learnt[j++] = out_learnt[i];
                        break; }
            }
        }
    }else
        i = j = out_learnt.size();

    max_literals += out_learnt.size();
    out_learnt.shrink(i - j);
    tot_literals += out_learnt.size();

    // Find correct backtrack level:
    //
    if (out_learnt.size() == 1)
        out_btlevel = 0;
    else{
        int max_i = 1;
        // Find the first literal assigned at the next-highest level:
        for (int i = 2; i < out_learnt.size(); i++)
            if (level(var(out_learnt[i])) > level(var(out_learnt[max_i])))
                max_i = i;
        // Swap-in this literal at index 1:
        Lit p             = out_learnt[max_i];
        out_learnt[max_i] = out_learnt[1];
        out_learnt[1]     = p;
        out_btlevel       = level(var(p));
    }

    for (int j = 0; j < analyze_toclear.size(); j++) seen[var(analyze_toclear[j])] = 0;    // ('seen[]' is now cleared)
}


// Check if 'p' can be removed. 'abstract_levels' is used to abort early if the algorithm is
// visiting literals at levels that cannot be removed later.
bool Solver::litRedundant(Lit p, uint32_t abstract_levels, Range &part)
{
    analyze_stack.clear(); analyze_stack.push(p);
    // -- partition of all clauses used in the derivation to replace p
    Range lPart;
    int top = analyze_toclear.size();
    while (analyze_stack.size() > 0){
        assert(reason(var(analyze_stack.last())) != CRef_Undef);
        Clause& c = ca[reason(var(analyze_stack.last()))]; analyze_stack.pop();
        if (log_proof) lPart.join (c.part ());
        
        for (int i = 1; i < c.size(); i++){
            Lit p  = c[i];
            if (!seen[var(p)]){
              if (level (var (p)) > 0)
              {
                if (reason(var(p)) != CRef_Undef && (abstractLevel(var(p)) & abstract_levels) != 0){
                    seen[var(p)] = 1;
                    analyze_stack.push(p);
                    analyze_toclear.push(p);
                }else{
                    for (int j = top; j < analyze_toclear.size(); j++)
                        seen[var(analyze_toclear[j])] = 0;
                    analyze_toclear.shrink(analyze_toclear.size() - top);
                    return false;
                }
              }
              else if (log_proof)
              {
                assert (!trail_part[var (p)].undef ());
                // update part based on partition of var(q)
                lPart.join (trail_part [var (p)]);
              }
            }
        }
    }

    if (log_proof) part.join (lPart);

    return true;
}


/*_________________________________________________________________________________________________
|
|  analyzeFinal : (p : Lit)  ->  [void]
|
|  Description:
|    Specialized analysis procedure to express the final conflict in terms of assumptions.
|    Calculates the (possibly empty) set of assumptions that led to the assignment of 'p', and
|    stores the result in 'out_conflict'.
|________________________________________________________________________________________________@*/
void Solver::analyzeFinal(Lit p, vec<Lit>& out_conflict)
{
    out_conflict.clear();
    out_conflict.push(p);

    if (decisionLevel() == 0)
        return;

    seen[var(p)] = 1;

    for (int i = trail.size()-1; i >= trail_lim[0]; i--){
        Var x = var(trail[i]);
        if (seen[x]){
            if (reason(x) == CRef_Undef){
                assert(level(x) > 0);
                out_conflict.push(~trail[i]);
            }else{
                Clause& c = ca[reason(x)];
                for (int j = 1; j < c.size(); j++)
                    if (level(var(c[j])) > 0)
                        seen[var(c[j])] = 1;
            }
            seen[x] = 0;
        }
    }

    seen[var(p)] = 0;
}


void Solver::uncheckedEnqueue(Lit p, CRef from)
{
    assert(value(p) == l_Undef);
    assigns[var(p)] = lbool(!sign(p));
    vardata[var(p)] = mkVarData(from, decisionLevel());
    trail.push_(p);

    // -- everything at level 0 has a reason
    assert (!log_proof || decisionLevel () != 0 || from != CRef_Undef);

    if (log_proof && decisionLevel () == 0)
      {
        Clause &c = ca[from];
        Var x = var (p);

        assert (!c.part ().undef ());

        trail_part [x] = c.part ();
        for (int i = 1; i < c.size (); ++i)
          trail_part [x].join (ca [reason (var (c[i]))].part ());
      }

}


/*_________________________________________________________________________________________________
|
|  propagate : [void]  ->  [Clause*]
|
|  Description:
|    Propagates all enqueued facts. If a conflict arises, the conflicting clause is returned,
|    otherwise CRef_Undef.
|
|    Post-conditions:
|      * the propagation queue is empty, even if there was a conflict.
|________________________________________________________________________________________________@*/
CRef Solver::propagate(bool coreOnly)
{
    CRef    confl     = CRef_Undef;
    int     num_props = 0;
    watches.cleanAll();

    while (qhead < trail.size()){
        Lit            p   = trail[qhead++];     // 'p' is enqueued fact to propagate.
        vec<Watcher>&  ws  = watches[p];
        Watcher        *i, *j, *end;
        num_props++;

        //sort (ws, WatcherLt(ca));
        for (i = j = (Watcher*)ws, end = i + ws.size();  i != end;){
            // Try to avoid inspecting the clause:
            Lit blocker = i->blocker;
            if (value(blocker) == l_True){
                *j++ = *i++; continue; }

            // Make sure the false literal is data[1]:
            CRef     cr        = i->cref;
            Clause&  c         = ca[cr];

            if (coreOnly && !c.core ()) { *j++ = *i++; continue; }

            Lit      false_lit = ~p;
            if (c[0] == false_lit)
                c[0] = c[1], c[1] = false_lit;
            assert(c[1] == false_lit);
            i++;

            // If 0th watch is true, then clause is already satisfied.
            Lit     first = c[0];
            Watcher w     = Watcher(cr, first);
            if (first != blocker && value(first) == l_True){
                *j++ = w; continue; }

            // Look for new watch:
            for (int k = 2; k < c.size(); k++)
                if (value(c[k]) != l_False){
                    c[1] = c[k]; c[k] = false_lit;
                    watches[~c[1]].push(w);
                    goto NextClause; }

            // Did not find watch -- clause is unit under assignment:
            *j++ = w;
            if (value(first) == l_False){
                confl = cr;
                qhead = trail.size();
                // Copy the remaining watches:
                while (i < end)
                    *j++ = *i++;
            }else
                uncheckedEnqueue(first, cr);

        NextClause:;
        }
        ws.shrink(i - j);
    }
    propagations += num_props;
    simpDB_props -= num_props;

    return confl;
}


/*_________________________________________________________________________________________________
|
|  reduceDB : ()  ->  [void]
|
|  Description:
|    Remove half of the learnt clauses, minus the clauses locked by the current assignment. Locked
|    clauses are clauses that are reason to some assignment. Binary clauses are never removed.
|________________________________________________________________________________________________@*/
struct reduceDB_lt {
    ClauseAllocator& ca;
    reduceDB_lt(ClauseAllocator& ca_) : ca(ca_) {}
    bool operator () (CRef x, CRef y) {
        return ca[x].size() > 2 && (ca[y].size() == 2 || ca[x].activity() < ca[y].activity()); }
};
void Solver::reduceDB()
{
    int     i, j;
    double  extra_lim = cla_inc / learnts.size();    // Remove any clause below this activity

    sort(learnts, reduceDB_lt(ca));
    // Don't delete binary or locked clauses. From the rest, delete clauses from the first half
    // and clauses with activity smaller than 'extra_lim':
    for (i = j = 0; i < learnts.size(); i++){
        Clause& c = ca[learnts[i]];
        if (c.size() > 2 && !locked(c) && (i < learnts.size() / 2 || c.activity() < extra_lim))
            removeClause(learnts[i]);
        else
            learnts[j++] = learnts[i];
    }
    learnts.shrink(i - j);
    checkGarbage();
}


void Solver::removeSatisfied(vec<CRef>& cs)
{
    int i, j;
    for (i = j = 0; i < cs.size(); i++){
        Clause& c = ca[cs[i]];
        if (satisfied(c))
            removeClause(cs[i]);
        else
            cs[j++] = cs[i];
    }
    cs.shrink(i - j);
}


void Solver::rebuildOrderHeap()
{
    vec<Var> vs;
    for (Var v = 0; v < nVars(); v++)
        if (decision[v] && value(v) == l_Undef)
            vs.push(v);
    order_heap.build(vs);
}


/*_________________________________________________________________________________________________
|
|  simplify : [void]  ->  [bool]
|
|  Description:
|    Simplify the clause database according to the current top-level assigment. Currently, the only
|    thing done here is the removal of satisfied clauses, but more things can be put here.
|________________________________________________________________________________________________@*/
bool Solver::simplify()
{
    assert(decisionLevel() == 0);

    if (!ok || propagate() != CRef_Undef)
        return ok = false;

    if (nAssigns() == simpDB_assigns || (simpDB_props > 0))
        return true;

    // Remove satisfied clauses:
    removeSatisfied(learnts);
    if (remove_satisfied)        // Can be turned off.
        removeSatisfied(clauses);
    checkGarbage();
    rebuildOrderHeap();

    simpDB_assigns = nAssigns();
    simpDB_props   = clauses_literals + learnts_literals;   // (shouldn't depend on stats really, but it will do for now)

    return true;
}


/*_________________________________________________________________________________________________
|
|  search : (nof_conflicts : int) (params : const SearchParams&)  ->  [lbool]
|
|  Description:
|    Search for a model the specified number of conflicts.
|    NOTE! Use negative value for 'nof_conflicts' indicate infinity.
|
|  Output:
|    'l_True' if a partial assigment that is consistent with respect to the clauseset is found. If
|    all variables are decision variables, this means that the clause set is satisfiable. 'l_False'
|    if the clause set is unsatisfiable. 'l_Undef' if the bound on number of conflicts is reached.
|________________________________________________________________________________________________@*/
lbool Solver::search(int nof_conflicts)
{
    assert(ok);
    int         backtrack_level;
    int         conflictC = 0;
    vec<Lit>    learnt_clause;
    Range       part;
    starts++;

    for (;;){
        CRef confl = propagate();
        if (confl != CRef_Undef){
            // CONFLICT
            conflicts++; conflictC++;
            if (decisionLevel() == 0)
              {
                if (log_proof) proof.push (confl);
                return l_False;
              }

            learnt_clause.clear();
            analyze(confl, learnt_clause, backtrack_level, part);
            cancelUntil(backtrack_level);

            if (learnt_clause.size() == 1){
              if (log_proof)
                {
                  // Need to log learned unit clauses in the proof
                  CRef cr = ca.alloc (learnt_clause, true);
                  proof.push (cr);
                  ca[cr].part (part);
                  uncheckedEnqueue (learnt_clause [0], cr);
                }
              else
                uncheckedEnqueue(learnt_clause[0]);
            }else{
                CRef cr = ca.alloc(learnt_clause, true);
                if (log_proof) proof.push (cr);
                if (log_proof) ca[cr].part (part);
                learnts.push(cr);
                attachClause(cr);
                claBumpActivity(ca[cr]);
                uncheckedEnqueue(learnt_clause[0], cr);
            }

            varDecayActivity();
            claDecayActivity();

            if (--learntsize_adjust_cnt == 0){
                learntsize_adjust_confl *= learntsize_adjust_inc;
                learntsize_adjust_cnt    = (int)learntsize_adjust_confl;
                max_learnts             *= learntsize_inc;

                if (verbosity >= 1)
                    printf("| %9d | %7d %8d %8d | %8d %8d %6.0f | %6.3f %% |\n",
                           (int)conflicts,
                           (int)dec_vars - (trail_lim.size() == 0 ? trail.size() : trail_lim[0]), nClauses(), (int)clauses_literals,
                           (int)max_learnts, nLearnts(), (double)learnts_literals/nLearnts(), progressEstimate()*100);
            }

        }else{
            // NO CONFLICT
            if (nof_conflicts >= 0 && conflictC >= nof_conflicts || !withinBudget()){
                // Reached bound on number of conflicts:
                progress_estimate = progressEstimate();
                cancelUntil(0);
                return l_Undef; }

            // Simplify the set of problem clauses:
            if (decisionLevel() == 0 && !simplify())
                return l_False;

            if (learnts.size()-nAssigns() >= max_learnts)
                // Reduce the set of learnt clauses:
                reduceDB();

            Lit next = lit_Undef;
            while (decisionLevel() < assumptions.size()){
                // Perform user provided assumption:
                Lit p = assumptions[decisionLevel()];
                if (value(p) == l_True){
                    // Dummy decision level:
                    newDecisionLevel();
                }else if (value(p) == l_False){
                    analyzeFinal(~p, conflict);
                    return l_False;
                }else{
                    next = p;
                    break;
                }
            }

            if (next == lit_Undef){
                // New variable decision:
                decisions++;
                next = pickBranchLit();

                if (next == lit_Undef)
                    // Model found:
                    return l_True;
            }

            // Increase decision level and enqueue 'next'
            newDecisionLevel();
            uncheckedEnqueue(next);
        }
    }
}


double Solver::progressEstimate() const
{
    double  progress = 0;
    double  F = 1.0 / nVars();

    for (int i = 0; i <= decisionLevel(); i++){
        int beg = i == 0 ? 0 : trail_lim[i - 1];
        int end = i == decisionLevel() ? trail.size() : trail_lim[i];
        progress += pow(F, i) * (end - beg);
    }

    return progress / nVars();
}

/*
  Finite subsequences of the Luby-sequence:

  0: 1
  1: 1 1 2
  2: 1 1 2 1 1 2 4
  3: 1 1 2 1 1 2 4 1 1 2 1 1 2 4 8
  ...


 */

static double luby(double y, int x){

    // Find the finite subsequence that contains index 'x', and the
    // size of that subsequence:
    int size, seq;
    for (size = 1, seq = 0; size < x+1; seq++, size = 2*size+1);

    while (size-1 != x){
        size = (size-1)>>1;
        seq--;
        x = x % size;
    }

    return pow(y, seq);
}

// NOTE: assumptions passed in member-variable 'assumptions'.
lbool Solver::solve_()
{
    model.clear();
    conflict.clear();
    if (!ok) return l_False;

    solves++;

    max_learnts               = nClauses() * learntsize_factor;
    learntsize_adjust_confl   = learntsize_adjust_start_confl;
    learntsize_adjust_cnt     = (int)learntsize_adjust_confl;
    lbool   status            = l_Undef;

    if (verbosity >= 1){
        printf("============================[ Search Statistics ]==============================\n");
        printf("| Conflicts |          ORIGINAL         |          LEARNT          | Progress |\n");
        printf("|           |    Vars  Clauses Literals |    Limit  Clauses Lit/Cl |          |\n");
        printf("===============================================================================\n");
    }

    // Search:
    int curr_restarts = 0;
    while (status == l_Undef){
        double rest_base = luby_restart ? luby(restart_inc, curr_restarts) : pow(restart_inc, curr_restarts);
        status = search(rest_base * restart_first);
        if (!withinBudget()) break;
        curr_restarts++;
    }

    if (verbosity >= 1)
        printf("===============================================================================\n");


    if (status == l_True){
        // Extend & copy model:
        model.growTo(nVars());
        for (int i = 0; i < nVars(); i++) model[i] = value(i);
    }else if (status == l_False && conflict.size() == 0)
        ok = false;

    cancelUntil(0);
    return status;
}

//=================================================================================================
// Writing CNF to DIMACS:
//
// FIXME: this needs to be rewritten completely.

static Var mapVar(Var x, vec<Var>& map, Var& max)
{
    if (map.size() <= x || map[x] == -1){
        map.growTo(x+1, -1);
        map[x] = max++;
    }
    return map[x];
}


void Solver::toDimacs(FILE* f, Clause& c, vec<Var>& map, Var& max)
{
    if (satisfied(c)) return;

    for (int i = 0; i < c.size(); i++)
        if (value(c[i]) != l_False)
            fprintf(f, "%s%d ", sign(c[i]) ? "-" : "", mapVar(var(c[i]), map, max)+1);
    fprintf(f, "0\n");
}


void Solver::toDimacs(const char *file, const vec<Lit>& assumps)
{
    FILE* f = fopen(file, "wr");
    if (f == NULL)
        fprintf(stderr, "could not open file %s\n", file), exit(1);
    toDimacs(f, assumps);
    fclose(f);
}


void Solver::toDimacs(FILE* f, const vec<Lit>& assumps)
{
    // Handle case when solver is in contradictory state:
    if (!ok){
        fprintf(f, "p cnf 1 2\n1 0\n-1 0\n");
        return; }

    vec<Var> map; Var max = 0;

    // Cannot use removeClauses here because it is not safe
    // to deallocate them at this point. Could be improved.
    int cnt = 0;
    for (int i = 0; i < clauses.size(); i++)
        if (!satisfied(ca[clauses[i]]))
            cnt++;

    for (int i = 0; i < clauses.size(); i++)
        if (!satisfied(ca[clauses[i]])){
            Clause& c = ca[clauses[i]];
            for (int j = 0; j < c.size(); j++)
                if (value(c[j]) != l_False)
                    mapVar(var(c[j]), map, max);
        }

    // Assumptions are added as unit clauses:
    cnt += assumptions.size();

    fprintf(f, "p cnf %d %d\n", max, cnt);

    for (int i = 0; i < assumptions.size(); i++){
        assert(value(assumptions[i]) != l_False);
        fprintf(f, "%s%d 0\n", sign(assumptions[i]) ? "-" : "", mapVar(var(assumptions[i]), map, max)+1);
    }

    for (int i = 0; i < clauses.size(); i++)
        toDimacs(f, ca[clauses[i]], map, max);

    if (verbosity > 0)
        printf("Wrote %d clauses with %d variables.\n", cnt, max);
}


//=================================================================================================
// Garbage Collection methods:

void Solver::relocAll(ClauseAllocator& to)
{
    // All watchers:
    //
    // for (int i = 0; i < watches.size(); i++)
    watches.cleanAll();
    for (int v = 0; v < nVars(); v++)
        for (int s = 0; s < 2; s++){
            Lit p = mkLit(v, s);
            // printf(" >>> RELOCING: %s%d\n", sign(p)?"-":"", var(p)+1);
            vec<Watcher>& ws = watches[p];
            for (int j = 0; j < ws.size(); j++)
                ca.reloc(ws[j].cref, to);
        }

    // All reasons:
    //
    for (int i = 0; i < trail.size(); i++){
        Var v = var(trail[i]);

        if (reason(v) != CRef_Undef && (ca[reason(v)].reloced() || locked(ca[reason(v)])))
            ca.reloc(vardata[v].reason, to);
    }

    // All learnt:
    //
    for (int i = 0; i < learnts.size(); i++)
        ca.reloc(learnts[i], to);

    // All original:
    //
    for (int i = 0; i < clauses.size(); i++)
        ca.reloc(clauses[i], to);

    // Clausal proof:
    //
    for (int i = 0; i < proof.size (); i++)
      ca.reloc (proof[i], to);
}




void Solver::garbageCollect()
{
  assert (!log_proof);
    // Initialize the next region to a size corresponding to the estimated utilization degree. This
    // is not precise but should avoid some unnecessary reallocations for the new region:
    ClauseAllocator to(ca.size() - ca.wasted());

    relocAll(to);
    if (verbosity >= 2)
        printf("|  Garbage collection:   %12d bytes => %12d bytes             |\n",
               ca.size()*ClauseAllocator::Unit_Size, to.size()*ClauseAllocator::Unit_Size);
    to.moveTo(ca);
}
