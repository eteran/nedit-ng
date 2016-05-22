
#ifndef UNDO_INFO_H_
#define UNDO_INFO_H_

#include <string>


/* The accumulated list of undo operations can potentially consume huge
   amounts of memory.  These tuning parameters determine how much undo infor-
   mation is retained.  Normally, the list is kept between UNDO_OP_LIMIT and
   UNDO_OP_TRIMTO in length (when the list reaches UNDO_OP_LIMIT, it is
   trimmed to UNDO_OP_TRIMTO then allowed to grow back to UNDO_OP_LIMIT).
   When there are very large amounts of saved text held in the list,
   UNDO_WORRY_LIMIT and UNDO_PURGE_LIMIT take over and cause the list to
   be trimmed back further to keep its size down. */
#define UNDO_PURGE_LIMIT  15000000 /* If undo list gets this large (in bytes), trim it to length of UNDO_PURGE_TRIMTO */
#define UNDO_PURGE_TRIMTO 1        /* Amount to trim the undo list in a purge */
#define UNDO_WORRY_LIMIT  2000000  /* If undo list gets this large (in bytes), trim it to length of UNDO_WORRY_TRIMTO */
#define UNDO_WORRY_TRIMTO 5        /* Amount to trim the undo list when memory use begins to get serious */
#define UNDO_OP_LIMIT     400      /* normal limit for length of undo list */
#define UNDO_OP_TRIMTO    200      /* size undo list is normally trimmed to when it exceeds UNDO_OP_TRIMTO in length */

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
	UndoInfo(UndoTypes undoType, int start, int end);
	~UndoInfo();
	UndoInfo(const UndoInfo &) = default;
	UndoInfo(UndoInfo &&) = default;
	UndoInfo &operator=(const UndoInfo &) = default;
	UndoInfo &operator=(UndoInfo &&) = default;
	
public:
	UndoTypes type;
	int startPos;
	int endPos;
	int oldLen;
	std::string oldText;
	bool inUndo;          // flag to indicate undo command on this record in progress. Redirects SaveUndoInfo to save the next modifications on the redo list instead of the undo list.
	bool restoresToSaved; // flag to indicate undoing this operation will restore file to last saved (unmodified) state
};

#endif
