add_library (minisat_core OBJECT Solver.cc TraceProofVisitor.cc)

install (FILES Solver.h SolverTypes.h ProofVisitor.h TraceProofVisitor.h
  DESTINATION include/minisat/core)
