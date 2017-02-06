cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 5.4.0 REQUIRED Widgets Network)

add_library (NUtil 
	misc.cpp 
	fileUtils.cpp 
	utils.cpp 
	clearcase.cpp)


target_include_directories (NUtil PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR} 
)

target_link_libraries(NUtil LINK_PUBLIC
	Qt5::Widgets
	Qt5::Network
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -pedantic -Wunused") #-Wuseless-cast

set_property(TARGET NUtil PROPERTY CXX_STANDARD 14)


