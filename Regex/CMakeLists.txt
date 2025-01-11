cmake_minimum_required(VERSION 3.15)

option(NEDIT_BUILD_TESTS "Build Tests")
option(NEDIT_INCLUDE_DECOMPILER "Build experimental regex decompiler code.")

set(SOURCES
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

if(NEDIT_INCLUDE_DECOMPILER)
	set(SOURCES
		${SOURCES}
		Decompile.cpp
		Decompile.h
	)
endif()

add_library(Regex
	${SOURCES}
)

target_include_directories(Regex PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Regex PUBLIC
	Util
)

target_add_warnings(Regex)

set_target_properties(Regex
PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)

if(NEDIT_BUILD_TESTS)
	if(NOT MSVC)
		add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/test")
	endif()
endif()
