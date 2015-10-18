cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

find_package(Motif REQUIRED)
find_package(X11 REQUIRED)
find_package(BISON REQUIRED)

find_library(LibM m)

# using a different name from parse.c
# because it will also output parse.h, which will conflict with the
# in source copy of that file!
bison_target(parser parse.y ${CMAKE_CURRENT_BINARY_DIR}/parser.c)


add_executable(nedit
	nedit.c
	file.c
	menu.c
	window.c
	selection.c
	search.c
	undo.c
	shift.c
	help.c
	preferences.c
	tags.c
	userCmds.c
	shell.c
	regularExp.c
	macro.c
	text.c
	textSel.c
	textDisp.c
	textBuf.c
	textDrag.c
	server.c
	highlight.c
	highlightData.c
	interpret.c
	smartIndent.c
	regexConvert.c
	rbTree.c
	windowTitle.c
	calltips.c
	server_common.c
	rangeset.c
	linkdate.c
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
	nc.c
	server_common.c)

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

set(CMAKE_C_FLAGS "-Wc++-compat")

# 
