
#ifndef DOCUMENT_INFO_H_
#define DOCUMENT_INFO_H_

#include "UndoInfo.h"
#include "TextBufferFwd.h"
#include "ShowMatchingStyle.h"
#include "IndentStyle.h"
#include "WrapStyle.h"
#include "SmartIndent.h"
#include "LockReasons.h"
#include "Util/FileFormats.h"
#include <QString>
#include <QtGlobal>
#include <deque>
#include <memory>

#ifdef Q_OS_MACOS
#include <unistd.h>
#include <sys/stat.h>
#endif

struct DocumentInfo {
	QString filename;                                  // name component of file being edited
	QString path;                                      // path component of file being edited
	std::deque<UndoInfo> redo;                         // info for redoing last undone op
	std::deque<UndoInfo> undo;                         // info for undoing last operation
	LockReasons lockReasons;                           // all ways a file can be locked
	std::unique_ptr<SmartIndentData> smartIndentData;  // compiled macros for smart indent

#ifdef Q_OS_UNIX
	uid_t uid             = 0;                         // last recorded user id of the file
	gid_t gid             = 0;                         // last recorded group id of the file
	mode_t mode           = 0;                         // permissions of file being edited
#elif defined(Q_OS_WIN)
	// copied from the Windows version of the struct stat
	unsigned short mode   = 0;
	short          uid    = 0;
	short          gid    = 0;
#endif

	FileFormats fileFormat  = FileFormats::Unix;        // whether to save the file straight (Unix format), or convert it to MS DOS style with \r\n line breaks
	time_t lastModTime      = 0;                        // time of last modification to file
	dev_t dev               = 0;                        // device where the file resides
	ino_t ino               = 0;                        // file's inode
	std::unique_ptr<TextBuffer> buffer;                 // holds the text being edited
	int autoSaveCharCount   = 0;                        // count of single characters typed since last backup file generated
	int autoSaveOpCount     = 0;                        // count of editing operations
	bool filenameSet        = false;                    // is the window still "Untitled"?
	bool fileChanged        = false;                    // has window been modified?
	bool autoSave           = false;                    // is autosave turned on?
	bool saveOldVersion     = false;                    // keep old version in filename.bc
	bool overstrike         = false;                    // is overstrike mode turned on ?
	bool fileMissing        = true;                     // is the window's file gone?
	bool matchSyntaxBased   = false;                    // Use syntax info to show matching
	bool wasSelected        = false;                    // last selection state (for dim/undim of selection related menu items
	bool ignoreModify       = false;                    // ignore modifications to text area
	WrapStyle wrapMode      = WrapStyle::Default;       // line wrap style: None, Newline or Continuous
	IndentStyle indentStyle = IndentStyle::Default;     // whether/how to auto indent
	ShowMatchingStyle showMatchingStyle = ShowMatchingStyle::None; // How to show matching parens: None, Delimeter, or Range
};

#endif
