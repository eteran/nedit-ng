cmake_minimum_required(VERSION 3.0)

find_package(Qt5 5.5.0 REQUIRED Core Network)

add_executable(nc-ng
	nc.cpp
)

target_include_directories(nc-ng SYSTEM
	PRIVATE ${Boost_INCLUDE_DIR}
)

target_link_libraries(nc-ng LINK_PUBLIC
	Qt5::Core
	Qt5::Network
	Util
	Settings
)

set_property(TARGET nc-ng PROPERTY CXX_STANDARD 14)
set_property(TARGET nc-ng PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET nc-ng PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	set_property(TARGET nc-ng PROPERTY WIN32_EXECUTABLE ON)
endif()

install(TARGETS nc-ng DESTINATION bin)
