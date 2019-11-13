
#include "UndoInfo.h"

UndoInfo::UndoInfo(UndoTypes undoType, TextCursor start, TextCursor end)
	: type(undoType), startPos(start), endPos(end) {
}
