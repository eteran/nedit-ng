
#include <QBoxLayout>
#include <QSplitter>
#include "MainWindow.h"
#include "DocumentWidget.h"
#include "TextArea.h"
#include "preferences.h"
#include "TextBuffer.h"
#include "nedit.h"
#include "Color.h"
#include "highlight.h"

//------------------------------------------------------------------------------
// Name: DocumentWidget
//------------------------------------------------------------------------------
DocumentWidget::DocumentWidget(const QString &name, MainWindow *window, QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f) {

	buffer_ = new TextBuffer();

	// create the text widget
	auto layout   = new QBoxLayout(QBoxLayout::TopToBottom);
	auto splitter = new QSplitter(Qt::Vertical, this);

#if 1

	int P_marginWidth  = 5;
	int P_marginHeight = 5;
	int lineNumCols    = 0;
	int marginWidth    = 0;
	int charWidth      = 0;
	int l = P_marginWidth + ((lineNumCols == 0) ? 0 : marginWidth + charWidth * lineNumCols);
	int h = P_marginHeight;

	auto area = new TextArea(this,
							 l,
							 h,
							 100,
							 100,
							 0,
							 0,
							 buffer_,
							 QFont(tr("Monospace"), 12),
							 Qt::white,
							 Qt::black,
							 Qt::white,
							 Qt::blue,
							 Qt::black, //QColor highlightFGPixel,
							 Qt::black, //QColor highlightBGPixel,
							 Qt::black, //QColor cursorFGPixel,
							 Qt::black, //QColor lineNumFGPixel,
							 Qt::black, //QColor calltipFGPixel,
							 Qt::black //QColor calltipBGPixel;
							 );

	Pixel textFgPix   = AllocColor(GetPrefColorName(TEXT_FG_COLOR));
	Pixel textBgPix   = AllocColor(GetPrefColorName(TEXT_BG_COLOR));
	Pixel selectFgPix = AllocColor(GetPrefColorName(SELECT_FG_COLOR));
	Pixel selectBgPix = AllocColor(GetPrefColorName(SELECT_BG_COLOR));
	Pixel hiliteFgPix = AllocColor(GetPrefColorName(HILITE_FG_COLOR));
	Pixel hiliteBgPix = AllocColor(GetPrefColorName(HILITE_BG_COLOR));
	Pixel lineNoFgPix = AllocColor(GetPrefColorName(LINENO_FG_COLOR));
	Pixel cursorFgPix = AllocColor(GetPrefColorName(CURSOR_FG_COLOR));

	area->TextDSetColors(
		toQColor(textFgPix),
		toQColor(textBgPix),
		toQColor(selectFgPix),
		toQColor(selectBgPix),
		toQColor(hiliteFgPix),
		toQColor(hiliteBgPix),
		toQColor(lineNoFgPix),
		toQColor(cursorFgPix));

	splitter->addWidget(area);

	// add focus, drag, cursor tracking, and smart indent callbacks
#if 0
	textD->addCursorMovementCallback(movedCB, window);
	textD->addDragStartCallback(dragStartCB, window);
	textD->addDragEndCallback(dragEndCB, window);
	textD->addsmartIndentCallback(SmartIndentCB, window);
#endif

	connect(area, SIGNAL(focusIn(QWidget*)), window, SLOT(onFocusIn(QWidget*)));
	connect(area, SIGNAL(focusOut(QWidget*)), window, SLOT(onFocusOut(QWidget*)));


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

