cmake_minimum_required(VERSION 3.10)

project(ref_counter)

set(CMAKE_CONFIGURATION_TYPES Release Debug)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CHECK_IPO TRUE)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_COMPILE_WARNING_AS_ERROR ON)

if(CHECK_IPO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT USE_IPO)
  if(USE_IPO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
  endif()
endif()

add_compile_definitions(CATCH_AMALGAMATED_CUSTOM_MAIN)

set(ref_counter_include
  ${CMAKE_SOURCE_DIR}/ref_counter.h
)
set(ref_counter_source
  ${CMAKE_SOURCE_DIR}/ref_weak_test.cpp
  ${CMAKE_SOURCE_DIR}/ref_counter_test.h
  ${CMAKE_SOURCE_DIR}/ref_counter_test.cpp
  ${CMAKE_SOURCE_DIR}/main.cpp
  ${CMAKE_SOURCE_DIR}/catch_amalgamated.cpp
  ${CMAKE_SOURCE_DIR}/catch_amalgamated.hpp
)

include_directories(
  ${CMAKE_SOURCE_DIR}
)

add_executable(ref_counter ${ref_counter_include} ${ref_counter_source})

if (MSVC)
  target_compile_options(ref_counter PRIVATE "/MP")
  target_compile_options(ref_counter PRIVATE "/Zi")
  target_compile_options(ref_counter PRIVATE "$<$<CONFIG:Release>:/Oi>")
  target_compile_options(ref_counter PRIVATE "$<$<CONFIG:Release>:/Gy>")
  target_compile_options(ref_counter PRIVATE /guard:cf)
  target_compile_options(ref_counter PRIVATE /permissive-)
  target_link_options(ref_counter PRIVATE "$<$<CONFIG:Release>:/Debug>")
  target_link_options(ref_counter PRIVATE "$<$<CONFIG:Release>:/OPT:ICF>")
  target_link_options(ref_counter PRIVATE "$<$<CONFIG:Release>:/OPT:REF>")
  target_link_options(ref_counter PRIVATE "$<$<CONFIG:Release>:/LTCG>")
  target_link_options(ref_counter PRIVATE /guard:cf)
endif()

