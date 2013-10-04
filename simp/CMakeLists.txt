add_library (minisat_simp OBJECT SimpSolver.cc)

install (FILES SimpSolver.h   DESTINATION include/minisat/simp)