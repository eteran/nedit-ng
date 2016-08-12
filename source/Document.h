
#ifndef DOCUMENT_H_
#define DOCUMENT_H_

#include <QPointer>
#include <QString>
#include <list>
#include <tuple>
#include "nedit.h"
#include "UserBGMenuCache.h"
#include "util/fileUtils.h"
#include "calltips.h"
#include "util/string_view.h"
#include "LockReasons.h"
#include "Bookmark.h"
#include <Xm/Xm.h>

class QDialog;
class DialogReplace;
class DialogReplaceScope;
class DialogColors;
class UndoInfo;
class TextBuffer;
class TextDisplay;
class TextArea;


/* The Document structure holds the information on a Document. A number
   of 'tabbed' documents may reside within a shell window, hence some of
   its members are of 'shell-level'; namely the find/replace dialogs, the
   menu bar & its associated members, the components on the stats area
   (i-search line, statsline and tab bar), plus probably a few olock_thers.
   See CreateWindow() and CreateDocument() for more info.

   Each document actually 'lives' within its splitPane widget member,
   which can be raised to become the 'top' (visible) document by function
   RaiseDocument(). The non-top documents may still be accessed through
   macros, or the context menu on the tab bar.

   Prior to the introduction of tabbed mode, each window may house only
   one document, making it effectively an 'editor window', hence the name
   Document. This struct name has been preserved to ease the transition
   when tabbed mode was introduced after NEdit 5.4.
*/
class Document {
public:
	Document(const QString &name, char *geometry, bool iconic);
	virtual ~Document();
	
private:
	Document(const Document &) = default;
	Document& operator=(const Document &) = default;
	Document(Document &&) = default;
	Document& operator=(Document &&) = default;

public:
	Document *CreateDocument(const QString &name);
	Document *DetachDocument();
	Document *MarkActiveDocument();
	Document *MarkLastDocument();
	Document *MoveDocument(Document *toWindow);
	
public:
	bool IsTopDocument() const;
	bool CloseAllDocumentInWindow();
	bool GetShowTabBar();
	bool IsIconic();
	bool IsValidWindow();
	int TabCount() const;
	void CleanUpTabBarExposeQueue();
	void ClearModeMessage();
	void ClosePane();
	void CloseWindow();	
	void LastDocument() const;
	void MakeSelectionVisible(Widget textPane);
	void MoveDocumentDialog();
	void NextDocument();
	void PreviousDocument();
	void RaiseDocument();
	void RaiseDocumentWindow();
	void RaiseFocusDocumentWindow(Boolean focus);
	void RefreshMenuToggleStates();
	void RefreshTabState();
	void RefreshWindowStates();
	void SetAutoIndent(int state);
	void SetAutoScroll(int margin);
	void SetAutoWrap(int state);
	void SetBacklightChars(char *applyBacklightTypes);
	void SetColors(const char *textFg, const char *textBg, const char *selectFg, const char *selectBg, const char *hiliteFg, const char *hiliteBg, const char *lineNoFg, const char *cursorFg);
	void SetEmTabDist(int emTabDist);
	void SetFonts(const char *fontName, const char *italicName, const char *boldName, const char *boldItalicName);
	void SetModeMessage(const char *message);
	void SetOverstrike(bool overstrike);
	void SetSensitive(Widget w, Boolean sensitive) const;
	void SetShowMatching(int state);
	void SetTabDist(int tabDist);
	void SetToggleButtonState(Widget w, Boolean state, Boolean notify) const;	
	void SetWindowModified(bool modified);
	void ShowISearchLine(bool state);
	void ShowLineNumbers(bool state);
	void ShowStatsLine(int state);
	void ShowTabBar(int state);
	void ShowWindowTabBar();
	void SortTabBar();
	void SplitPane();
	void TempShowISearch(int state);
	void UpdateMinPaneHeights();
	void UpdateNewOppositeMenu(int openInTab);
	void UpdateStatsLine();
	void UpdateWindowReadOnly();
	void UpdateWindowTitle();
	void UpdateWMSizeHints();
	Widget GetPaneByIndex(int index) const;
	int WidgetToPaneIndex(Widget w) const;
	void EditCustomTitleFormat();
	QString FullPath() const;	
	
public:
	void Undo();
	void Redo();
	void SaveUndoInformation(int pos, int nInserted, int nDeleted, view::string_view deletedText);
	void ClearUndoList();
	void ClearRedoList();
	
public:
	int GetCalltipID(int calltipID);
	int ShowCalltip(view::string_view text, bool anchored, int pos, int hAlign, int vAlign, int alignMode);
	void KillCalltip(int calltipID);

public:
	static Document *GetTopDocument(Widget w);
	static Document *WidgetToWindow(Widget w);
	static Document *TabToWindow(Widget tab);
	static Document *FindWindowWithFile(const QString &name, const QString &path);
	static int WindowCount();
	
public:
	int updateLineNumDisp();
	void getGeometryString(char *geomString);	

private:
	Document *getNextTabWindow(int direction, int crossWin, int wrap);
	void showStatistics(int state);
	void showISearch(int state);
	void showStatsForm();
	void addToWindowList();
	void removeFromWindowList();
	void refreshMenuBar();
	int updateGutterWidth();
	void cloneDocument(Document *window);
	void deleteDocument();
	void getTextPaneDimension(int *nRows, int *nCols);
	void trimUndoList(int maxLength);
	void appendDeletedText(view::string_view deletedText, int deletedLen, int direction);
	void removeRedoItem();
	void removeUndoItem();
	void addRedoItem(UndoInfo *redo);
	void addUndoItem(UndoInfo *undo);
	void showTabBar(int state);

public:
	static std::tuple<Widget, TextArea *> createTextArea(Widget parent, Document *window, int rows, int cols, int emTabDist, char *delimiters, int wrapMargin, int lineNumCols);

public:
	DialogReplace *getDialogReplace() const;
	
