cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
#set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTOUIC ON)
#set(CMAKE_AUTORCC ON)

find_package(Motif REQUIRED)
find_package(X11 REQUIRED)
find_package(BISON REQUIRED)
find_package(Boost 1.35 REQUIRED)
find_package(Qt4 4.6.0 REQUIRED QtCore QtGui)

find_library(LibM m)

# using a different name from parse.cpp
# because it will also output parse.h, which will conflict with the
# in source copy of that file!
bison_target(parser parse.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp)

qt4_wrap_cpp(MOC_SOURCES
	QtMotif.h
	QtMotifDialog.h
	QtMotifWidget.h
	QtXtWidget.h

	ui/DialogExecuteCommand.h
	ui/DialogFilter.h
	ui/DialogFind.h
	ui/DialogFontSelector.h
	ui/DialogFonts.h
	ui/DialogLanguageModes.h
	ui/DialogMultiReplace.h
	ui/DialogReplace.h
	ui/DialogTabs.h
	ui/DialogWindowSize.h
	ui/DialogWrapMargin.h
	ui/DialogSmartIndentCommon.h
	ui/DialogSmartIndent.h
	ui/DialogColors.h
	ui/DialogDrawingStyles.h
	ui/MainWindow.h
	ui/ListWidget.h
)

set(UI_FILES
	ui/DialogExecuteCommand.ui
	ui/DialogFilter.ui
	ui/DialogFind.ui
	ui/DialogFontSelector.ui
	ui/DialogFonts.ui
	ui/DialogLanguageModes.ui
	ui/DialogMoveDocument.ui
	ui/DialogMultiReplace.ui
	ui/DialogReplaceScope.ui
	ui/DialogReplace.ui
	ui/DialogTabs.ui
	ui/DialogWindowSize.ui
	ui/DialogWrapMargin.ui
	ui/DialogSmartIndentCommon.ui
	ui/DialogSmartIndent.ui	
	ui/DialogColors.ui
	ui/MainWindow.ui
	ui/DialogDrawingStyles.ui
)

qt4_wrap_ui(UI_H ${UI_FILES})

add_executable(nedit

	${MOC_SOURCES}
	${UI_H}

	ui/DialogExecuteCommand.cpp
	ui/DialogFilter.cpp
	ui/DialogFind.cpp
	ui/DialogFonts.cpp
	ui/DialogFontSelector.cpp
	ui/DialogLanguageModes.cpp
	ui/DialogMultiReplace.cpp
	ui/DialogReplace.cpp
	ui/DialogTabs.cpp
	ui/DialogWindowSize.cpp
	ui/DialogWrapMargin.cpp
	ui/ListWidget.cpp
	ui/DialogSmartIndentCommon.cpp
	ui/DialogSmartIndent.cpp
	ui/DialogColors.cpp
	ui/MainWindow.cpp
	ui/DialogDrawingStyles.cpp

	QtMotif.cpp
	QtMotifDialog.cpp
	QtMotifWidget.cpp
	QtXtWidget.cpp
	
	Color.cpp
	SmartIndent.cpp
	Document.cpp
	PatternSet.cpp
	Rangeset.cpp
	RangesetTable.cpp
	TextBuffer.cpp
	TextSelection.cpp
	UndoInfo.cpp
	calltips.cpp
	file.cpp
	help.cpp
	highlight.cpp
	highlightData.cpp
	interpret.cpp
	macro.cpp
	menu.cpp
	nedit.cpp
	preferences.cpp
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
	TextDisplay.cpp
	textDrag.cpp
	textSel.cpp
	userCmds.cpp
	window.cpp
	windowTitle.cpp
	HighlightPattern.cpp
	${CMAKE_CURRENT_BINARY_DIR}/DefaultCommonMacros.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet00.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet01.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet02.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet03.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet04.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet05.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet06.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet07.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet08.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet09.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet10.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet11.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet12.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet13.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet14.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet15.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet16.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet17.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet18.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet19.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet20.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet21.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet22.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet23.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet24.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet25.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet26.inc
	${CMAKE_CURRENT_BINARY_DIR}/DefaultPatternSet27.inc
	${BISON_parser_OUTPUT_SOURCE})

