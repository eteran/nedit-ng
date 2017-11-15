cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 5.4.0 REQUIRED Widgets Network)

add_library (NUtil 
	fileUtils.cpp 
	utils.cpp 
	ClearCase.cpp
	ServerCommon.cpp
)


target_include_directories (NUtil PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR} 
	${PROJECT_SOURCE_DIR}/GSL/include/
)

target_link_libraries(NUtil LINK_PUBLIC
	Qt5::Widgets
	Qt5::Network
)

set_property(TARGET NUtil PROPERTY CXX_STANDARD 14)
set_property(TARGET NUtil PROPERTY CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set_target_properties(NUtil 
		PROPERTIES 
		COMPILE_FLAGS "-W -Wall -pedantic -Wunused -Wzero-as-null-pointer-constant -ggdb3")
else()
	set_target_properties(NUtil
		PROPERTIES 
		COMPILE_FLAGS "-W -Wall -pedantic -Wunused -Wzero-as-null-pointer-constant")
endif()



