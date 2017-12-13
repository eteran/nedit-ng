cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 5.4.0 REQUIRED Core Network)

add_library (NUtil 
	ClearCase.cpp
	ClearCase.h
	FileFormats.h
	ServerCommon.cpp
	ServerCommon.h
	algorithm.h
	fileUtils.cpp
	fileUtils.h
	raise.h
	string_view.h
	system.h
	utils.cpp
	utils.h
)

target_include_directories (NUtil PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR} 
)

target_include_directories (NUtil PRIVATE
	${PROJECT_SOURCE_DIR}/GSL/include/
)

target_link_libraries(NUtil LINK_PUBLIC
	Qt5::Core
	Qt5::Network
)

set_property(TARGET NUtil PROPERTY CXX_STANDARD 14)
set_property(TARGET NUtil PROPERTY CXX_EXTENSIONS OFF)


