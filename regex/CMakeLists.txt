cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_library (RegExp 
	regularExp.cpp
)


target_include_directories (RegExp PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR} 
	${PROJECT_SOURCE_DIR}
)

target_link_libraries(RegExp LINK_PUBLIC
)

set_property(TARGET RegExp PROPERTY CXX_STANDARD 14)
set_property(TARGET RegExp PROPERTY CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set_target_properties(RegExp 
		PROPERTIES 
		COMPILE_FLAGS "-W -Wall -pedantic -Wunused -Wzero-as-null-pointer-constant -ggdb3")
else()
	set_target_properties(RegExp
		PROPERTIES 
		COMPILE_FLAGS "-W -Wall -pedantic -Wunused -Wzero-as-null-pointer-constant")
endif()




