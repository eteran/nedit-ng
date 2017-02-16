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
	ElidedLabel.h
	DialogDuplicateTags.h
	DialogAbout.h
	DialogColors.h
	DialogDrawingStyles.h
	DialogExecuteCommand.h
	DialogFilter.h
	DialogFind.h
	DialogFonts.h
	DialogLanguageModes.h
	DialogMacros.h
	DialogMoveDocument.h
	DialogMultiReplace.h
	DialogPrint.h
	DialogReplace.h
	DialogShellMenu.h
	DialogSmartIndent.h
	DialogSmartIndentCommon.h
	DialogTabs.h
	DialogWindowBackgroundMenu.h
	DialogWindowSize.h
	DialogWindowTitle.h
	DialogWrapMargin.h
	DialogSyntaxPatterns.h
	DialogPrompt.h
	DialogPromptString.h
	DialogPromptList.h
	DialogRepeat.h
	ListWidget.h
	MainWindow.h
	KeySequenceEdit.h
	TextArea.h
	DocumentWidget.h
	TabWidget.h
	DialogOutput.h
	CallTipWidget.h
	TabWidget.h
	Settings.h
	CommandRecorder.h
	NeditServer.h
)

set(UI_FILES
	CallTipWidget.ui
	DialogDuplicateTags.ui
	DialogAbout.ui
	DialogColors.ui
	DialogDrawingStyles.ui
	DialogExecuteCommand.ui
	DialogFilter.ui
	DialogFind.ui
	DialogFonts.ui
	DialogLanguageModes.ui
	DialogMacros.ui
	DialogMoveDocument.ui
	DialogMultiReplace.ui
	DialogPrint.ui
	DialogReplace.ui
	DialogReplaceScope.ui
	DialogShellMenu.ui
	DialogSmartIndent.ui
	DialogSmartIndentCommon.ui
	DialogTabs.ui
	DialogWindowBackgroundMenu.ui
	DialogWindowSize.ui
	DialogWindowTitle.ui
	DialogWrapMargin.ui
	DialogSyntaxPatterns.ui
	DialogPrompt.ui
	DialogPromptString.ui
	DialogPromptList.ui
	DialogRepeat.ui
	MainWindow.ui
	DialogOutput.ui
	DocumentWidget.ui
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
	CallTipWidget.cpp
	DialogAbout.cpp
	DialogColors.cpp
	DialogDrawingStyles.cpp
	DialogDuplicateTags.cpp
	DialogExecuteCommand.cpp
	DialogFilter.cpp
	DialogFind.cpp
	DialogFonts.cpp
	DialogLanguageModes.cpp
	DialogMacros.cpp
	DialogMoveDocument.cpp
	DialogMultiReplace.cpp
	DialogOutput.cpp
	DialogPrint.cpp
	DialogPrompt.cpp
	DialogPromptList.cpp
	DialogPromptString.cpp
	DialogRepeat.cpp
	DialogReplace.cpp
	DialogShellMenu.cpp
	DialogSmartIndentCommon.cpp
	DialogSmartIndent.cpp
	DialogSyntaxPatterns.cpp
	DialogTabs.cpp
	DialogWindowBackgroundMenu.cpp
	DialogWindowSize.cpp
	DialogWindowTitle.cpp
	DialogWrapMargin.cpp
	DocumentWidget.cpp
	ElidedLabel.cpp
	KeySequenceEdit.cpp
	ListWidget.cpp
	MainWindow.cpp
	SignalBlocker.h
	TabWidget.cpp
	TabWidget.cpp
	TextArea.cpp
	UndoInfo.cpp
	userCmds.cpp
	X11Colors.cpp
	CommandRecorder.cpp
	NeditServer.cpp

	${BISON_parser_OUTPUT_SOURCE}
)

include_directories(nedit
	${PROJECT_SOURCE_DIR}
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

