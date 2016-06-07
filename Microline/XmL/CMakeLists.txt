cmake_minimum_required(VERSION 3.0)
add_library (XmL Folder.c XmL.c)
target_include_directories (XmL PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} -Wc++-compat) 
