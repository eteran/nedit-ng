cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library (Regex 
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

target_include_directories (Regex PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR} 
)

target_include_directories (Regex PRIVATE
	${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/GSL/include/
)

set_property(TARGET Regex PROPERTY CXX_STANDARD 14)
set_property(TARGET Regex PROPERTY CXX_EXTENSIONS OFF)


