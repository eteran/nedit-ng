
#ifndef DOCUMENT_WIDGET_H_
#define DOCUMENT_WIDGET_H_

#include <QDialog>
#include <QPointer>
#include <QWidget>

#include "SearchDirection.h"
#include "SearchType.h"
#include "ShowMatchingStyle.h"
#include "ui_DocumentWidget.h"
#include "Bookmark.h"
#include "LockReasons.h"
#include "UserBGMenuCache.h"
#include "userCmds.h"
#include "util/FileFormats.h"

#include <Xm/Xm.h>

class UndoInfo;
class TextBuffer;
class Document;
class MainWindow;
class QFrame;
class QLabel;
class QSplitter;
class QTimer;
class TextArea;
struct dragEndCBStruct;
struct smartIndentCBStruct;
struct WindowHighlightData;
struct HighlightData;

class DocumentWidget : public QWidget {
	Q_OBJECT
	friend class MainWindow;
public:
	DocumentWidget(const QString &name, QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DocumentWidget();

private Q_SLOTS:
	void onFocusIn(QWidget *now);
	void onFocusOut(QWidget *was);
    void flashTimerTimeout();

public Q_SLOTS:
    void setLanguageMode(const QString &mode);
    void open(const char *fullpath);
    void bannerTimeoutProc();
    void findIncrAP(const QString &searchString, SearchDirection direction, SearchType searchType, bool searchWraps, bool isContinue);
    void findAP(const QString &searchString, SearchDirection direction, SearchType searchType, bool searchWraps);
    void replaceAP(const QString &searchString, const QString &replaceString, SearchDirection direction, SearchType searchType, bool searchWraps);
    void replaceFindAP(const QString &searchString, const QString &replaceString, SearchDirection direction, SearchType searchType, bool searchWraps);
    void replaceAllAP(const QString &searchString, const QString &replaceString, SearchType searchType);
    void replaceInSelAP(const QString &searchString, const QString &replaceString, SearchType searchType);
    void markAP(QChar ch);
    void gotoMarkAP(QChar label, bool extendSel);

public:
	void movedCallback(TextArea *area);
	void dragStartCallback(TextArea *area);
	void dragEndCallback(TextArea *area, dragEndCBStruct *data);
	void smartIndentCallback(TextArea *area, smartIndentCBStruct *data);
    void modifiedCallback(int pos, int nInserted, int nDeleted, int nRestyled, view::string_view deletedText);

public:
    static DocumentWidget *documentFrom(TextArea *area);

public:
    TextArea *createTextArea(TextBuffer *buffer);
    QList<TextArea *> textPanes() const;
    TextArea *firstPane() const;
	void SetWindowModified(bool modified);
	void RefreshTabState();
	void DetermineLanguageMode(bool forceNewDefaults);
	void SetLanguageMode(int mode, bool forceNewDefaults);
	int matchLanguageMode();
	void UpdateStatsLine(TextArea *area);
    void RaiseDocument();
    void documentRaised();
    void reapplyLanguageMode(int mode, bool forceDefaults);
    void SetAutoWrap(int state);
    void SetAutoIndent(int state);
    void SetEmTabDist(int emTabDist);
    void SetTabDist(int tabDist);
    bool IsTopDocument() const;
    void updateWindowMenu();
    QString getWindowsMenuEntry();
    void UpdateUserMenus();
    QMenu *createUserMenu(const QVector<MenuData> &menuData);
    MainWindow *toWindow() const;
    void UpdateMarkTable(int pos, int nInserted, int nDeleted);
    void StopHighlighting();
    void freeHighlightData(WindowHighlightData *hd);
    void freePatterns(HighlightData *patterns);
    QString GetWindowDelimiters() const;
    void DimSelectionDepUserMenuItems(bool sensitive);
    void dimSelDepItemsInMenu(QMenu *menuPane, const QVector<MenuData> &menuList, bool sensitive);
    void RaiseFocusDocumentWindow(bool focus);
    void RaiseDocumentWindow();
    void SaveUndoInformation(int pos, int nInserted, int nDeleted, view::string_view deletedText);
    void ClearRedoList();
    void ClearUndoList();
    void appendDeletedText(view::string_view deletedText, int deletedLen, int direction);
    void removeRedoItem();
    void removeUndoItem();
    void addRedoItem(UndoInfo *redo);
    void addUndoItem(UndoInfo *undo);
    void trimUndoList(int maxLength);
    void Undo();
    void Redo();
    bool CheckReadOnly() const;
    void MakeSelectionVisible(TextArea *textPane);
    void RemoveBackupFile();
    QString backupFileNameEx();
    void CheckForChangesToFileEx();
    QString FullPath() const;
    void UpdateWindowReadOnly();
    int cmpWinAgainstFile(const QString &fileName);
    void RevertToSaved();
    int WriteBackupFile();
    int SaveWindow();
    bool doSave();
    int SaveWindowAs(const char *newName, bool addWrap);
    void addWrapNewlines();
    bool writeBckVersion();
    bool bckError(const QString &errString, const QString &file);
    int fileWasModifiedExternally();
    int CloseFileAndWindow(int preResponse);
    void CloseWindow();
    int doOpen(const QString &name, const QString &path, int flags);
    void RefreshWindowStates();
    void refreshMenuBar();
    void RefreshMenuToggleStates();
    void executeNewlineMacroEx(smartIndentCBStruct *cbInfo);
    void SetShowMatching(ShowMatchingStyle state);
    int textPanesCount() const;
    void executeModMacro(smartIndentCBStruct *cbInfo);
    void actionClose(const QString &mode);
    bool includeFile(const QString &name);

public:
    static DocumentWidget *EditExistingFileEx(DocumentWidget *inWindow, const QString &name, const QString &path, int flags, char *geometry, int iconic, const char *languageMode, int tabbed, int bgOpen);

private:
	// TODO(eteran): are these dialog's per window or per text document?
	QPointer<QDialog> dialogColors_;
	QPointer<QDialog> dialogFonts_; /* nullptr, unless font dialog is up */	

public:
	Atom fileClosedAtom_;              // Atom used to tell nc that the file is closed
	Bookmark markTable_[MAX_MARKS];    // marked locations in window
	FileFormats fileFormat_;           // whether to save the file straight (Unix format), or convert it to MS DOS style with \r\n line breaks
	LockReasons lockReasons_;          // all ways a file can be locked
	QString backlightCharTypes_;       // what backlighting to use
	QString boldFontName_;
	QString boldItalicFontName_;
	QString filename_;                 // name component of file being edited
	QString fontName_;                 // names of the text fonts in use
	QString italicFontName_;
	QString path_;                     // path component of file being edited
	TextBuffer *buffer_;               // holds the text being edited

	XFontStruct *boldFontStruct_;
	XFontStruct *boldItalicFontStruct_;
	XFontStruct *italicFontStruct_;    // fontStructs for highlighting fonts
	XmFontList fontList_;              // fontList for the primary font

    QTimer *flashTimer_;               // timer for getting rid of highlighted matching paren.
    QMenu *contextMenu_;

	XtIntervalId markTimeoutID_;       // backup timer for mark event handler

	bool autoSave_;                    // is autosave turned on?
	bool backlightChars_;              // is char backlighting turned on?
	bool fileChanged_;                 // has window been modified?
	bool fileMissing_;                 // is the window's file gone?
	bool filenameSet_;                 // is the window still "Untitled"?
	bool highlightSyntax_;             // is syntax highlighting turned on?
	bool ignoreModify_;                // ignore modifications to text area
	bool modeMessageDisplayed_;        // special stats line banner for learn and shell command executing modes
	bool multiFileBusy_;               // suppresses multiple beeps/dialogs during multi-file replacements
	bool multiFileReplSelected_;       // selected during last multi-window replacement operation (history)
	bool overstrike_;                  // is overstrike mode turned on ?
	bool replaceFailed_;               // flags replacements failures during multi-file replacements
	bool saveOldVersion_;              // keep old version in filename.bck
	bool windowMenuValid_;             // is window menu up to date?
    QString modeMessage_;              // stats line banner content for learn and shell command executing modes
	char indentStyle_;                 // whether/how to auto indent
	char matchSyntaxBased_;            // Use syntax info to show matching
    ShowMatchingStyle showMatchingStyle_;           // How to show matching parens: NO_FLASH, FLASH_DELIMIT, or FLASH_RANGE
	char wrapMode_;                    // line wrap style: NO_WRAP, NEWLINE_WRAP or CONTINUOUS_WRAP
	dev_t device_;                     // device where the file resides
	gid_t fileGid_;                    // last recorded group id of the file
	ino_t inode_;                      // file's inode
	int autoSaveCharCount_;            // count of single characters typed since last backup file generated
	int autoSaveOpCount_;              // count of editing operations ""    
	int flashPos_;                     // position saved for erasing matching paren highlight (if one is drawn)
	int languageMode_;                 // identifies language mode currently selected in the window
	int nMarks_;                       // number of active bookmarks
	int undoMemUsed_;                  // amount of memory (in bytes) dedicated to the undo list
	std::list<UndoInfo *> redo_;       // info for redoing last undone op
	std::list<UndoInfo *> undo_;       // info for undoing last operation
	time_t lastModTime_;               // time of last modification to file
	uid_t fileUid_;                    // last recorded user id of the file
	unsigned fileMode_;                // permissions of file being edited
    void *highlightData_;              // info for syntax highlighting                                          // TODO(eteran): why void* ?
    void *macroCmdData_;               // same for macro commands                                               // TODO(eteran): why void* ?
    void *shellCmdData_;               // when a shell command is executing, info. about it, otherwise, nullptr // TODO(eteran): why void* ?
    void *smartIndentData_;            // compiled macros for smart indent                                      // TODO(eteran): why void* ?

private:
	QSplitter *splitter_;
	Ui::DocumentWidget ui;
};

#endif
