cmake_minimum_required(VERSION 3.0)

find_package(Qt5 5.5.0 REQUIRED Core Network)

if(WIN32)
	set(APP_ICON_RESOURCE_WINDOWS "../src/res/nedit-ng.rc")
endif()

add_executable(nc-ng
	nc.cpp
	${APP_ICON_RESOURCE_WINDOWS}
)

target_link_libraries(nc-ng PUBLIC
	Qt5::Core
	Qt5::Network
	Util
	Settings
)

target_add_warnings(nc-ng)

set_property(TARGET nc-ng PROPERTY CXX_STANDARD ${TARGET_COMPILER_HIGHEST_STD_SUPPORTED})
set_property(TARGET nc-ng PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET nc-ng PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if(TARGET_COMPILER_MSVC)
	set_property(TARGET nc-ng PROPERTY WIN32_EXECUTABLE ON)
endif()

install(TARGETS nc-ng DESTINATION bin)
