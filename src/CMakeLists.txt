cmake_minimum_required(VERSION 3.0)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.5.0 REQUIRED Widgets Network Xml PrintSupport)
find_package(Boost 1.35 REQUIRED)
find_package(Qt5LinguistTools)

set(TRANSLATIONS
	res/translations/nedit-ng_fr.ts
)

qt5_add_translation(QM_FILES ${TRANSLATIONS})
set(QM_FILES ${QM_FILES} PARENT_SCOPE)
add_custom_target (translations ALL DEPENDS ${QM_FILES})

# Create translations QRC file - translations.qrc
set(TRANSLATIONS_QRC "${CMAKE_CURRENT_BINARY_DIR}/translations.qrc")

file(WRITE ${TRANSLATIONS_QRC} "<RCC>\n\t<qresource prefix=\"/translations\">")
foreach(QM_FILE ${QM_FILES})
	get_filename_component(QM_FILE_NAME ${QM_FILE} NAME)
	file(APPEND ${TRANSLATIONS_QRC} "\n\t\t<file>${QM_FILE_NAME}</file>")
endforeach()

file(APPEND ${TRANSLATIONS_QRC} "\n\t</qresource>\n</RCC>")
list(APPEND QRC_FILES ${TRANSLATIONS_QRC})

qt5_add_resources(QRC_SOURCES
	nedit.qrc
	${TRANSLATIONS_QRC}
)

