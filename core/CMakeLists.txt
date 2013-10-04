add_library (minisat_core OBJECT Solver.cc)

install (FILES Solver.h SolverTypes.h
  DESTINATION include/minisat/core)