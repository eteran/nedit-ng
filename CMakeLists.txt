cmake_minimum_required (VERSION 3.0)

project (nedit)

add_definitions(-DQT_NO_CAST_FROM_ASCII)
add_definitions(-DQT_NO_CAST_TO_ASCII)
#add_definitions(-DREPLACE_SCOPE)

add_subdirectory (util)
add_subdirectory (source)
add_subdirectory (client)