add_executable(nedit-ng

	${QRC_SOURCES}

	BlockDragTypes.h
	Bookmark.h
	CallTip.h
	CallTipWidget.cpp
	CallTipWidget.h
	CallTipWidget.ui
	CloseMode.h
	CommandRecorder.cpp
	CommandRecorder.h
	CommandSource.h
	CursorStyles.h
	DialogAbout.cpp
	DialogAbout.h
	DialogAbout.ui
	DialogColors.cpp
	DialogColors.h
	DialogColors.ui
	Dialog.cpp
	DialogDrawingStyles.cpp
	DialogDrawingStyles.h
	DialogDrawingStyles.ui
	DialogDuplicateTags.cpp
	DialogDuplicateTags.h
	DialogDuplicateTags.ui
	DialogExecuteCommand.cpp
	DialogExecuteCommand.h
	DialogExecuteCommand.ui
	DialogFilter.cpp
	DialogFilter.h
	DialogFilter.ui
	DialogFind.cpp
	DialogFind.h
	DialogFind.ui
	DialogFonts.cpp
	DialogFonts.h
	DialogFonts.ui
	Dialog.h
	DialogLanguageModes.cpp
	DialogLanguageModes.h
	DialogLanguageModes.ui
	DialogMacros.cpp
	DialogMacros.h
	DialogMacros.ui
	DialogMoveDocument.cpp
	DialogMoveDocument.h
	DialogMoveDocument.ui
	DialogMultiReplace.cpp
	DialogMultiReplace.h
	DialogMultiReplace.ui
	DialogOutput.cpp
	DialogOutput.h
	DialogOutput.ui
	DialogPrint.cpp
	DialogPrint.h
	DialogPrint.ui
	DialogPrompt.cpp
	DialogPrompt.h
	DialogPromptList.cpp
	DialogPromptList.h
	DialogPromptList.ui
	DialogPromptString.cpp
	DialogPromptString.h
	DialogPromptString.ui
	DialogPrompt.ui
	DialogRepeat.cpp
	DialogRepeat.h
	DialogRepeat.ui
	DialogReplace.cpp
	DialogReplace.h
	DialogReplace.ui
	DialogShellMenu.cpp
	DialogShellMenu.h
	DialogShellMenu.ui
	DialogSmartIndentCommon.cpp
	DialogSmartIndentCommon.h
	DialogSmartIndentCommon.ui
	DialogSmartIndent.cpp
	DialogSmartIndent.h
	DialogSmartIndent.ui
	DialogSyntaxPatterns.cpp
	DialogSyntaxPatterns.h
	DialogSyntaxPatterns.ui
	DialogTabs.cpp
	DialogTabs.h
	DialogTabs.ui
	DialogWindowBackgroundMenu.cpp
	DialogWindowBackgroundMenu.h
	DialogWindowBackgroundMenu.ui
	DialogWindowSize.cpp
	DialogWindowSize.h
	DialogWindowSize.ui
	DialogWindowTitle.cpp
	DialogWindowTitle.h
	DialogWindowTitle.ui
	DialogWrapMargin.cpp
	DialogWrapMargin.h
	DialogWrapMargin.ui
	Direction.h
	DocumentModel.cpp
	DocumentModel.h
	DocumentWidget.cpp
	DocumentWidget.h
	DocumentWidget.ui
	DragEndEvent.h
	DragStates.h
	EditFlags.h
	ElidedLabel.cpp
	ElidedLabel.h
	Font.cpp
	Font.h
	FontType.h
	gap_buffer_fwd.h
	gap_buffer.h
	gap_buffer_iterator.h
	Help.cpp
	Help.h
	Highlight.cpp
	HighlightData.h
	Highlight.h
	HighlightPattern.cpp
	HighlightPattern.h
	HighlightPatternModel.cpp
	HighlightPatternModel.h
	HighlightStyle.h
	HighlightStyleModel.cpp
	HighlightStyleModel.h
	KeySequenceEdit.cpp
	KeySequenceEdit.h
	LanguageMode.h
	LanguageModeModel.cpp
	LanguageModeModel.h
	LineNumberArea.cpp
	LineNumberArea.h
	LockReasons.h
	macro.cpp
	macro.h
	Main.cpp
	Main.h
	MainWindow.cpp
	MainWindow.h
	MainWindow.ui
	MenuData.h
	MenuItem.h
	MenuItemModel.cpp
	MenuItemModel.h
	MultiClickStates.h
	nedit.cpp
	nedit.h
	NeditServer.cpp
	NeditServer.h
	NewMode.h
	PatternSet.cpp
	PatternSet.h
	Preferences.cpp
	Preferences.h
	Rangeset.cpp
	Rangeset.h
	RangesetTable.cpp
	RangesetTable.h
	ReparseContext.h
	Search.cpp
	Search.h
	shift.cpp
	ShiftDirection.h
	shift.h
	SignalBlocker.h
	SmartIndent.cpp
	SmartIndentEntry.cpp
	SmartIndentEntry.h
	SmartIndentEvent.h
	SmartIndent.h
	Style.h
	StyleTableEntry.h
	TabWidget.cpp
	TabWidget.h
	Tags.cpp
	Tags.h
	TextArea.cpp
	TextArea.h
	TextAreaMimeData.cpp
	TextAreaMimeData.h
	TextBuffer.cpp
	TextBufferFwd.h
	TextBuffer.h
	TextBuffer.tcc
	TextCursor.h
	TextEditEvent.cpp
	TextEditEvent.h
	UndoInfo.cpp
	UndoInfo.h
	userCmds.cpp
	userCmds.h
	Verbosity.h
	WindowHighlightData.h
	WindowMenuEvent.cpp
	WindowMenuEvent.h
	WrapMode.h
	X11Colors.cpp
	X11Colors.h
)

target_include_directories (nedit-ng PRIVATE
	${Boost_INCLUDE_DIR}
)

target_link_libraries(nedit-ng LINK_PUBLIC
	Util
	Regex
	Settings
	Interpreter
	GSL
	Qt5::Widgets
	Qt5::Network
	Qt5::Xml
	Qt5::PrintSupport
)

set_property(TARGET nedit-ng PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET nedit-ng PROPERTY CXX_STANDARD 14)

set_target_properties(nedit-ng
	PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

install (TARGETS nedit-ng DESTINATION bin)
