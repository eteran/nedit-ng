cmake_minimum_required(VERSION 3.0)

project(nedit-ng CXX)

enable_testing()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")
include("AddWarnings")
include("DetectCompiler")
include("DetectOS")
include("EnableSanitizers")
include("EnableSTLDebug")
include("Profiling")
include("ProjectDefaults")

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Qt port of the Nirvana Editor (NEdit)")
set(CPACK_PACKAGE_CONTACT "evan.teran@gmail.com")
set(CPACK_PACKAGE_VENDOR "Evan Teran")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.md")
set(CPACK_PACKAGE_VERSION_MAJOR "2020")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://github.com/eteran/nedit-ng")
set(CPACK_DEBIAN_PACKAGE_SECTION "editors")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5core5a (>= 5.6), libqt5gui5 (>= 5.6), libqt5network5 (>= 5.6), libqt5printsupport5 (>= 5.6), libqt5widgets5 (>= 5.6), libqt5xml5 (>= 5.6)")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}_${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}")
include(CPack)


find_package(Boost 1.35 REQUIRED)

set(NEDIT_PURIFY            OFF CACHE BOOL "Fill Unused TextBuffer space")
set(NEDIT_PER_TAB_CLOSE     ON  CACHE BOOL "Per Tab Close Buttons")
set(NEDIT_VISUAL_CTRL_CHARS ON  CACHE BOOL "Visualize ASCII Control Characters")

if(NEDIT_PURIFY)
	add_definitions(-DPURIFY)
endif()

if(NEDIT_VISUAL_CTRL_CHARS)
	add_definitions(-DVISUAL_CTRL_CHARS)
endif()

if(NEDIT_PER_TAB_CLOSE)
	add_definitions(-DPER_TAB_CLOSE)
endif()

add_definitions(-DQT_NO_CAST_FROM_ASCII)
add_definitions(-DQT_NO_CAST_TO_ASCII)
add_definitions(-DQT_NO_KEYWORDS)

add_subdirectory(Util)
add_subdirectory(src)
add_subdirectory(libs)
add_subdirectory(client)
add_subdirectory(Regex)
add_subdirectory(Settings)
add_subdirectory(Interpreter)

if(UNIX)
    find_package(X11)
    if(${X11_FOUND})
        add_subdirectory(import)
    endif()
endif()
