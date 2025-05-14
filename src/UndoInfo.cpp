
#include "UndoInfo.h"

/**
 * @brief Constructor for the UndoInfo class.
 *
 * @param undoType The type of undo operation.
 * @param start The starting position of the undo operation.
 * @param end The ending position of the undo operation.
 */
UndoInfo::UndoInfo(UndoTypes undoType, TextCursor start, TextCursor end)
	: type(undoType), startPos(start), endPos(end) {
}
