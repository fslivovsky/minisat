add_library (minisat_utils STATIC  Options.cc System.cc)

install(TARGETS minisat_utils 
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
install (FILES Options.h ParseUtils.h System.h 
  DESTINATION include/minisat/utils)