cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 5.4.0 REQUIRED Core Network)

add_library (Util
	ClearCase.cpp
	fileUtils.cpp
	ServerCommon.cpp
	utils.cpp
	Input.cpp
	regex.cpp
	include/Util/regex.h
	include/Util/Input.h
	include/Util/algorithm.h
	include/Util/ClearCase.h
	include/Util/FileFormats.h
	include/Util/fileUtils.h
	include/Util/raise.h
	include/Util/ServerCommon.h
	include/Util/string_view.h
	include/Util/system.h
	include/Util/utils.h
	include/Util/version.h
)

target_include_directories (Util PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Util LINK_PUBLIC
	Regex
	GSL
	Qt5::Core
	Qt5::Network
)

set_property(TARGET Util PROPERTY CXX_STANDARD 14)
set_property(TARGET Util PROPERTY CXX_EXTENSIONS OFF)