include(${QT_USE_FILE})

include_directories(nedit
	${CMAKE_CURRENT_SOURCE_DIR}/ui
	${MOTIF_INCLUDES}
	${X11_INCLUDE_DIR}
	${X11_Xt_INCLUDE_PATH}
)

target_link_libraries(nedit LINK_PUBLIC
	Xlt
	NUtil
	XmL
	${LibM}
	${MOTIF_LIBRARIES}
	${X11_LIBRARIES}
	${X11_Xt_LIB}
	Qt4::QtGui
)

add_executable(nc
	nc.cpp
	server_common.cpp)

include_directories(nc
	${X11_INCLUDE_DIR}
	${X11_Xt_INCLUDE_PATH}
	${QT_QTCORE_INCLUDE_DIR}
	${QT_QTGUI_INCLUDE_DIR})

target_link_libraries(nc LINK_PUBLIC
	NUtil
	${X11_Xt_LIB}
	${X11_LIBRARIES}
)

set_property(TARGET nedit PROPERTY C_STANDARD 99)
set_property(TARGET nc    PROPERTY C_STANDARD 99)

set_property(TARGET nedit PROPERTY CXX_STANDARD 11)
set_property(TARGET nc    PROPERTY CXX_STANDARD 11)

set(CMAKE_C_FLAGS "-Wc++-compat -Wwrite-strings -Wunused") # -ffunction-sections -fdata-sections
set(CMAKE_CXX_FLAGS "-W -Wall -pedantic -Wunused") #-Wuseless-cast -Wold-style-cast -ffunction-sections -fdata-sections

# SET_TARGET_PROPERTIES(nedit PROPERTIES LINK_FLAGS "-Wl,--gc-sections,--print-gc-sections")

set_target_properties(nedit nc
	PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

macro(GenerateInclude arg)
	add_custom_command(
		PRE_BUILD
		OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${arg}.inc
		COMMAND ${CMAKE_COMMAND} -DINFILE=${CMAKE_CURRENT_SOURCE_DIR}/res/${arg}.txt -DVARNAME=${arg} -DOUTFILE=${CMAKE_CURRENT_BINARY_DIR}/${arg}.inc -P ${CMAKE_CURRENT_SOURCE_DIR}/File2C.cmake
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/res/${arg}.txt
		)
endmacro()


GenerateInclude(DefaultCommonMacros)

GenerateInclude(DefaultCommonMacros)
GenerateInclude(DefaultPatternSet00)
GenerateInclude(DefaultPatternSet01)
GenerateInclude(DefaultPatternSet02)
GenerateInclude(DefaultPatternSet03)
GenerateInclude(DefaultPatternSet04)
GenerateInclude(DefaultPatternSet05)
GenerateInclude(DefaultPatternSet06)
GenerateInclude(DefaultPatternSet07)
GenerateInclude(DefaultPatternSet08)
GenerateInclude(DefaultPatternSet09)
GenerateInclude(DefaultPatternSet10)
GenerateInclude(DefaultPatternSet11)
GenerateInclude(DefaultPatternSet12)
GenerateInclude(DefaultPatternSet13)
GenerateInclude(DefaultPatternSet14)
GenerateInclude(DefaultPatternSet15)
GenerateInclude(DefaultPatternSet16)
GenerateInclude(DefaultPatternSet17)
GenerateInclude(DefaultPatternSet18)
GenerateInclude(DefaultPatternSet19)
GenerateInclude(DefaultPatternSet20)
GenerateInclude(DefaultPatternSet21)
GenerateInclude(DefaultPatternSet22)
GenerateInclude(DefaultPatternSet23)
GenerateInclude(DefaultPatternSet24)
GenerateInclude(DefaultPatternSet25)
GenerateInclude(DefaultPatternSet26)
GenerateInclude(DefaultPatternSet27)

