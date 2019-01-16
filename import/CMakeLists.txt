cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
#set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTOUIC ON)
#set(CMAKE_AUTORCC ON)

find_package(Qt5 5.5.0 REQUIRED Core Xml)
find_package(X11 REQUIRED)

link_directories(${X11_LIBRARY_DIR})

add_executable(nedit-import
	import.cpp
)

target_link_libraries(nedit-import LINK_PUBLIC
	Qt5::Core
	Qt5::Xml
	Settings
	Interpreter
	X11
)

target_include_directories(nedit-import PRIVATE
	${X11_INCLUDE_DIR}
)

set_property(TARGET nedit-import PROPERTY CXX_STANDARD 14)
set_property(TARGET nedit-import PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET nedit-import PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

install(TARGETS nedit-import DESTINATION bin)
