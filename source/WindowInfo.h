
#ifndef WINDOW_INFO_H_
#define WINDOW_INFO_H_

#include "nedit.h"
#include "preferences.h"
#include "../util/misc.h"
#include "../util/fileUtils.h"
#include <X11/Intrinsic.h>
#include <list>


struct TextBuffer;
struct UndoInfo;

/* The WindowInfo structure holds the information on a Document. A number
   of 'tabbed' documents may reside within a shell window, hence some of
   its members are of 'shell-level'; namely the find/replace dialogs, the
   menu bar & its associated members, the components on the stats area
   (i-search line, statsline and tab bar), plus probably a few others.
   See CreateWindow() and CreateDocument() for more info.

   Each document actually 'lives' within its splitPane widget member,
   which can be raised to become the 'top' (visible) document by function
   RaiseDocument(). The non-top documents may still be accessed through
   macros, or the context menu on the tab bar.

   Prior to the introduction of tabbed mode, each window may house only
   one document, making it effectively an 'editor window', hence the name
   WindowInfo. This struct name has been preserved to ease the transition
   when tabbed mode was introduced after NEdit 5.4.
*/
struct WindowInfo {
public:
	WindowInfo(const char *name, char *geometry, bool iconic);
	WindowInfo(const WindowInfo &) = default;
	WindowInfo& operator=(const WindowInfo &) = default;
	
public:
	bool IsTopDocument() const;
	Widget GetPaneByIndex(int paneIndex) const;	
	int CloseAllDocumentInWindow();
	int GetShowTabBar();
	int IsIconic();
	int IsValidWindow();
	int NDocuments();
	void LastDocument();
	void MakeSelectionVisible(Widget textPane);
	void MoveDocumentDialog();
	void NextDocument();
	void PreviousDocument();
	void RaiseDocument();
	void RaiseDocumentWindow();
	
