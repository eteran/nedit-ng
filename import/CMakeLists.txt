cmake_minimum_required(VERSION 3.15)

find_package(Qt5 5.5.0 REQUIRED Core Xml)
find_package(X11 REQUIRED)

if(${X11_FOUND})
    if(TARGET_PLATFORM_MACOS)
        link_directories(/usr/X11/lib)
        link_directories(/opt/X11/lib)
    endif()
    if(TARGET_PLATFORM_OPENBSD)
        link_directories(/usr/X11R6/lib)
    endif()
endif()

add_executable(nedit-import
	import.cpp
)

target_link_libraries(nedit-import PUBLIC
	Qt5::Core
	Qt5::Xml
	Settings
	Interpreter
	X11
)

target_include_directories(nedit-import PRIVATE
	${X11_INCLUDE_DIR}
)

set_target_properties(nedit-import
PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

install(TARGETS nedit-import DESTINATION bin)
