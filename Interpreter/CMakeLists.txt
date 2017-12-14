cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(BISON REQUIRED)
find_package(Qt5 5.4.0 REQUIRED Core)
find_package(Boost 1.35 REQUIRED)

# using a different name from parse.cpp
# because it will also output parse.h, which will conflict with the
# in source copy of that file!
bison_target(parser parse.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp)

add_library (Interpreter
	DataValue.h
	interpret.cpp
	interpret.h
	parse.h
	
	${BISON_parser_OUTPUT_SOURCE}
)

target_include_directories (Interpreter PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
	${Boost_INCLUDE_DIR} 
)

target_include_directories (Interpreter PRIVATE
	${PROJECT_SOURCE_DIR}/GSL/include/	
)

target_link_libraries(Interpreter LINK_PUBLIC
	Qt5::Core
	NUtil
)

set_property(TARGET Interpreter PROPERTY CXX_STANDARD 14)
set_property(TARGET Interpreter PROPERTY CXX_EXTENSIONS OFF)