	TextDisplay *lastFocus() const;

	// NOTE(eteran): I think the following are the "shell" level members
	//               essentially the variables associated with the window
	//               but not a particular tab
	// TODO(eteran): separate this out in a new class which owns the tabs/documents!
public:
	QPointer<QDialog> dialogFind_;
	QPointer<QDialog> dialogReplace_;
	bool              showLineNumbers_; // is the line number display shown
	bool              showStats_;       // is stats line supposed to be shown
	bool              showISearchLine_; // is incr. search line to be shown

	// NOTE(eteran): these appear to be the document/tab specific variables
public:
	Widget shell_;                /* application shell of window */
	Widget mainWin_;              /* main window of shell */
	Widget splitPane_;            /* paned win. for splitting text area */
	Widget textArea_;             /* the first text editing area widget */
	QVector<Widget> textPanes_;   /* additional ones created on demand */
	Widget lastFocus_;            /* the last pane to have kbd. focus */
	Widget statsLine_;            /* file stats information display */
	Widget statsLineForm_;
	Widget statsLineColNo_; /* Line/Column information display */
	Widget iSearchForm_;    /* incremental search line widgets */
	Widget iSearchFindButton_;
	Widget iSearchText_;
	Widget iSearchClearButton_;
	Widget iSearchRegexToggle_;
	Widget iSearchCaseToggle_;
	Widget iSearchRevToggle_;
	Widget menuBar_;     /* the main menu bar */
	Widget tabBar_;      /* tab bar for tabbed window */
	Widget tab_;         /* tab for this document */

	QPointer<QDialog> dialogColors_;
	QPointer<QDialog> dialogFonts_; /* nullptr, unless font dialog is up */

