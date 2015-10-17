cmake_minimum_required(VERSION 3.0)
add_library (NUtil DialogF.c getfiles.c printUtils.c misc.c fileUtils.c prefFile.c fontsel.c managedList.c utils.c clearcase.c motif.c)
target_include_directories (NUtil PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