	void CleanUpTabBarExposeQueue();
	void ClearModeMessage();
	void ClosePane();
	void CloseWindow();

public:
	WindowInfo *next;

public:
	Widget shell;                /* application shell of window */
	Widget mainWin;              /* main window of shell */
	Widget splitPane;            /* paned win. for splitting text area */
	Widget textArea;             /* the first text editing area widget */
	Widget textPanes[MAX_PANES]; /* additional ones created on demand */
	Widget lastFocus;            /* the last pane to have kbd. focus */
	Widget statsLine;            /* file stats information display */
	Widget statsLineForm;
	Widget statsLineColNo; /* Line/Column information display */
	Widget iSearchForm;    /* incremental search line widgets */
	Widget iSearchFindButton;
	Widget iSearchText;
	Widget iSearchClearButton;
	Widget iSearchRegexToggle;
	Widget iSearchCaseToggle;
	Widget iSearchRevToggle;
	Widget menuBar;     /* the main menu bar */
	Widget tabBar;      /* tab bar for tabbed window */
	Widget tab;         /* tab for this document */
	Widget replaceDlog; /* replace dialog */
	Widget replaceText; /* replace dialog settable widgets... */
	Widget replaceWithText;
	Widget replaceCaseToggle;
	Widget replaceWordToggle;
	Widget replaceRegexToggle;
	Widget replaceRevToggle;
	Widget replaceKeepBtn;
	Widget replaceBtns;
	Widget replaceBtn;
	Widget replaceAllBtn;
#ifndef REPLACE_SCOPE
	Widget replaceInWinBtn;
	Widget replaceInSelBtn;
#endif
	Widget replaceSearchTypeBox;
	Widget replaceFindBtn;
	Widget replaceAndFindBtn;
	Widget findDlog; /* find dialog */
	Widget findText; /* find dialog settable widgets... */
	Widget findCaseToggle;
	Widget findWordToggle;
	Widget findRegexToggle;
	Widget findRevToggle;
	Widget findKeepBtn;
	Widget findBtns;
	Widget findBtn;
	Widget findSearchTypeBox;
	Widget replaceMultiFileDlog; /* Replace in multiple files */
	Widget replaceMultiFileList;
	Widget replaceMultiFilePathBtn;
	Widget fontDialog;   /* NULL, unless font dialog is up */
	Widget colorDialog;  /* NULL, unless color dialog is up */
	Widget readOnlyItem; /* menu bar settable widgets... */
	Widget autoSaveItem;
	Widget saveLastItem;
	Widget openSelItem;
	Widget newOppositeItem;
	Widget closeItem;
	Widget printSelItem;
	Widget undoItem;
	Widget redoItem;
	Widget cutItem;
	Widget delItem;
	Widget copyItem;
	Widget lowerItem;
	Widget upperItem;
	Widget findSelItem;
	Widget findAgainItem;
	Widget replaceFindAgainItem;
	Widget replaceAgainItem;
	Widget gotoSelItem;
	Widget langModeCascade;
	Widget findDefItem;
	Widget showTipItem;
	Widget autoIndentOffItem;
	Widget autoIndentItem;
	Widget smartIndentItem;
	Widget noWrapItem;
	Widget newlineWrapItem;
	Widget continuousWrapItem;
	Widget statsLineItem;
	Widget iSearchLineItem;
	Widget lineNumsItem;
	Widget showMatchingOffItem;
	Widget showMatchingDelimitItem;
	Widget showMatchingRangeItem;
	Widget matchSyntaxBasedItem;
	Widget overtypeModeItem;
	Widget highlightItem;
	Widget windowMenuPane;
	Widget shellMenuPane;
	Widget macroMenuPane;
	Widget bgMenuPane;
	Widget tabMenuPane;
	Widget prevOpenMenuPane;
	Widget prevOpenMenuItem;
	Widget unloadTagsMenuPane;
	Widget unloadTagsMenuItem;
	Widget unloadTipsMenuPane;
	Widget unloadTipsMenuItem;
	Widget filterItem;
	Widget autoIndentOffDefItem;
	Widget autoIndentDefItem;
	Widget smartIndentDefItem;
	Widget autoSaveDefItem;
	Widget saveLastDefItem;
	Widget noWrapDefItem;
	Widget newlineWrapDefItem;
	Widget contWrapDefItem;
	Widget showMatchingOffDefItem;
	Widget showMatchingDelimitDefItem;
	Widget showMatchingRangeDefItem;
	Widget matchSyntaxBasedDefItem;
	Widget highlightOffDefItem;
	Widget highlightDefItem;
	Widget backlightCharsItem;
	Widget backlightCharsDefItem;
	Widget searchDlogsDefItem;
	Widget beepOnSearchWrapDefItem;
	Widget keepSearchDlogsDefItem;
	Widget searchWrapsDefItem;
	Widget appendLFItem;
	Widget sortOpenPrevDefItem;
	Widget allTagsDefItem;
	Widget smartTagsDefItem;
	Widget reposDlogsDefItem;
	Widget autoScrollDefItem;
	Widget openInTabDefItem;
	Widget tabBarDefItem;
	Widget tabBarHideDefItem;
	Widget toolTipsDefItem;
	Widget tabNavigateDefItem;
	Widget tabSortDefItem;
	Widget statsLineDefItem;
	Widget iSearchLineDefItem;
	Widget lineNumsDefItem;
	Widget pathInWindowsMenuDefItem;
	Widget modWarnDefItem;
	Widget modWarnRealDefItem;
	Widget exitWarnDefItem;
	Widget searchLiteralDefItem;
	Widget searchCaseSenseDefItem;
	Widget searchLiteralWordDefItem;
	Widget searchCaseSenseWordDefItem;
	Widget searchRegexNoCaseDefItem;
	Widget searchRegexDefItem;
#ifdef REPLACE_SCOPE
	Widget replScopeWinDefItem;
	Widget replScopeSelDefItem;
	Widget replScopeSmartDefItem;
#endif
	Widget size24x80DefItem;
	Widget size40x80DefItem;
	Widget size60x80DefItem;
	Widget size80x80DefItem;
	Widget sizeCustomDefItem;
	Widget cancelShellItem;
	Widget learnItem;
	Widget finishLearnItem;
	Widget cancelMacroItem;
	Widget replayItem;
	Widget repeatItem;
	Widget splitPaneItem;
	Widget closePaneItem;
	Widget detachDocumentItem;
	Widget moveDocumentItem;
	Widget contextMoveDocumentItem;
	Widget contextDetachDocumentItem;
	Widget bgMenuUndoItem;
	Widget bgMenuRedoItem;

