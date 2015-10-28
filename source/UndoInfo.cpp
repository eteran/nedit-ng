
#include "UndoInfo.h"

UndoInfo::UndoInfo(undoTypes undoType, int start, int end) : next(nullptr), type(undoType), startPos(start), endPos(end), oldLen(0), inUndo(false), restoresToSaved(false) {
}

UndoInfo::~UndoInfo() {
}
