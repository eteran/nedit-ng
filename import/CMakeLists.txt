cmake_minimum_required(VERSION 3.0)

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

target_add_warnings(nedit-import)

set_property(TARGET nedit-import PROPERTY CXX_STANDARD ${TARGET_COMPILER_HIGHEST_STD_SUPPORTED})
set_property(TARGET nedit-import PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET nedit-import PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

install(TARGETS nedit-import DESTINATION bin)
