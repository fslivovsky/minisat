add_library (minisat_simp STATIC SimpSolver.cc)
add_executable (minisat Main.cc)
target_link_libraries (minisat 
  minisat_simp minisat_core minisat_utils z)

install(TARGETS minisat_simp
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
install (FILES SimpSolver.h 
  DESTINATION include/minisat/simp)