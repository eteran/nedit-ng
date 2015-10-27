
#ifndef UNDO_INFO_H_
#define UNDO_INFO_H_

enum undoTypes {
	UNDO_NOOP, 
	ONE_CHAR_INSERT, 
	ONE_CHAR_REPLACE, 
	ONE_CHAR_DELETE, 
	BLOCK_INSERT, 
	BLOCK_REPLACE, 
	BLOCK_DELETE
};

/* Record on undo list */
struct UndoInfo {
public:
	UndoInfo();
	~UndoInfo();
	
public:
	UndoInfo *next; /* pointer to the next undo record */
	undoTypes type;
	int startPos;
	int endPos;
	int oldLen;
	char *oldText;
	char inUndo;          /* flag to indicate undo command on this record in progress.  Redirects SaveUndoInfo to save the next modifications on the redo list instead of the undo list. */
	char restoresToSaved; /* flag to indicate undoing this operation will restore file to last saved (unmodified) state */
};

#endif
