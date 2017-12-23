cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.4.0 REQUIRED Core)

add_executable(nedit-import
	import.cpp
)

target_link_libraries(nedit-import LINK_PUBLIC
	Qt5::Core
	Settings
	-lX11
)

set_property(TARGET nedit-import PROPERTY CXX_STANDARD 14)
set_property(TARGET nedit-import PROPERTY CXX_EXTENSIONS OFF)

set_target_properties(nedit-import
	PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

install (TARGETS nedit-import DESTINATION bin)
