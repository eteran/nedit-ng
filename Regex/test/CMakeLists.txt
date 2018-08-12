cmake_minimum_required(VERSION 3.0)
project(RegexTest CXX)

add_executable(RegexTest
	Test.cpp
)

target_link_libraries(RegexTest
	Regex
)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

set_property(TARGET RegexTest PROPERTY CXX_STANDARD 14)

add_test("RegexTest" "RegexTest")
