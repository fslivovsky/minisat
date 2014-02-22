/*
 * ProofVisitor.h
 *
 * Proof traversal functionality.
 *
 *  Created on: Jan 4, 2014
 *      Author: yakir
 */

#ifndef PROOF_VISITOR_H_
#define PROOF_VISITOR_H_

#include "SolverTypes.h"
#include "Solver.h"

namespace Minisat {

class ProofVisitor
{
public:
    ProofVisitor() {}

    virtual int visitResolvent      (Lit parent, Lit p1, CRef p2) { return 0; }
    virtual int visitHyperResolvent (Lit parent)                  { return 0; }
    virtual int visitHyperResolvent (CRef parent)                 { return 0; }

    vec<Lit>        hyperPivots;
    vec<CRef>       hyperClauses;
};

 class TraceProofVisitor : public ProofVisitor
 {
 protected:
   Solver &m_Solver;
   CMap<bool> m_visited;

   vec<bool> m_units;

   void doAntecendents ();
   
 public:
   TraceProofVisitor (Solver &solver) : m_Solver (solver) 
   {
     m_units.growTo (m_Solver.nVars (), false);
   }
   
   int visitResolvent (Lit parent, Lit p1, CRef p2);
   int visitHyperResolvent (Lit parent);
   int visitHyperResolvent (CRef parent);
 };
}

#endif /* G_PROOF_VISITOR_H_ */
