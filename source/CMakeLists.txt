cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
#set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTOUIC ON)
#set(CMAKE_AUTORCC ON)

find_package(BISON REQUIRED)
find_package(Qt5 5.4.0 REQUIRED Widgets X11Extras Concurrent DBus)

# using a different name from parse.cpp
# because it will also output parse.h, which will conflict with the
# in source copy of that file!
bison_target(parser parse.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp)

qt5_add_resources(QRC_SOURCES
	nedit.qrc
)

qt5_wrap_cpp(MOC_SOURCES
	ui/ElidedLabel.h
	ui/DialogDuplicateTags.h
	ui/DialogAbout.h
	ui/DialogColors.h
	ui/DialogDrawingStyles.h
	ui/DialogExecuteCommand.h
	ui/DialogFilter.h
	ui/DialogFind.h
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
	ui/ListWidget.h
	ui/MainWindow.h
	ui/KeySequenceEdit.h
	ui/TextArea.h
	ui/DocumentWidget.h
	ui/TabWidget.h
	ui/DialogOutput.h
	ui/CallTipWidget.h
	ui/TabWidget.h
	Settings.h
	CommandRecorder.h
	NeditServer.h
)

set(UI_FILES
	ui/CallTipWidget.ui
	ui/DialogDuplicateTags.ui
	ui/DialogAbout.ui
	ui/DialogColors.ui
	ui/DialogDrawingStyles.ui
	ui/DialogExecuteCommand.ui
	ui/DialogFilter.ui
	ui/DialogFind.ui
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
	ui/DocumentWidget.ui
)

qt5_wrap_ui(UI_H ${UI_FILES})



add_executable(nedit

	${MOC_SOURCES}
	${QRC_SOURCES}
	${UI_H}

	Settings.cpp
	file.cpp
	highlight.cpp
	highlightData.cpp
	HighlightPattern.cpp
	interpret.cpp
	LanguageMode.cpp
	macro.cpp
	menu.cpp
	MenuItem.cpp
	nedit.cpp
	PatternSet.cpp
	preferences.cpp
	Rangeset.cpp
	RangesetTable.cpp
	rbTree.cpp
	regularExp.cpp
	search.cpp
	selection.cpp
	shift.cpp
	smartIndent.cpp
	SmartIndent.cpp
	tags.cpp
	TextBuffer.cpp
	TextSelection.cpp
	ui/CallTipWidget.cpp
	ui/DialogAbout.cpp
	ui/DialogColors.cpp
	ui/DialogDrawingStyles.cpp
	ui/DialogDuplicateTags.cpp
	ui/DialogExecuteCommand.cpp
	ui/DialogFilter.cpp
	ui/DialogFind.cpp
	ui/DialogFonts.cpp
	ui/DialogLanguageModes.cpp
	ui/DialogMacros.cpp
	ui/DialogMoveDocument.cpp
	ui/DialogMultiReplace.cpp
	ui/DialogOutput.cpp
	ui/DialogPrint.cpp
	ui/DialogPrompt.cpp
	ui/DialogPromptList.cpp
	ui/DialogPromptString.cpp
	ui/DialogRepeat.cpp
	ui/DialogReplace.cpp
	ui/DialogShellMenu.cpp
	ui/DialogSmartIndentCommon.cpp
	ui/DialogSmartIndent.cpp
	ui/DialogSyntaxPatterns.cpp
	ui/DialogTabs.cpp
	ui/DialogWindowBackgroundMenu.cpp
	ui/DialogWindowSize.cpp
	ui/DialogWindowTitle.cpp
	ui/DialogWrapMargin.cpp
	ui/DocumentWidget.cpp
	ui/ElidedLabel.cpp
	ui/KeySequenceEdit.cpp
	ui/ListWidget.cpp
	ui/MainWindow.cpp
	ui/SignalBlocker.h
	ui/TabWidget.cpp
	ui/TabWidget.cpp
	ui/TextArea.cpp
	UndoInfo.cpp
	userCmds.cpp
	X11Colors.cpp
	CommandRecorder.cpp
	NeditServer.cpp

	${BISON_parser_OUTPUT_SOURCE}
)

include_directories(nedit
	${PROJECT_SOURCE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/ui
	${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(nedit LINK_PUBLIC
	NUtil
	Qt5::Widgets
	Qt5::X11Extras
	Qt5::Concurrent
	Qt5::DBus
)

qt5_wrap_cpp(MOC_SOURCES_NC
	Settings.h
)


set_property(TARGET nedit PROPERTY C_STANDARD 99)
set_property(TARGET nedit PROPERTY CXX_STANDARD 14)


set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wc++-compat -Wwrite-strings -Wunused") # -ffunction-sections -fdata-sections
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -pedantic -Wunused") #-Wuseless-cast -Wold-style-cast -ffunction-sections -fdata-sections


set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG}   -ggdb3")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ggdb3")

# SET_TARGET_PROPERTIES(nedit PROPERTIES LINK_FLAGS "-Wl,--gc-sections,--print-gc-sections")

set_target_properties(nedit
	PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

