
#include "UndoInfo.h"

UndoInfo::UndoInfo(UndoTypes undoType, int start, int end) : type(undoType), startPos(start), endPos(end), oldLen(0), inUndo(false), restoresToSaved(false) {
}

UndoInfo::~UndoInfo() {
}
