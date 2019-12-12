cmake_minimum_required(VERSION 3.0)

find_package(Qt5 5.5.0 REQUIRED Core Network)

add_library(Util
	ClearCase.cpp
	FileSystem.cpp
	Host.cpp
	Input.cpp
	regex.cpp
	Resource.cpp
	ServerCommon.cpp
	String.cpp
	System.cpp
	User.cpp
	include/Util/algorithm.h
	include/Util/ClearCase.h
	include/Util/FileFormats.h
	include/Util/FileSystem.h
	include/Util/Host.h
	include/Util/Input.h
	include/Util/Raise.h
	include/Util/regex.h
	include/Util/Resource.h
	include/Util/ServerCommon.h
	include/Util/String.h
	include/Util/string_view.h
	include/Util/System.h
	include/Util/User.h
	include/Util/utils.h
	include/Util/version.h
	include/Util/QtHelper.h
	include/Util/Compiler.h
)

target_include_directories(Util 
	PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(Util
PUBLIC
	Regex
	GSL
	Qt5::Core
	Qt5::Network
	Boost::boost
)

target_add_warnings(Util)

set_property(TARGET Util PROPERTY CXX_STANDARD 14)
set_property(TARGET Util PROPERTY CXX_EXTENSIONS OFF)


