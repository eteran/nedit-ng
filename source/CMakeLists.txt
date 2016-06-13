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

qt4_add_resources(QRC_SOURCES
	nedit.qrc
)

qt4_wrap_cpp(MOC_SOURCES
	QtMotif.h
	QtMotifDialog.h
	QtMotifWidget.h
	QtXtWidget.h

	ui/DialogDuplicateTags.h
	ui/DialogAbout.h
	ui/DialogColors.h
	ui/DialogDrawingStyles.h
	ui/DialogExecuteCommand.h
	ui/DialogFilter.h
	ui/DialogFind.h
	ui/DialogFontSelector.h
	ui/DialogFonts.h
	ui/DialogLanguageModes.h
	ui/DialogMacros.h
	ui/DialogMoveDocument.h
	ui/DialogMultiReplace.h
	ui/DialogPrint.h
	ui/DialogReplace.h
	ui/DialogShellMenu.h
	ui/DialogSmartIndent.h
	ui/DialogSmartIndentCommon.h
	ui/DialogTabs.h
	ui/DialogWindowBackgroundMenu.h
	ui/DialogWindowSize.h
	ui/DialogWindowTitle.h
	ui/DialogWrapMargin.h
	ui/DialogSyntaxPatterns.h
	ui/DialogPrompt.h
	ui/DialogPromptString.h
	ui/DialogPromptList.h
	ui/DialogRepeat.h
	ui/LineEdit.h
	ui/ListWidget.h
	ui/MainWindow.h
	ui/KeySequenceEdit.h
	ui/TextArea.h
	ui/DocumentWidget.h
	ui/TabWidget.h
	ui/DialogOutput.h
)

set(UI_FILES
	ui/DialogDuplicateTags.ui
	ui/DialogAbout.ui
	ui/DialogColors.ui
	ui/DialogDrawingStyles.ui
	ui/DialogExecuteCommand.ui
	ui/DialogFilter.ui
	ui/DialogFind.ui
	ui/DialogFontSelector.ui
	ui/DialogFonts.ui
	ui/DialogLanguageModes.ui
	ui/DialogMacros.ui
	ui/DialogMoveDocument.ui
	ui/DialogMultiReplace.ui
	ui/DialogPrint.ui
	ui/DialogReplace.ui
	ui/DialogReplaceScope.ui
	ui/DialogShellMenu.ui
	ui/DialogSmartIndent.ui	
	ui/DialogSmartIndentCommon.ui
	ui/DialogTabs.ui
	ui/DialogWindowBackgroundMenu.ui
	ui/DialogWindowSize.ui
	ui/DialogWindowTitle.ui
	ui/DialogWrapMargin.ui
	ui/DialogSyntaxPatterns.ui
	ui/DialogPrompt.ui
	ui/DialogPromptString.ui
	ui/DialogPromptList.ui
	ui/DialogRepeat.ui
	ui/MainWindow.ui
	ui/DialogOutput.ui
)

qt4_wrap_ui(UI_H ${UI_FILES})

add_executable(nedit

	${MOC_SOURCES}
	${QRC_SOURCES}
	${UI_H}



	QtMotif.cpp
	QtMotifDialog.cpp
	QtMotifWidget.cpp
	QtXtWidget.cpp
	
	UserMenuListElement.cpp
	LanguageMode.cpp
	MenuItem.cpp
	Color.cpp
	SmartIndent.cpp
	Document.cpp
	PatternSet.cpp
	Rangeset.cpp
	RangesetTable.cpp
	TextBuffer.cpp
	TextSelection.cpp
	UndoInfo.cpp
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
	userCmds.cpp
	window.cpp
	HighlightPattern.cpp
	
	
	ui/DialogDuplicateTags.cpp
	ui/DialogAbout.cpp
	ui/DialogColors.cpp
	ui/DialogDrawingStyles.cpp
	ui/DialogExecuteCommand.cpp
	ui/DialogFilter.cpp
	ui/DialogFind.cpp
	ui/DialogFontSelector.cpp
	ui/DialogFonts.cpp
	ui/DialogLanguageModes.cpp
	ui/DialogMacros.cpp
	ui/DialogMoveDocument.cpp
	ui/DialogMultiReplace.cpp
	ui/DialogPrint.cpp
	ui/DialogReplace.cpp
	ui/DialogShellMenu.cpp
	ui/DialogSmartIndent.cpp
	ui/DialogSmartIndentCommon.cpp
	ui/DialogTabs.cpp
	ui/DialogWindowBackgroundMenu.cpp
	ui/DialogWindowSize.cpp
	ui/DialogWindowTitle.cpp
	ui/DialogWrapMargin.cpp
	ui/LineEdit.cpp
	ui/ListWidget.cpp
	ui/MainWindow.cpp
	ui/DialogPrompt.cpp
	ui/DialogPromptString.cpp
	ui/DialogPromptList.cpp
	ui/DialogSyntaxPatterns.cpp
	ui/DialogRepeat.cpp
	ui/KeySequenceEdit.cpp
	ui/TextArea.cpp
	ui/DocumentWidget.cpp
	ui/TabWidget.cpp
	ui/DialogOutput.cpp
		
	${BISON_parser_OUTPUT_SOURCE}
)

include(${QT_USE_FILE})

include_directories(nedit
	${CMAKE_CURRENT_SOURCE_DIR}/ui
	${CMAKE_CURRENT_SOURCE_DIR}/include
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

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wc++-compat -Wwrite-strings -Wunused") # -ffunction-sections -fdata-sections
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -pedantic -Wunused") #-Wuseless-cast -Wold-style-cast -ffunction-sections -fdata-sections

# SET_TARGET_PROPERTIES(nedit PROPERTIES LINK_FLAGS "-Wl,--gc-sections,--print-gc-sections")

set_target_properties(nedit nc
	PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

