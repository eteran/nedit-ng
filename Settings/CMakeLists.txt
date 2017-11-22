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
	${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/GSL/include/
)

target_link_libraries(Settings LINK_PUBLIC
	Qt5::Widgets
	NUtil
)

set_property(TARGET Settings PROPERTY CXX_STANDARD 14)
set_property(TARGET Settings PROPERTY CXX_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	set_target_properties(Settings 
		PROPERTIES 
		COMPILE_FLAGS "-W -Wall -pedantic -Wunused -Wzero-as-null-pointer-constant -ggdb3")
else()
	set_target_properties(Settings
		PROPERTIES 
		COMPILE_FLAGS "-W -Wall -pedantic -Wunused -Wzero-as-null-pointer-constant")
endif()




