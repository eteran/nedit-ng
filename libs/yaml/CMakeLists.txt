cmake_minimum_required(VERSION 3.0)

add_library(mini-yaml 
	Yaml.cpp
	include/mini-yaml/Yaml.hpp
)
	

target_include_directories(mini-yaml
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(mini-yaml
PUBLIC
    -W
	-Wall
	#-Wshadow
	-Wnon-virtual-dtor
	-Wold-style-cast
	-Wcast-align
	-Wunused
	-Woverloaded-virtual
	-pedantic
	-Wconversion
	-Wsign-conversion
	-Wnull-dereference
	-Wdouble-promotion
	-Wformat=2
	-Wno-unused-macros
	-Wno-switch-enum
	-Wno-unknown-pragmas
	-Wuseless-cast
	-Wduplicated-cond
	-Wduplicated-branches
	-Wlogical-op
	-Wsuggest-final-types
	-Wsuggest-final-methods
	-Wsuggest-override
)

set_property(TARGET mini-yaml PROPERTY CXX_STANDARD 14)
set_property(TARGET mini-yaml PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET mini-yaml PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
