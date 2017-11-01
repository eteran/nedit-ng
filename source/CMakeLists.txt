cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
#set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTOUIC ON)
#set(CMAKE_AUTORCC ON)

find_package(BISON REQUIRED)
find_package(Qt5 5.4.0 REQUIRED Widgets Network)

# using a different name from parse.cpp
# because it will also output parse.h, which will conflict with the
# in source copy of that file!
bison_target(parser parse.y ${CMAKE_CURRENT_BINARY_DIR}/parser.cpp)

qt5_add_resources(QRC_SOURCES
	nedit.qrc
)

qt5_wrap_cpp(MOC_SOURCES
	CallTipWidget.h
	CommandRecorder.h
	Dialog.h
	DialogAbout.h
	DialogColors.h
	DialogDrawingStyles.h
	DialogDuplicateTags.h
	DialogExecuteCommand.h
	DialogFilter.h
	DialogFind.h
	DialogFonts.h
	DialogLanguageModes.h
	DialogMacros.h
	DialogMoveDocument.h
	DialogMultiReplace.h
	DialogOutput.h
	DialogPrint.h
	DialogPrompt.h
	DialogPromptList.h
	DialogPromptString.h
	DialogRepeat.h
	DialogReplace.h
	DialogShellMenu.h
	DialogSmartIndent.h
	DialogSmartIndentCommon.h
	DialogSyntaxPatterns.h
	DialogTabs.h
	DialogWindowBackgroundMenu.h
	DialogWindowSize.h
	DialogWindowTitle.h
	DialogWrapMargin.h
	DocumentWidget.h
	ElidedLabel.h
	KeySequenceEdit.h
	MainWindow.h
	NeditServer.h
	Settings.h
	TextArea.h	
)

set(UI_FILES
	CallTipWidget.ui
	DialogAbout.ui
	DialogColors.ui
	DialogDrawingStyles.ui
	DialogDuplicateTags.ui
	DialogExecuteCommand.ui
	DialogFilter.ui
	DialogFind.ui
	DialogFonts.ui
	DialogLanguageModes.ui
	DialogMacros.ui
	DialogMoveDocument.ui
	DialogMultiReplace.ui
	DialogOutput.ui
	DialogPrint.ui
	DialogPrompt.ui
	DialogPromptList.ui
	DialogPromptString.ui
	DialogRepeat.ui
	DialogReplace.ui
	DialogReplaceScope.ui
	DialogShellMenu.ui
	DialogSmartIndent.ui
	DialogSmartIndentCommon.ui
	DialogSyntaxPatterns.ui
	DialogTabs.ui
	DialogWindowBackgroundMenu.ui
	DialogWindowSize.ui
	DialogWindowTitle.ui
	DialogWrapMargin.ui
	DocumentWidget.ui
	MainWindow.ui
)

qt5_wrap_ui(UI_H ${UI_FILES})

add_executable(nedit-ng

	${MOC_SOURCES}
	${QRC_SOURCES}
	${UI_H}

	Input.cpp
	Help.cpp
	Font.cpp
	CallTipWidget.cpp
	CommandRecorder.cpp
	Dialog.cpp
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
	TextEditEvent.cpp
	WindowMenuEvent.cpp
	DialogReplace.cpp
	DialogShellMenu.cpp
	DialogSmartIndent.cpp
	DialogSmartIndentCommon.cpp
	DialogSyntaxPatterns.cpp
	DialogTabs.cpp
	DialogWindowBackgroundMenu.cpp
	DialogWindowSize.cpp
	DialogWindowTitle.cpp
	DialogWrapMargin.cpp
	DocumentWidget.cpp
	ElidedLabel.cpp
	HighlightPattern.cpp
	KeySequenceEdit.cpp
	MainWindow.cpp
	NeditServer.cpp
	PatternSet.cpp
	Rangeset.cpp
	RangesetTable.cpp
	Settings.cpp
	SignalBlocker.h
	SmartIndentEntry.cpp
	TextArea.cpp
	TextBuffer.cpp
	TextSelection.cpp
	UndoInfo.cpp
	X11Colors.cpp
	highlight.cpp
	highlightData.cpp
	interpret.cpp
	macro.cpp
	nedit.cpp
	preferences.cpp
	rbTree.cpp
	search.cpp
	selection.cpp
	shift.cpp
	smartIndent.cpp
	tags.cpp
	userCmds.cpp

	${BISON_parser_OUTPUT_SOURCE}
)

include_directories(nedit-ng
	${PROJECT_SOURCE_DIR}
)

target_link_libraries(nedit-ng LINK_PUBLIC
	NUtil
	RegExp
	Qt5::Widgets
	Qt5::Network
)

set_property(TARGET nedit-ng PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET nedit-ng PROPERTY CXX_STANDARD 14)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -pedantic -Wunused") #-Wuseless-cast -Wold-style-cast -ffunction-sections -fdata-sections

# SET_TARGET_PROPERTIES(nedit-ng PROPERTIES LINK_FLAGS "-Wl,--gc-sections,--print-gc-sections")

set_target_properties(nedit-ng
	PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

