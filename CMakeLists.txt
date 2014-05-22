project(MiniSAT)
cmake_minimum_required(VERSION 2.8.6)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR ) 
  message (FATAL_ERROR
    "In-source builds are not allowed. Please clean your source tree and try again.")  
endif()

# # Add path for custom modules
# set(CMAKE_MODULE_PATH
#   ${CMAKE_MODULE_PATH}
#   "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
#   )



# check for rt lib. Not needed on OSX.
find_library(RT_LIB NAMES rt)
if (NOT RT_LIB)
  set(RT_LIB "")
endif()
mark_as_advanced(RT_LIB)

# prefer linking with static libraries
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ${CMAKE_FIND_LIBRARY_SUFFIXES})

include ("cmake/PackageOptions.cmake")

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}/include
  ${MINISAT_INCLUDE_DIR}
  )


add_subdirectory(utils)
add_subdirectory(core)
add_subdirectory(simp)
add_subdirectory(mtl)

add_library(minisat.LIB STATIC 
  $<TARGET_OBJECTS:minisat_utils>
  $<TARGET_OBJECTS:minisat_core>
  $<TARGET_OBJECTS:minisat_simp>)

set_target_properties(minisat.LIB 
  PROPERTIES OUTPUT_NAME "minisat")

add_executable (minisat simp/Main.cc)
target_link_libraries (minisat minisat.LIB z)

add_executable (minisat_core.BIN core/Main.cc)
set_target_properties (minisat_core.BIN 
  PROPERTIES OUTPUT_NAME "minisat_core")
target_link_libraries (minisat_core.BIN minisat.LIB z)


install (TARGETS minisat.LIB minisat minisat_core.BIN
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib)

enable_testing ()
macro(add_cnf_test name)
  add_test (NAME ${name}
    COMMAND minisat 
    ${CMAKE_CURRENT_SOURCE_DIR}/${name}.cnf 
    -tcpf=/dev/null )
  set_tests_properties (${name}
    PROPERTIES PASS_REGULAR_EXPRESSION "Replay SUCCESS")
endmacro(add_cnf_test)

add_cnf_test (6s120_f2)
add_cnf_test (6s102_f7)
add_cnf_test (6s102_f8)
add_cnf_test (6s102_f8_1)
add_cnf_test (6s102_f9)
add_cnf_test (6s10_f1)
add_cnf_test (6s10_f2)
add_cnf_test (6s10_f3)
add_cnf_test (6s10_f4)
add_cnf_test (6s10_f5)
add_cnf_test (6s10_f6)
add_cnf_test (6s10_f7)
add_cnf_test (intel024_f7)
add_cnf_test (intel024_f22)
