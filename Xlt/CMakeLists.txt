cmake_minimum_required(VERSION 3.0)
add_library (Xlt BubbleButton.cpp SlideC.cpp)
target_include_directories (Xlt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set_property(TARGET Xlt    PROPERTY CXX_STANDARD 11)
