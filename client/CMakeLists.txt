cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.4.0 REQUIRED Core Network)

include_directories(nc-ng
	${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/source
)

add_executable(nc-ng
	nc.cpp
)

target_link_libraries(nc-ng LINK_PUBLIC
	Qt5::Core
	Qt5::Network
	NUtil
	Settings
)

set_property(TARGET nc-ng PROPERTY CXX_STANDARD 14)
set_property(TARGET nc-ng PROPERTY CXX_EXTENSIONS OFF)

set_target_properties(nc-ng
	PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

install (TARGETS nc-ng RUNTIME DESTINATION bin)