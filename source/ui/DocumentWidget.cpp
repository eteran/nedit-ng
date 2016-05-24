
#include "DocumentWidget.h"
#include "TextArea.h"
#include <QBoxLayout>
#include <QSplitter>
#include "preferences.h"

DocumentWidget::DocumentWidget(const QString &name, QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {

	// create the text widget
	auto layout   = new QBoxLayout(QBoxLayout::TopToBottom);
	auto splitter = new QSplitter(Qt::Vertical, this);
	
	splitter->addWidget(new TextArea(this));
	splitter->setChildrenCollapsible(false);
	layout->addWidget(splitter);
	layout->setContentsMargins(0, 0, 0, 0);
	setLayout(layout);
	
	// initialize the members
	multiFileReplSelected_ = false;
	multiFileBusy_         = false;
	writableWindows_       = nullptr;
	nWritableWindows_      = 0;
	fileChanged_           = false;
	fileMissing_           = true;
	fileMode_              = 0;
	fileUid_               = 0;
	fileGid_               = 0;
	filenameSet_           = false;
	fileFormat_            = UNIX_FILE_FORMAT;
	lastModTime_           = 0;
	filename_              = name;
	undo_                  = std::list<UndoInfo *>();
	redo_                  = std::list<UndoInfo *>();
	nPanes_                = 0;
	autoSaveCharCount_     = 0;
	autoSaveOpCount_       = 0;
	undoMemUsed_           = 0;
	
	lockReasons_.clear();
	
	indentStyle_           = GetPrefAutoIndent(PLAIN_LANGUAGE_MODE);
	autoSave_              = GetPrefAutoSave();
	saveOldVersion_        = GetPrefSaveOldVersion();
	wrapMode_              = GetPrefWrap(PLAIN_LANGUAGE_MODE);
	overstrike_            = false;
	showMatchingStyle_     = GetPrefShowMatching();
	matchSyntaxBased_      = GetPrefMatchSyntaxBased();
	highlightSyntax_       = GetPrefHighlightSyntax();
	backlightCharTypes_    = QString();
	backlightChars_        = GetPrefBacklightChars();
	
#if 0
	if (window->backlightChars_) {
		const char *cTypes = GetPrefBacklightCharTypes();
		if (cTypes && window->backlightChars_) {			
			window->backlightCharTypes_ = QLatin1String(cTypes);
		}
	}
	
	window->modeMessageDisplayed_  = false;
	window->modeMessage_           = nullptr;
	window->ignoreModify_          = false;
	window->windowMenuValid_       = false;
	window->flashTimeoutID_        = 0;
	window->fileClosedAtom_        = None;
	window->wasSelected_           = false;
#endif
	fontName_              = GetPrefFontName();
	italicFontName_        = GetPrefItalicFontName();
	boldFontName_          = GetPrefBoldFontName();
	boldItalicFontName_    = GetPrefBoldItalicFontName();

#if 0
	dialogColors_          = nullptr;
	fontList_              = GetPrefFontList();
	italicFontStruct_      = GetPrefItalicFont();
	boldFontStruct_        = GetPrefBoldFont();
	boldItalicFontStruct_  = GetPrefBoldItalicFont();
	dialogFonts_           = nullptr;
#endif
	nMarks_                = 0;
#if 0
	markTimeoutID_         = 0;
#endif
	highlightData_         = nullptr;
	shellCmdData_          = nullptr;
	macroCmdData_          = nullptr;
	smartIndentData_       = nullptr;
	languageMode_          = PLAIN_LANGUAGE_MODE;
	iSearchHistIndex_      = 0;
	iSearchStartPos_       = -1;
	iSearchLastRegexCase_  = true;
	iSearchLastLiteralCase_= false;
#if 0
	bgMenuUndoItem_        = nullptr;
	bgMenuRedoItem_        = nullptr;
#endif
	device_                = 0;
	inode_                 = 0;	

}

DocumentWidget::~DocumentWidget() {
}

