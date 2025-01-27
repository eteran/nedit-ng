cmake_minimum_required(VERSION 3.15)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets)

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

target_add_warnings(Settings)

target_link_libraries(Settings
PUBLIC
	Util
	Qt${QT_VERSION_MAJOR}::Core
	Qt${QT_VERSION_MAJOR}::Widgets
)
