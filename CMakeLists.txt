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

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}/includ
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

add_subdirectory(utils)
add_subdirectory(core)
add_subdirectory(simp)
add_subdirectory(mtl)

add_library(minisat STATIC 
  $<TARGET_OBJECTS:minisat_utils>
  $<TARGET_OBJECTS:minisat_core>
  $<TARGET_OBJECTS:minisat_simp>)

install (TARGETS minisat ARCHIVE DESTINATION lib)
