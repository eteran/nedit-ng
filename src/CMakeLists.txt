cmake_minimum_required(VERSION 3.3)


set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 5.5.0 REQUIRED Widgets Network Xml PrintSupport LinguistTools)
find_package(Qt5 5.5.0 QUIET OPTIONAL_COMPONENTS X11Extras)

if(UNIX)
	find_package(X11)
endif()

qt5_add_translation(QM_FILES
	res/translations/nedit-ng_fr.ts
	res/translations/nedit-ng_fi.ts
)

if(WIN32)
	set(APP_ICON_RESOURCE_WINDOWS "res/nedit-ng.rc")
endif()

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
	res/nedit-ng.qrc
	res/breeze-nedit.qrc
	res/breeze-dark-nedit.qrc
	${TRANSLATIONS_QRC}
)

foreach(FILE IN LISTS QRC_SOURCES)
	set_property(SOURCE ${FILE} PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)
endforeach()

if(${X11_FOUND})
    if(TARGET_PLATFORM_MACOS)
        link_directories(/usr/X11/lib)
        link_directories(/opt/X11/lib)
    endif()
endif()

#set_property(SOURCE NeditServer.cpp PROPERTY SKIP_UNITY_BUILD_INCLUSION ON)

add_executable(nedit-ng

	${QRC_SOURCES}
	${APP_ICON_RESOURCE_WINDOWS}

	Theme.h
	Theme.cpp
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
	CommonDialog.h
	CursorStyles.h
	Dialog.cpp
	Dialog.h
	DialogAbout.cpp
	DialogAbout.h
	DialogAbout.ui
	DialogColors.cpp
	DialogColors.h
	DialogColors.ui
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
	DialogPrompt.ui
	DialogPromptList.cpp
	DialogPromptList.h
	DialogPromptList.ui
	DialogPromptString.cpp
	DialogPromptString.h
	DialogPromptString.ui
	DialogRepeat.cpp
	DialogRepeat.h
	DialogRepeat.ui
	DialogReplace.cpp
	DialogReplace.h
	DialogReplace.ui
	DialogShellMenu.cpp
	DialogShellMenu.h
	DialogShellMenu.ui
	DialogSmartIndent.cpp
	DialogSmartIndent.h
	DialogSmartIndent.ui
	DialogSmartIndentCommon.cpp
	DialogSmartIndentCommon.h
	DialogSmartIndentCommon.ui
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
	DocumentInfo.cpp
	DocumentInfo.h
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
	ErrorSound.h
	Font.cpp
	Font.h
	Help.cpp
	Help.h
	Highlight.cpp
	Highlight.h
	HighlightData.h
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
	Location.h
	LockReasons.h
	Main.cpp
	Main.h
	MainWindow.cpp
	MainWindow.h
	MainWindow.ui
	MenuData.h
	MenuItem.h
	MenuItemModel.cpp
	MenuItemModel.h
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
	ShiftDirection.h
	SignalBlocker.h
	SmartIndent.cpp
	SmartIndent.h
	SmartIndentEntry.cpp
	SmartIndentEntry.h
	SmartIndentEvent.h
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
	TextBuffer.h
	TextBuffer.tcc
	TextBufferFwd.h
	TextCursor.h
	TextEditEvent.cpp
	TextEditEvent.h
	TextRange.h
	UndoInfo.cpp
	UndoInfo.h
	Verbosity.h
	WindowHighlightData.h
	WindowMenuEvent.cpp
	WindowMenuEvent.h
	WrapMode.h
	X11Colors.cpp
	X11Colors.h
	gap_buffer.h
	gap_buffer_fwd.h
	gap_buffer_iterator.h
	macro.cpp
	macro.h
	nedit.cpp
	nedit.h
	shift.cpp
	shift.h
	userCmds.cpp
	userCmds.h
)

if(${X11_FOUND})
    if(Qt5X11Extras_FOUND)
        target_compile_definitions(nedit-ng
            PUBLIC -DQT_X11
        )
    endif()
endif()

target_add_warnings(nedit-ng)

target_link_libraries(nedit-ng
PUBLIC
	Util
	Regex
	Settings
	Interpreter
	GSL
	Qt5::Widgets
	Qt5::Network
	Qt5::Xml
	Qt5::PrintSupport
	$<$<BOOL:${Qt5X11Extras_FOUND}>:Qt5::X11Extras>
	$<$<BOOL:${X11_FOUND}>:X11>
PRIVATE
	Boost::boost
	yaml-cpp
)
set_property(TARGET nedit-ng PROPERTY CXX_EXTENSIONS OFF)
set_property(TARGET nedit-ng PROPERTY CXX_STANDARD ${TARGET_COMPILER_HIGHEST_STD_SUPPORTED})
set_property(TARGET nedit-ng PROPERTY RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

if(TARGET_COMPILER_MSVC)
	set_property(TARGET nedit-ng PROPERTY WIN32_EXECUTABLE ON)
endif()

install(TARGETS nedit-ng DESTINATION bin)
