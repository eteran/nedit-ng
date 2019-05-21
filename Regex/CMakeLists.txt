cmake_minimum_required(VERSION 3.0)

option(NEDIT_BUILD_TESTS "Build Tests")

add_library(Regex 
	Common.h
	Constants.h
	Execute.cpp
	Execute.h
	Opcodes.h
	Compile.cpp
	Compile.h
	Regex.cpp
	Regex.h
	RegexError.cpp
	RegexError.h
	Substitute.cpp
	Substitute.h
)

target_include_directories(Regex PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Regex PUBLIC
	Util
)

set_property(TARGET Regex PROPERTY CXX_STANDARD 14)
set_property(TARGET Regex PROPERTY CXX_EXTENSIONS OFF)

if(NEDIT_BUILD_TESTS)
	if(NOT MSVC)
		add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/test")
	endif()
endif()
