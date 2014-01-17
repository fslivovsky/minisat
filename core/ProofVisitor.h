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

namespace Minisat {

class ProofVisitor
{
public:
    virtual int visitResolvent (CRef parent, Var resolvent, CRef p1, CRef p2) { return 0; }
    virtual int visitResolvent (Var resolvent, Var p1, CRef p2)               { return 0; }
    virtual int visitHyperResolvent (Var parent)                              { return 0; }
    virtual int visitHyperResolvent (CRef parent)                             { return 0; }
    virtual int visitLeaf      (CRef cls, const vec<Lit>& lits)               { return 0; }
    virtual int visitLeaf      (Var v, const vec<Lit>& lits)                  { return 0; }

    // -- Utility
    bool itpExists(CRef c)             { return clauseToItp.has(c); }
    void setVarItp(Var x, int itp)     { if (itpForVar.size() <= toInt(x)) itpForVar.growTo(toInt(x)+1); itpForVar[x] = itp; }

    vec<Var>       hyperChildren;
    vec<CRef>      hyperClauses;
    vec<int>       itpForVar;   // -- Itp labeling on the trail
    Map<CRef, int> clauseToItp; // -- Clause to its Itp labeling
};

}

#endif /* PROOF_VISITOR_H_ */
