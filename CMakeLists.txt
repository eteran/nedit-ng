cmake_minimum_required(VERSION 3.0)

project(nedit CXX)

enable_testing()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")
include("AddWarnings")
include("DetectCompiler")
include("DetectOS")
include("EnableSanitizers")
include("EnableSTLDebug")
include("Profiling")
include("ProjectDefaults")

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

# extract nedit-ng version info if source directory is a git repo
message(STATUS "Extracting nedit-ng version info with git...")
find_package(Git)
if(Git_FOUND)
	execute_process(COMMAND ${GIT_EXECUTABLE} describe --dirty --always --tags
					WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
					OUTPUT_VARIABLE GIT_OUTPUT
					ERROR_VARIABLE GIT_ERROR)

	if(GIT_OUTPUT)
		string(STRIP ${GIT_OUTPUT} NEDIT_VERSION_GIT)
		message(STATUS "#define NEDIT_VERSION_GIT \"${NEDIT_VERSION_GIT}\"")
		add_definitions(-DNEDIT_VERSION_GIT=\"${NEDIT_VERSION_GIT}\")
	elseif(GIT_ERROR)
		string(STRIP ${GIT_ERROR} GIT_ERROR)
		message(STATUS "Git error: ${GIT_ERROR}")
	else()
		message(STATUS "Unable to extract nedit-ng version info from git")
	endif()
else()
	message(STATUS "Git is not installed")
endif()

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
