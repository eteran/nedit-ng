
#ifndef DOCUMENT_INFO_H_
#define DOCUMENT_INFO_H_

#include "IndentStyle.h"
#include "LockReasons.h"
#include "ShowMatchingStyle.h"
#include "SmartIndent.h"
#include "TextBufferFwd.h"
#include "UndoInfo.h"
#include "Util/FileFormats.h"
#include "WrapStyle.h"
#include <QString>
#include <QtGlobal>
#include <deque>
#include <memory>
#include <qplatformdefs.h>

#ifdef Q_OS_MACOS
#include <sys/stat.h>
#include <unistd.h>
#endif

struct DocumentInfo {
	QString filename;                                 // name component of file being edited
	QString path;                                     // path component of file being edited
	std::deque<UndoInfo> redo;                        // info for redoing last undone op
	std::deque<UndoInfo> undo;                        // info for undoing last operation
	LockReasons lockReasons;                          // all ways a file can be locked
	std::unique_ptr<SmartIndentData> smartIndentData; // compiled macros for smart indent

	QT_STATBUF statbuf = {}; // we care about MOST of the fields of this structure.
							 // So instead of trying to match the OS specific types, just use it

	FileFormats fileFormat = FileFormats::Unix;                    // whether to save the file straight (Unix format), or convert it to MS DOS style with \r\n line breaks
	std::shared_ptr<TextBuffer> buffer;                            // holds the text being edited
	int autoSaveCharCount               = 0;                       // count of single characters typed since last backup file generated
	int autoSaveOpCount                 = 0;                       // count of editing operations
	bool filenameSet                    = false;                   // is the window still "Untitled"?
	bool fileChanged                    = false;                   // has window been modified?
	bool autoSave                       = false;                   // is autosave turned on?
	bool saveOldVersion                 = false;                   // keep old version in filename.bc
	bool overstrike                     = false;                   // is overstrike mode turned on ?
	bool fileMissing                    = true;                    // is the window's file gone?
	bool matchSyntaxBased               = false;                   // Use syntax info to show matching
	bool wasSelected                    = false;                   // last selection state (for dim/undim of selection related menu items
	bool ignoreModify                   = false;                   // ignore modifications to text area
	WrapStyle wrapMode                  = WrapStyle::Default;      // line wrap style: None, Newline or Continuous
	IndentStyle indentStyle             = IndentStyle::Default;    // whether/how to auto indent
	ShowMatchingStyle showMatchingStyle = ShowMatchingStyle::None; // How to show matching parens: None, Delimeter, or Range
};

#endif
