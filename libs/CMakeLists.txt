cmake_minimum_required(VERSION 3.15)
add_subdirectory(yaml-cpp-0.8.0)

set(GSL_CXX_STANDARD "17")
set(GSL_CXX_STD "cxx_std_17")
add_subdirectory(GSL-3.1.0 EXCLUDE_FROM_ALL)
