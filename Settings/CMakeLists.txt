cmake_minimum_required(VERSION 3.0)

find_package(Qt5 5.5.0 REQUIRED Core Widgets)

add_library(Settings 
	IndentStyle.h
	SearchType.h
	Settings.cpp
	Settings.h
	ShowMatchingStyle.h
	TruncSubstitution.h
	WrapStyle.h
)

target_include_directories(Settings PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Settings
PUBLIC
	Qt5::Core
	Qt5::Widgets
	Util
)

target_add_warnings(Settings)

set_property(TARGET Settings PROPERTY CXX_STANDARD ${TARGET_COMPILER_HIGHEST_STD_SUPPORTED})
set_property(TARGET Settings PROPERTY CXX_EXTENSIONS OFF)


