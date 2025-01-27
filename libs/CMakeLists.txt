cmake_minimum_required(VERSION 3.15)
add_subdirectory(yaml-cpp-0.8.0)

set(GSL_CXX_STANDARD "17" CACHE STRING "Use c++ standard")
add_subdirectory(GSL-3.1.0 EXCLUDE_FROM_ALL)
