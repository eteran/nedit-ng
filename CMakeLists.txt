cmake_minimum_required(VERSION 3.0)

project(nedit CXX)

enable_testing()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/Modules/")
include("DetectCompiler")

if((TARGET_COMPILER_GCC) OR (TARGET_COMPILER_CLANG))
    option(ENABLE_PROFILING     "Enable profiling")
    option(ENABLE_ASAN          "Enable address santiziers")
    option(ENABLE_USAN          "Enable undefined santiziers")
    option(ENABLE_MSAN          "Enable memory santiziers")
    option(ENABLE_TSAN          "Enable thread santiziers")
    option(ENABLE_LTO           "Enable Link Time Optimization")
    option(ENABLE_STL_DEBUG     "Enable STL container debugging")
elseif(TARGET_COMPILER_MSVC)
    option(ENABLE_STL_DEBUG     "Enable STL container debugging")
endif()

option(PURIFY "Fill Unused TextBuffer space")

set(PER_TAB_CLOSE     ON CACHE BOOL "Per Tab Close Buttons")
set(VISUAL_CTRL_CHARS ON CACHE BOOL "Visualize ASCII Control Characters")

if((TARGET_COMPILER_GCC) OR (TARGET_COMPILER_CLANG))
	if(ENABLE_STL_DEBUG)
		add_definitions(-D_GLIBCXX_DEBUG)
		add_definitions(-D_GLIBCXX_DEBUG_PEDANTIC)
	endif()

	if(ENABLE_PROFILING)
		set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS}        -pg -no-pie")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")
	endif()
	
	if(ENABLE_ASAN)
		set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS}        -fsanitize=address") # -fsanitize-address-use-after-scope
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
		add_definitions(-D_GLIBCXX_SANITIZE_VECTOR)
	endif()
	
	if(ENABLE_USAN)
		set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS}        -fsanitize=undefined")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
	endif()

	if(ENABLE_TSAN)
		set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS}        -fsanitize=thread")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
	endif()

	if(ENABLE_MSAN)
		set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS}        -fsanitize=memory")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=memory")
	endif()

	if(ENABLE_LTO)
		set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS}        -flto")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
		set(CMAKE_AR               "gcc-ar")
		set(CMAKE_RANLIB           "gcc-ranlib")
	endif()
elseif(TARGET_COMPILER_MSVC)
    if(ENABLE_STL_DEBUG)
        add_definitions(-D_ITERATOR_DEBUG_LEVEL=1)
    endif()
endif()

find_package(Boost 1.35 REQUIRED)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

if(PURIFY)
	add_definitions(-DPURIFY)
endif()

if(VISUAL_CTRL_CHARS)
	add_definitions(-DVISUAL_CTRL_CHARS)
endif()

if(PER_TAB_CLOSE)
	add_definitions(-DPER_TAB_CLOSE)
endif()

add_definitions(-DQT_NO_CAST_FROM_ASCII)
add_definitions(-DQT_NO_CAST_TO_ASCII)
add_definitions(-DQT_NO_KEYWORDS)

if(TARGET_COMPILER_MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -DUNICODE -D_UNICODE)
else()
    add_compile_options(
        -W
        -Wall
        #-Wshadow
        -Wnon-virtual-dtor
        #-Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -pedantic
        #-Wconversion
        #-Wsign-conversion
        #-Wnull-dereference
        -Wdouble-promotion
        -Wformat=2
        -Wno-unused-macros
        -Wno-switch-enum
        -Wno-unknown-pragmas
    )

    if(TARGET_COMPILER_CLANG)
		add_compile_options(
			#-Wshorten-64-to-32
			-Wconditional-uninitialized
			#-Wshadow-uncaptured-local
			-Wmissing-prototypes
			#-Wexit-time-destructors
			#-Wglobal-constructors
			-Wimplicit-fallthrough
		)

    elseif(TARGET_COMPILER_GCC)
		add_compile_options(
			#-Wuseless-cast
			#-Wduplicated-cond
			#-Wduplicated-branches
			-Wlogical-op
			
			#-Wsuggest-attribute=pure
			#-Wsuggest-attribute=const
			#-Wsuggest-attribute=noreturn
			-Wsuggest-final-types
			-Wsuggest-final-methods
			-Wsuggest-override
		)
	endif()
endif()

add_subdirectory(Util)
add_subdirectory(src)
add_subdirectory(client)
add_subdirectory(Regex)
add_subdirectory(Settings)
add_subdirectory(Interpreter)
add_subdirectory(GSL)

if(UNIX)
    find_package(X11)
    if(${X11_FOUND})
        add_subdirectory(import)
    endif()
endif()
