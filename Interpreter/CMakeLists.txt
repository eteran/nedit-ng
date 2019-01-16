cmake_minimum_required(VERSION 3.0)

find_package(BISON)
find_package(Qt5 5.5.0 REQUIRED Core)
find_package(Boost 1.35 REQUIRED)

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

target_include_directories(Interpreter SYSTEM
	PRIVATE ${Boost_INCLUDE_DIR}
)

target_link_libraries(Interpreter LINK_PUBLIC
	Qt5::Core
	Util
	GSL
)

set_property(TARGET Interpreter PROPERTY CXX_STANDARD 14)
set_property(TARGET Interpreter PROPERTY CXX_EXTENSIONS OFF)
