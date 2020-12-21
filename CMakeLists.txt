cmake_minimum_required(VERSION 3.0)

project(nedit-ng 
	LANGUAGES CXX 
	VERSION 2020.1)

enable_testing()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")
include("AddWarnings")
include("DetectCompiler")
include("DetectOS")
include("EnableSanitizers")
include("EnableSTLDebug")
include("Profiling")
include("ProjectDefaults")

set(CMAKE_PROJECT_HOMEPAGE_URL "https://github.com/eteran/nedit-ng")
set(CMAKE_PROJECT_DESCRIPTION "A Qt port of the Nirvana Editor (NEdit)")

set(CPACK_PACKAGE_NAME "nedit-ng")
set(CPACK_PACKAGE_CONTACT "Evan Teran <evan.teran@gmail.com>")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${PROJECT_SOURCE_DIR}/README.md")
set(CPACK_PACKAGE_VENDOR "Evan Teran")

set(CPACK_RPM_PACKAGE_LICENSE "GPL-2")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Editors")

# build requirements...
# set(CPACK_RPM_PACKAGE_REQUIRES "qt5-qtbase-devel, qt5-linguist, gcc, gcc-c++, boost-devel, cmake")
set(CPACK_RPM_PACKAGE_REQUIRES "qt5-qtbase >= 5.6")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

set(CPACK_DEBIAN_PACKAGE_BUILD_DEPENDS "cmake (>= 3.0), qt5-default (>= 5.6), qtbase5-dev-tools (>= 5.6), qttools5-dev-tools (>= 5.6), qttools5-dev (>= 5.6), libboost-dev (>= 1.35), bison (>=3.0)")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt5core5a (>= 5.6), libqt5gui5 (>= 5.6), libqt5network5 (>= 5.6), libqt5printsupport5 (>= 5.6), libqt5widgets5 (>= 5.6), libqt5xml5 (>= 5.6)")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE ${CMAKE_PROJECT_HOMEPAGE_URL})
set(CPACK_DEBIAN_PACKAGE_SECTION "editors")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE.md")
set(CPACK_STRIP_FILES "bin/nc-ng")
set(CPACK_STRIP_FILES "bin/nedit-import")
set(CPACK_STRIP_FILES "bin/nedit-ng")
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
