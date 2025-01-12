cmake_minimum_required(VERSION 3.15)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network Xml PrintSupport LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network Xml PrintSupport LinguistTools)

if(WIN32)
	set(APP_ICON_RESOURCE_WINDOWS "res/nedit-ng.rc")
endif()

set(TS_FILES
	i18n/nedit-ng_fr.ts
	i18n/nedit-ng_fi.ts
)

set(PROJECT_SOURCES
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
	gap_buffer_fwd.h
	gap_buffer_iterator.h
	gap_buffer.h
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
	shift.h
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
	Theme.cpp
	Theme.h
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
	${TS_FILES}
)

set(TRANSLATIONS_QRC ${CMAKE_CURRENT_BINARY_DIR}/i18n.qrc)

if(QT_VERSION_MAJOR EQUAL 6)
	qt6_add_resources(PROJECT_SOURCES
		res/nedit-ng.qrc
		res/breeze-nedit.qrc
		res/breeze-dark-nedit.qrc
	)

	qt6_add_translation(QM_FILES ${TS_FILES})

	generate_translation_resource_file(${TRANSLATIONS_QRC} "${QM_FILES}")

    qt6_add_executable(nedit-ng
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
		${TRANSLATIONS_QRC}
		${APP_ICON_RESOURCE_WINDOWS}
    )
else()
	qt5_add_resources(PROJECT_SOURCES
		res/nedit-ng.qrc
		res/breeze-nedit.qrc
		res/breeze-dark-nedit.qrc
	)

	qt5_add_translation(QM_FILES ${TS_FILES})

	generate_translation_resource_file(${TRANSLATIONS_QRC} "${QM_FILES}")

    add_executable(nedit-ng
        ${PROJECT_SOURCES}
		${TRANSLATIONS_QRC}
		${APP_ICON_RESOURCE_WINDOWS}
    )
endif()

target_add_warnings(nedit-ng)

target_link_libraries(nedit-ng
PRIVATE
	Util
	Regex
	Settings
	Interpreter
	GSL
	Qt${QT_VERSION_MAJOR}::Widgets
	Qt${QT_VERSION_MAJOR}::Network
	Qt${QT_VERSION_MAJOR}::Xml
	Qt${QT_VERSION_MAJOR}::PrintSupport
	yaml-cpp
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
	set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.nedit-ng)
endif()

set_target_properties(nedit-ng PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

if(TARGET_COMPILER_MSVC)
	target_compile_definitions(nedit-ng
		PUBLIC -D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
	)
endif()

include(GNUInstallDirs)
install(TARGETS nedit-ng
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt6_finalize_executable(nedit-ng)
endif()