	Widget readOnlyItem_; /* menu bar settable widgets... */
	Widget autoSaveItem_;
	Widget saveLastItem_;
	Widget openSelItem_;
	Widget newOppositeItem_;
	Widget closeItem_;
	Widget printSelItem_;
	Widget undoItem_;
	Widget redoItem_;
	Widget cutItem_;
	Widget delItem_;
	Widget copyItem_;
	Widget lowerItem_;
	Widget upperItem_;
	Widget findSelItem_;
	Widget findAgainItem_;
	Widget replaceFindAgainItem_;
	Widget replaceAgainItem_;
	Widget gotoSelItem_;
	Widget langModeCascade_;
	Widget findDefItem_;
	Widget showTipItem_;
	Widget autoIndentOffItem_;
	Widget autoIndentItem_;
	Widget smartIndentItem_;
	Widget noWrapItem_;
	Widget newlineWrapItem_;
	Widget continuousWrapItem_;
	Widget statsLineItem_;
	Widget iSearchLineItem_;
	Widget lineNumsItem_;
	Widget showMatchingOffItem_;
	Widget showMatchingDelimitItem_;
	Widget showMatchingRangeItem_;
	Widget matchSyntaxBasedItem_;
	Widget overtypeModeItem_;
	Widget highlightItem_;
	Widget windowMenuPane_;
	Widget shellMenuPane_;
	Widget macroMenuPane_;
	Widget bgMenuPane_;
	Widget tabMenuPane_;
	Widget prevOpenMenuPane_;
	Widget prevOpenMenuItem_;
	Widget unloadTagsMenuPane_;
	Widget unloadTagsMenuItem_;
	Widget unloadTipsMenuPane_;
	Widget unloadTipsMenuItem_;
	Widget filterItem_;
	Widget autoIndentOffDefItem_;
	Widget autoIndentDefItem_;
	Widget smartIndentDefItem_;
	Widget autoSaveDefItem_;
	Widget saveLastDefItem_;
	Widget noWrapDefItem_;
	Widget newlineWrapDefItem_;
	Widget contWrapDefItem_;
	Widget showMatchingOffDefItem_;
	Widget showMatchingDelimitDefItem_;
	Widget showMatchingRangeDefItem_;
	Widget matchSyntaxBasedDefItem_;
	Widget highlightOffDefItem_;
	Widget highlightDefItem_;
	Widget backlightCharsItem_;
	Widget backlightCharsDefItem_;
	Widget searchDlogsDefItem_;
	Widget beepOnSearchWrapDefItem_;
	Widget keepSearchDlogsDefItem_;
	Widget searchWrapsDefItem_;
	Widget appendLFItem_;
	Widget sortOpenPrevDefItem_;
	Widget allTagsDefItem_;
	Widget smartTagsDefItem_;
	Widget reposDlogsDefItem_;
	Widget autoScrollDefItem_;
	Widget openInTabDefItem_;
	Widget tabBarDefItem_;
	Widget tabBarHideDefItem_;
	Widget toolTipsDefItem_;
	Widget tabNavigateDefItem_;
	Widget tabSortDefItem_;
	Widget statsLineDefItem_;
	Widget iSearchLineDefItem_;
	Widget lineNumsDefItem_;
	Widget pathInWindowsMenuDefItem_;
	Widget modWarnDefItem_;
	Widget modWarnRealDefItem_;
	Widget exitWarnDefItem_;
	Widget searchLiteralDefItem_;
	Widget searchCaseSenseDefItem_;
	Widget searchLiteralWordDefItem_;
	Widget searchCaseSenseWordDefItem_;
	Widget searchRegexNoCaseDefItem_;
	Widget searchRegexDefItem_;
#ifdef REPLACE_SCOPE
	Widget replScopeWinDefItem_;
	Widget replScopeSelDefItem_;
	Widget replScopeSmartDefItem_;
#endif
	Widget size24x80DefItem_;
	Widget size40x80DefItem_;
	Widget size60x80DefItem_;
	Widget size80x80DefItem_;
	Widget sizeCustomDefItem_;
	Widget cancelShellItem_;
	Widget learnItem_;
	Widget finishLearnItem_;
	Widget cancelMacroItem_;
	Widget replayItem_;
	Widget repeatItem_;
	Widget splitPaneItem_;
	Widget closePaneItem_;
	Widget detachDocumentItem_;
	Widget moveDocumentItem_;
	Widget contextMoveDocumentItem_;
	Widget contextDetachDocumentItem_;
	Widget bgMenuUndoItem_;
	Widget bgMenuRedoItem_;

public:
	QString filename_;                 /* name component of file being edited*/
	QString path_;                     /* path component of file being edited*/
	unsigned fileMode_;                /* permissions of file being edited */
	uid_t fileUid_;                    /* last recorded user id of the file */
	gid_t fileGid_;                    /* last recorded group id of the file */
	FileFormats fileFormat_;           /* whether to save the file straight (Unix format), or convert it to MS DOS style with \r\n line breaks */
	time_t lastModTime_;               /* time of last modification to file */
	dev_t device_;                     /* device where the file resides */
	ino_t inode_;                      /* file's inode  */
	std::list<UndoInfo *> undo_;       /* info for undoing last operation */
	std::list<UndoInfo *> redo_;       /* info for redoing last undone op */
	TextBuffer *buffer_;               /* holds the text being edited */
	int autoSaveCharCount_;            /* count of single characters typed since last backup file generated */
	int autoSaveOpCount_;              /* count of editing operations "" */
	int undoMemUsed_;                  /* amount of memory (in bytes) dedicated to the undo list */
	QString fontName_;                 /* names of the text fonts in use */
	QString italicFontName_;
	QString boldFontName_;
	QString boldItalicFontName_;
	XmFontList fontList_;           /* fontList for the primary font */
	XFontStruct *italicFontStruct_; /* fontStructs for highlighting fonts */
	XFontStruct *boldFontStruct_;
	XFontStruct *boldItalicFontStruct_;
	XtIntervalId flashTimeoutID_;   /* timer procedure id for getting rid of highlighted matching paren.  Non-zero val. means highlight is drawn */
	int flashPos_;                  /* position saved for erasing matching paren highlight (if one is drawn) */
	bool wasSelected_;              /* last selection state (for dim/undim of selection related menu items */
	bool filenameSet_;              /* is the window still "Untitled"? */
	bool fileChanged_;              /* has window been modified? */
	bool fileMissing_;              /* is the window's file gone? */
	LockReasons lockReasons_;       /* all ways a file can be locked */
	bool autoSave_;                 /* is autosave turned on? */
	bool saveOldVersion_;           /* keep old version in filename.bck */
	char indentStyle_;              /* whether/how to auto indent */
	char wrapMode_;                 /* line wrap style: NO_WRAP, NEWLINE_WRAP or CONTINUOUS_WRAP */
	bool overstrike_;               /* is overstrike mode turned on ? */
	char showMatchingStyle_;        /* How to show matching parens: NO_FLASH, FLASH_DELIMIT, or FLASH_RANGE */
	char matchSyntaxBased_;         /* Use syntax info to show matching */
	bool highlightSyntax_;          /* is syntax highlighting turned on? */
	bool backlightChars_;           /* is char backlighting turned on? */
	QString backlightCharTypes_;    /* what backlighting to use */
	bool modeMessageDisplayed_;     /* special stats line banner for learn and shell command executing modes */
	QString modeMessage_;           /* stats line banner content for learn and shell command executing modes */
	bool ignoreModify_;             /* ignore modifications to text area */
	bool windowMenuValid_;          /* is window menu up to date? */
	int rHistIndex_;
	int fHistIndex_;                /* history placeholders for */
	int iSearchHistIndex_;          /*   find and replace dialogs */
	int iSearchStartPos_;           /* start pos. of current incr. search */
	int iSearchLastBeginPos_;       /* beg. pos. last match of current i.s.*/
	int nMarks_;                    /* number of active bookmarks */
	XtIntervalId markTimeoutID_;    /* backup timer for mark event handler*/
	Bookmark markTable_[MAX_MARKS]; /* marked locations in window */
	void *highlightData_;           /* info for syntax highlighting */
	void *shellCmdData_;            /* when a shell command is executing, info. about it, otherwise, nullptr */
	void *macroCmdData_;            /* same for macro commands */
	void *smartIndentData_;         /* compiled macros for smart indent */
	Atom fileClosedAtom_;           /* Atom used to tell nc that the file is closed */
	int languageMode_;              /* identifies language mode currently selected in the window */
	bool multiFileReplSelected_;    /* selected during last multi-window replacement operation (history) */
	Document ** writableWindows_;   /* temporary list of writable windows, used during multi-file replacements */
	int nWritableWindows_;          /* number of elements in the list */
	bool multiFileBusy_;            /* suppresses multiple beeps/dialogs during multi-file replacements */
	bool replaceFailed_;            /* flags replacements failures during multi-file replacements */
	bool iSearchLastRegexCase_;     /* idem, for regex mode in incremental search bar */
	bool iSearchLastLiteralCase_;   /* idem, for literal mode */

	UserMenuCache *userMenuCache_;    /* cache user menus: */
	UserBGMenuCache userBGMenuCache_; /* shell & macro menu are shared over all "tabbed" documents, while each document has its own background menu. */
};



#endif
