cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Motif REQUIRED)
find_package(X11 REQUIRED)
find_package(BISON REQUIRED)

find_library(LibM m)

# using a different name from parse.c
# because it will also output parse.h, which will conflict with the
# in source copy of that file!
bison_target(parser parse.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp)


add_executable(nedit
	calltips.cpp
	file.cpp
	help.cpp
	highlight.cpp
	highlightData.cpp
	interpret.cpp
	linkdate.cpp
	macro.cpp
	menu.cpp
	nedit.cpp
	preferences.cpp
	rangeset.cpp
	rbTree.cpp
	regexConvert.cpp
	regularExp.cpp
	search.cpp
	selection.cpp
	server.cpp
	server_common.cpp
	shell.cpp
	shift.cpp
	smartIndent.cpp
	tags.cpp
	text.cpp
	textBuf.cpp
	textDisp.cpp
	textDrag.cpp
	textSel.cpp
	undo.cpp
	userCmds.cpp
	window.cpp
	windowTitle.cpp
	${BISON_parser_OUTPUT_SOURCE})

add_definitions(-DUSE_DIRENT -DUSE_LPR_PRINT_CMD)

include_directories(nedit
	${MOTIF_INCLUDES}
	${X11_INCLUDE_DIR}
	${X11_Xt_INCLUDE_PATH})

target_link_libraries(nedit LINK_PUBLIC
	Xlt
	NUtil
	XmL
	${LibM}
	${MOTIF_LIBRARIES}
	${X11_LIBRARIES}
	${X11_Xt_LIB})

add_executable(nc
	nc.cpp
	server_common.cpp)

include_directories(nc
	${X11_INCLUDE_DIR}
	${X11_Xt_INCLUDE_PATH})

target_link_libraries(nc LINK_PUBLIC
	NUtil
	${X11_Xt_LIB}
	${X11_LIBRARIES}
)

set_property(TARGET nedit PROPERTY C_STANDARD 99)
set_property(TARGET nc    PROPERTY C_STANDARD 99)

set_property(TARGET nedit PROPERTY CXX_STANDARD 11)
set_property(TARGET nc    PROPERTY CXX_STANDARD 11)

set(CMAKE_C_FLAGS "-Wc++-compat -Wwrite-strings")
#set(CMAKE_CXX_FLAGS "-W -Wall -pedantic")
