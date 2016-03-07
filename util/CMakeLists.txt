cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt4 4.6.0 REQUIRED QtCore QtGui QtNetwork)

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

include(${QT_USE_FILE})

message(${QT_USE_FILE})

target_include_directories (NUtil PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR} 
)

target_link_libraries(NUtil LINK_PUBLIC
	Qt4::QtCore
	Qt4::QtNetwork
)

set(CMAKE_CXX_FLAGS "-W -Wall -pedantic") #-Wuseless-cast

set_property(TARGET NUtil PROPERTY CXX_STANDARD 11)


