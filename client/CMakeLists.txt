cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.5.0 REQUIRED Core Network)
find_package(Boost 1.35 REQUIRED)

add_executable(nc-ng
	nc.cpp
)

target_include_directories (nc-ng PRIVATE
	${Boost_INCLUDE_DIR}
)

target_link_libraries(nc-ng LINK_PUBLIC
	Qt5::Core
	Qt5::Network
	Util
	Settings
)

set_property(TARGET nc-ng PROPERTY CXX_STANDARD 14)
set_property(TARGET nc-ng PROPERTY CXX_EXTENSIONS OFF)

set_target_properties(nc-ng
	PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

install (TARGETS nc-ng DESTINATION bin)
