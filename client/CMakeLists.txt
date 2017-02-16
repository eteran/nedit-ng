cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.4.0 REQUIRED Widgets DBus)

include_directories(nc
	${PROJECT_SOURCE_DIR}
	${PROJECT_SOURCE_DIR}/source
)

add_executable(nc
	nc.cpp
	${PROJECT_SOURCE_DIR}/source/Settings.cpp
)

target_link_libraries(nc LINK_PUBLIC
	Qt5::DBus
	Qt5::Widgets
	NUtil
)

set_property(TARGET nc PROPERTY C_STANDARD 99)
set_property(TARGET nc PROPERTY CXX_STANDARD 14)

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wc++-compat -Wwrite-strings -Wunused") # -ffunction-sections -fdata-sections
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -pedantic -Wunused") #-Wuseless-cast -Wold-style-cast -ffunction-sections -fdata-sections

set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -ggdb3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb3")

set_target_properties(nc
	PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
