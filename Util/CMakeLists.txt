cmake_minimum_required(VERSION 3.0)

find_package(Qt5 5.5.0 REQUIRED Core Network)

# extract nedit-ng version info if source directory is a git repo
message(STATUS "Extracting nedit-ng version info with git...")
find_package(Git)
if(Git_FOUND)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --dirty --always --tags
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_OUTPUT
		ERROR_VARIABLE GIT_ERROR
		OUTPUT_STRIP_TRAILING_WHITESPACE
		)

	if(GIT_OUTPUT)
		string(STRIP ${GIT_OUTPUT} NEDIT_VERSION_GIT)
		message(STATUS "NEDIT_VERSION_GIT = \"${NEDIT_VERSION_GIT}\"")
	elseif(GIT_ERROR)
		string(STRIP ${GIT_ERROR} GIT_ERROR)
		message(STATUS "Git error: ${GIT_ERROR}")
	else()
		message(STATUS "Unable to extract nedit-ng version info from git")
	endif()

	execute_process(
		COMMAND git rev-parse HEAD
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_OUTPUT
		ERROR_VARIABLE GIT_ERROR
		OUTPUT_STRIP_TRAILING_WHITESPACE
		)

	if(GIT_OUTPUT)
		string(STRIP ${GIT_OUTPUT} NEDIT_COMMIT_GIT)
		message(STATUS "NEDIT_COMMIT_GIT = \"${NEDIT_COMMIT_GIT}\"")
	elseif(GIT_ERROR)
		string(STRIP ${GIT_ERROR} GIT_ERROR)
		message(STATUS "Git error: ${GIT_ERROR}")
	else()
		message(STATUS "Unable to extract nedit-ng commit id from git")
	endif()


        execute_process(
                COMMAND git rev-parse --abbrev-ref HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_OUTPUT
                ERROR_VARIABLE GIT_ERROR
                OUTPUT_STRIP_TRAILING_WHITESPACE
                )

        if(GIT_OUTPUT)
                string(STRIP ${GIT_OUTPUT} NEDIT_BRANCH_GIT)
                message(STATUS "NEDIT_BRANCH_GIT = \"${NEDIT_BRANCH_GIT}\"")
        elseif(GIT_ERROR)
                string(STRIP ${GIT_ERROR} GIT_ERROR)
                message(STATUS "Git error: ${GIT_ERROR}")
        else()
                message(STATUS "Unable to extract nedit-ng branch name from git")
        endif()


else()
	message(STATUS "Git is not installed")
endif()

configure_file(include/Util/version.h.in include/Util/version.h)

add_library(Util
	ClearCase.cpp
	FileSystem.cpp
	Host.cpp
	Input.cpp
	regex.cpp
	Resource.cpp
	ServerCommon.cpp
	String.cpp
	System.cpp
	User.cpp
	include/Util/algorithm.h
	include/Util/ClearCase.h
	include/Util/FileFormats.h
	include/Util/FileSystem.h
	include/Util/Host.h
	include/Util/Input.h
	include/Util/Raise.h
	include/Util/regex.h
	include/Util/Resource.h
	include/Util/ServerCommon.h
	include/Util/String.h
	include/Util/string_view.h
	include/Util/System.h
	include/Util/User.h
	include/Util/utils.h
	include/Util/version.h.in
	include/Util/QtHelper.h
	include/Util/Compiler.h
)

target_include_directories(Util 
	PUBLIC
	    ${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_BINARY_DIR}/include
)

target_link_libraries(Util
PUBLIC
	Regex
	GSL
	Qt5::Core
	Qt5::Network
)

target_add_warnings(Util)

set_property(TARGET Util PROPERTY CXX_STANDARD ${TARGET_COMPILER_HIGHEST_STD_SUPPORTED})
set_property(TARGET Util PROPERTY CXX_EXTENSIONS OFF)


