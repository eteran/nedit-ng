cmake_minimum_required(VERSION 3.15)

find_package(BISON QUIET)
find_package(Qt5 5.5.0 REQUIRED Core)

# using a different name from parse.cpp
# because it will also output parse.h, which will conflict with the
# in source copy of that file!
if(${BISON_FOUND})
    bison_target(parser parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp)
else()
    set(BISON_parser_OUTPUT_SOURCE
        prebuilt/parser.cpp
        prebuilt/parser.hpp
    )
endif()

add_library(Interpreter
	DataValue.h
	interpret.cpp
	interpret.h
	parse.h
	parse.cpp

	${BISON_parser_OUTPUT_SOURCE}
)

target_include_directories(Interpreter
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Interpreter
PUBLIC
	Qt5::Core
	Util
	GSL
)

target_add_warnings(Interpreter)

set_target_properties(Interpreter
PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
)
