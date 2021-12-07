cmake_minimum_required(VERSION 3.0)
add_subdirectory(yaml-cpp)

set(GSL_CXX_STANDARD "${TARGET_COMPILER_HIGHEST_STD_SUPPORTED}" CACHE STRING "Use c++ standard")
set(GSL_CXX_STD "cxx_std_${GSL_CXX_STANDARD}")
add_subdirectory(GSL-3.1.0 EXCLUDE_FROM_ALL)
