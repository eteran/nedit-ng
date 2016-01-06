cmake_minimum_required(VERSION 3.0)

add_library (NUtil 
	DialogF.cpp 
	getfiles.cpp 
	printUtils.cpp 
	misc.cpp 
	fileUtils.cpp 
	prefFile.cpp 
	fontsel.cpp 
	managedList.cpp 
	utils.cpp 
	clearcase.cpp)

target_include_directories (NUtil PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_C_FLAGS "-Wc++-compat -Wwrite-strings")
set(CMAKE_CXX_FLAGS "-W -Wall -pedantic") #-Wuseless-cast

set_property(TARGET NUtil PROPERTY CXX_STANDARD 11)


