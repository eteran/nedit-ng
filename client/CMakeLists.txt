cmake_minimum_required(VERSION 3.15)

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

set_target_properties(nc-ng
PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if(TARGET_COMPILER_MSVC)
	set_property(TARGET nc-ng PROPERTY WIN32_EXECUTABLE ON)
endif()

install(TARGETS nc-ng DESTINATION bin)
