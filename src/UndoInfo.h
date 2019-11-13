
#ifndef UNDO_INFO_H_
#define UNDO_INFO_H_

#include "TextCursor.h"
#include <string>

/* The accumulated list of undo operations can potentially consume huge
   amounts of memory.  These tuning parameters determine how much undo infor-
   mation is retained.  Normally, the list is kept between UNDO_OP_LIMIT and
   UNDO_OP_TRIMTO in length (when the list reaches UNDO_OP_LIMIT, it is
   trimmed to UNDO_OP_TRIMTO then allowed to grow back to UNDO_OP_LIMIT). */

constexpr auto UNDO_OP_LIMIT  = 400u;
constexpr auto UNDO_OP_TRIMTO = 200u;

enum UndoTypes {
	UNDO_NOOP,
	ONE_CHAR_INSERT,
	ONE_CHAR_REPLACE,
	ONE_CHAR_DELETE,
	BLOCK_INSERT,
	BLOCK_REPLACE,
	BLOCK_DELETE
};

/* Record on undo list */
class UndoInfo {
public:
	explicit UndoInfo(UndoTypes undoType, TextCursor start, TextCursor end);
	UndoInfo(const UndoInfo &) = default;
	UndoInfo(UndoInfo &&)      = default;
	UndoInfo &operator=(const UndoInfo &) = default;
	UndoInfo &operator=(UndoInfo &&) = default;
	~UndoInfo()                      = default;

public:
	std::string oldText;
	UndoTypes type;
	TextCursor startPos;
	TextCursor endPos;
	bool inUndo          = false; // flag to indicate undo command on this record in progress. Redirects SaveUndoInfo to save the next modifications on the redo list instead of the undo list.
	bool restoresToSaved = false; // flag to indicate undoing this operation will restore file to last saved (unmodified) state
};

#endif
