add_library (minisat_utils OBJECT  Options.cc System.cc)

install (FILES Options.h ParseUtils.h System.h 
  DESTINATION include/minisat/utils)