cmake_minimum_required(VERSION 3.15)
project(nedit-regex-test CXX)

add_executable(nedit-regex-test
	Test.cpp
)

target_link_libraries(nedit-regex-test
	Regex
)

if(NEDIT_INCLUDE_DECOMPILER)
	target_compile_definitions(nedit-regex-test PUBLIC -DNEDIT_INCLUDE_DECOMPILER)
endif()

add_test(
	NAME nedit-regex-test
	COMMAND $<TARGET_FILE:nedit-regex-test>
)
