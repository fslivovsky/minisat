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
    ProofVisitor() {}

    virtual int visitResolvent      (Var parent, Var p1, CRef p2) { return 0; }
    virtual int visitHyperResolvent (Var parent)                  { return 0; }
    virtual int visitHyperResolvent (CRef parent)                 { return 0; }

    vec<Var>        hyperChildren;
    vec<CRef>       hyperClauses;
};

}

#endif /* G_PROOF_VISITOR_H_ */
