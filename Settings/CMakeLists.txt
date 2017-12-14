cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Qt5 5.4.0 REQUIRED Widgets)

qt5_wrap_cpp(MOC_SOURCES
	Settings.h
)

add_library (Settings 
	${MOC_SOURCES}
	IndentStyle.h
	SearchType.h
	Settings.cpp
	Settings.h
	ShowMatchingStyle.h
	TruncSubstitution.h
	VirtKeyOverride.h
	WrapStyle.h
	ReplaceAllDefaultScope.h
)

target_include_directories (Settings PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories (Settings PRIVATE
	${PROJECT_SOURCE_DIR}
)

target_link_libraries(Settings LINK_PUBLIC
	Qt5::Widgets
	Util
)

set_property(TARGET Settings PROPERTY CXX_STANDARD 14)
set_property(TARGET Settings PROPERTY CXX_EXTENSIONS OFF)


