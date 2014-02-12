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
    ProofVisitor()              : seqSize(1)    {}
    ProofVisitor(unsigned size) : seqSize(size), itpForVar(size), clauseToItp(size) {}

    virtual int visitResolvent      (CRef parent, Var resolvent, CRef p1, CRef p2) { return 0; }
    virtual int visitResolvent      (Var resolvent, Var p1, CRef p2)               { return 0; }
    virtual int visitHyperResolvent (Var parent)                                   { return 0; }
    virtual int visitHyperResolvent (CRef parent)                                  { return 0; }
    virtual int visitLeaf           (CRef cls, const vec<Lit>& lits)               { return 0; }
    virtual int visitLeaf           (Var v, CRef cls, const vec<Lit>& lits)                  { return 0; }

    // -- Utility
    virtual bool itpExists(CRef c) {return true;}

    unsigned        seqSize;        // -- ItpSeq size, always greater than 0.
    vec<Var>        hyperChildren;
    vec<CRef>       hyperClauses;
    vec<vec<int> >  itpForVar;      // -- Itp labeling on the trail
    vec<CMap<int> > clauseToItp;    // -- Clause to its Itp labeling
};

}

#endif /* PROOF_VISITOR_H_ */