	char filename[MAXPATHLEN];   /* name component of file being edited*/
	char path[MAXPATHLEN];       /* path component of file being edited*/
	unsigned fileMode;           /* permissions of file being edited */
	uid_t fileUid;               /* last recorded user id of the file */
	gid_t fileGid;               /* last recorded group id of the file */
	int fileFormat;              /* whether to save the file straight
	                                (Unix format), or convert it to
	            MS DOS style with \r\n line breaks */
	time_t lastModTime;          /* time of last modification to file */
	dev_t device;                /*  device where the file resides */
	ino_t inode;                 /*  file's inode  */
	std::list<UndoInfo *> undo;              /* info for undoing last operation */
	std::list<UndoInfo *> redo;              /* info for redoing last undone op */
	TextBuffer *buffer;          /* holds the text being edited */
	int nPanes;                  /* number of additional text editing
	                            areas, created by splitWindow */
	int autoSaveCharCount;       /* count of single characters typed
	                        since last backup file generated */
	int autoSaveOpCount;         /* count of editing operations "" */
	int undoMemUsed;             /* amount of memory (in bytes)
	                        dedicated to the undo list */
	char fontName[MAX_FONT_LEN]; /* names of the text fonts in use */
	char italicFontName[MAX_FONT_LEN];
	char boldFontName[MAX_FONT_LEN];
	char boldItalicFontName[MAX_FONT_LEN];
	XmFontList fontList;           /* fontList for the primary font */
	XFontStruct *italicFontStruct; /* fontStructs for highlighting fonts */
	XFontStruct *boldFontStruct;
	XFontStruct *boldItalicFontStruct;
	XtIntervalId flashTimeoutID;   /* timer procedure id for getting rid
	                      of highlighted matching paren.  Non-
	                      zero val. means highlight is drawn */
	int flashPos;                  /* position saved for erasing matching
	                              paren highlight (if one is drawn) */
	int wasSelected;               /* last selection state (for dim/undim
	                          of selection related menu items */
	bool filenameSet;           /* is the window still "Untitled"? */
	bool fileChanged;           /* has window been modified? */
	bool fileMissing;           /* is the window's file gone? */
	int lockReasons;               /* all ways a file can be locked */
	bool autoSave;              /* is autosave turned on? */
	bool saveOldVersion;        /* keep old version in filename.bck */
	char indentStyle;              /* whether/how to auto indent */
	char wrapMode;                 /* line wrap style: NO_WRAP,
	                                              NEWLINE_WRAP or CONTINUOUS_WRAP */
	bool overstrike;            /* is overstrike mode turned on ? */
	char showMatchingStyle;        /* How to show matching parens:
	                      NO_FLASH, FLASH_DELIMIT, or
	                      FLASH_RANGE */
	char matchSyntaxBased;         /* Use syntax info to show matching */
	bool showStats;             /* is stats line supposed to be shown */
	bool showISearchLine;       /* is incr. search line to be shown */
	bool showLineNumbers;       /* is the line number display shown */
	bool highlightSyntax;       /* is syntax highlighting turned on? */
	bool backlightChars;        /* is char backlighting turned on? */
	char *backlightCharTypes;      /* what backlighting to use */
	bool modeMessageDisplayed;  /* special stats line banner for learn
	                      and shell command executing modes */
	char *modeMessage;             /* stats line banner content for learn
	                          and shell command executing modes */
	bool ignoreModify;          /* ignore modifications to text area */
	bool windowMenuValid;       /* is window menu up to date? */
	int rHistIndex, fHistIndex;    /* history placeholders for */
	int iSearchHistIndex;          /*   find and replace dialogs */
	int iSearchStartPos;           /* start pos. of current incr. search */
	int iSearchLastBeginPos;       /* beg. pos. last match of current i.s.*/
	int nMarks;                    /* number of active bookmarks */
	XtIntervalId markTimeoutID;    /* backup timer for mark event handler*/
	Bookmark markTable[MAX_MARKS]; /* marked locations in window */
	void *highlightData;           /* info for syntax highlighting */
	void *shellCmdData;            /* when a shell command is executing,
	                                      info. about it, otherwise, NULL */
	void *macroCmdData;            /* same for macro commands */
	void *smartIndentData;         /* compiled macros for smart indent */
	Atom fileClosedAtom;           /* Atom used to tell nc that the file is closed */
	int languageMode;              /* identifies language mode currently
	                                      selected in the window */
	bool multiFileReplSelected; /* selected during last multi-window
	                  replacement operation (history) */
	struct WindowInfo **           /* temporary list of writable windows */
	    writableWindows;           /* used during multi-file replacements */
	int nWritableWindows;          /* number of elements in the list */
	Bool multiFileBusy;            /* suppresses multiple beeps/dialogs
	                      during multi-file replacements */
	Bool replaceFailed;            /* flags replacements failures during
	                      multi-file replacements */
	Bool replaceLastRegexCase;     /* last state of the case sense button
	                                      in regex mode for replace dialog */
	Bool replaceLastLiteralCase;   /* idem, for literal mode */
	Bool iSearchLastRegexCase;     /* idem, for regex mode in
	                                      incremental search bar */
	Bool iSearchLastLiteralCase;   /* idem, for literal mode */
	Bool findLastRegexCase;        /* idem, for regex mode in find dialog */
	Bool findLastLiteralCase;      /* idem, for literal mode */

#ifdef REPLACE_SCOPE
	int replaceScope;               /* Current scope for replace dialog */
	Widget replaceScopeWinToggle;   /* Scope for replace = window */
	Widget replaceScopeSelToggle;   /* Scope for replace = selection */
	Widget replaceScopeMultiToggle; /* Scope for replace = multiple files */
#endif
	UserMenuCache *userMenuCache;    /* cache user menus: */
	UserBGMenuCache userBGMenuCache; /* shell & macro menu are shared over all "tabbed" documents, while each document has its own background menu. */
};

#endif
