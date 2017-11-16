cmake_minimum_required(VERSION 3.0)

project (nedit CXX)

option(ENABLE_ASAN   "Enable various santiziers")
option(ENABLE_LTO    "Enable Link Time Optimization")
option(REPLACE_SCOPE "Use Alternative Search Dialog")

if(ENABLE_ASAN)
	set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS}        -g3 -fsanitize=address -fsanitize=undefined")
	set(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS}          -g3 -fsanitize=address -fsanitize=undefined")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g3 -fsanitize=address -fsanitize=undefined")
endif()

if(ENABLE_LTO)
	set(CMAKE_CXX_FLAGS        "${CMAKE_CXX_FLAGS} -flto")
	set(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} -flto")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -flto")
endif()

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif()

if(REPLACE_SCOPE)
	add_definitions(-DREPLACE_SCOPE)
endif()


add_definitions(-DQT_NO_CAST_FROM_ASCII)
add_definitions(-DQT_NO_CAST_TO_ASCII)

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wuseless-cast -Wno-unused-macros -Wno-newline-eof -Wno-exit-time-destructors -Wno-shadow -Wno-switch-enum -Wno-unknown-pragmas -Wno-padded -Wno-date-time")
# -Weverything
# -Wno-c++98-compat-pedantic
# -Wno-c++98-compat
# -Wno-documentation-unknown-command
# -Wno-global-constructors
# -Wno-gnu-zero-variadic-macro-arguments
# -Wno-documentation
# -Wno-missing-prototypes
# -Wno-used-but-marked-unused

add_subdirectory (util)
add_subdirectory (source)
add_subdirectory (client)
add_subdirectory (regex)

