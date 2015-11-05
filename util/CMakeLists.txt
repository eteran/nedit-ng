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

set_property(TARGET NUtil PROPERTY CXX_STANDARD 11)


