
#include "DocumentWidget.h"
#include "TextArea.h"
#include <QBoxLayout>
#include <QSplitter>
#include "preferences.h"

//------------------------------------------------------------------------------
// Name: DocumentWidget
//------------------------------------------------------------------------------
DocumentWidget::DocumentWidget(const QString &name, QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {

	// create the text widget
	auto layout   = new QBoxLayout(QBoxLayout::TopToBottom);
	auto splitter = new QSplitter(Qt::Vertical, this);

#if 0
	splitter->addWidget(new TextArea(this));
#endif
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
	
	if (backlightChars_) {
		const char *cTypes = GetPrefBacklightCharTypes();
		if (cTypes && backlightChars_) {			
			backlightCharTypes_ = QLatin1String(cTypes);
		}
	}
	
	modeMessageDisplayed_  = false;
	modeMessage_           = nullptr;
	ignoreModify_          = false;
	windowMenuValid_       = false;
	flashTimeoutID_        = 0;
	fileClosedAtom_        = None;
	wasSelected_           = false;
	fontName_              = GetPrefFontName();
	italicFontName_        = GetPrefItalicFontName();
	boldFontName_          = GetPrefBoldFontName();
	boldItalicFontName_    = GetPrefBoldItalicFontName();
	dialogColors_          = nullptr;
	fontList_              = GetPrefFontList();
	italicFontStruct_      = GetPrefItalicFont();
	boldFontStruct_        = GetPrefBoldFont();
	boldItalicFontStruct_  = GetPrefBoldItalicFont();
	dialogFonts_           = nullptr;
	nMarks_                = 0;
	markTimeoutID_         = 0;
	highlightData_         = nullptr;
	shellCmdData_          = nullptr;
	macroCmdData_          = nullptr;
	smartIndentData_       = nullptr;
	languageMode_          = PLAIN_LANGUAGE_MODE;
	iSearchHistIndex_      = 0;
	iSearchStartPos_       = -1;
	iSearchLastRegexCase_  = true;
	iSearchLastLiteralCase_= false;
	device_                = 0;
	inode_                 = 0;	
	
#if 0
	bgMenuUndoItem_        = nullptr;
	bgMenuRedoItem_        = nullptr;
#endif	

}

//------------------------------------------------------------------------------
// Name: ~DocumentWidget
//------------------------------------------------------------------------------
DocumentWidget::~DocumentWidget() {
}

