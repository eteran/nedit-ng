cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 5.4.0 REQUIRED Core Network)

add_library (Util
	ClearCase.cpp
	fileUtils.cpp
	ServerCommon.cpp
	utils.cpp
	Input.cpp
	include/util/Input.h
	include/util/algorithm.h
	include/util/ClearCase.h
	include/util/FileFormats.h
	include/util/fileUtils.h
	include/util/raise.h
	include/util/ServerCommon.h
	include/util/string_view.h
	include/util/system.h
	include/util/utils.h
	include/util/version.h
)

target_include_directories (Util PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Util LINK_PUBLIC
	GSL
	Qt5::Core
	Qt5::Network
)

set_property(TARGET Util PROPERTY CXX_STANDARD 14)
set_property(TARGET Util PROPERTY CXX_EXTENSIONS OFF)


