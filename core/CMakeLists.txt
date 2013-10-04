add_library (minisat_core STATIC Solver.cc)
add_executable (minisat-nosimp Main.cc)
target_link_libraries (minisat-nosimp 
  minisat_core minisat_utils z)

install(TARGETS minisat_core 
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
install (FILES Solver.h SolverTypes.h
  DESTINATION include/minisat/core)