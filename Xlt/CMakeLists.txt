cmake_minimum_required(VERSION 3.0)
add_library (Xlt BubbleButton.c SlideC.c)
target_include_directories (Xlt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
