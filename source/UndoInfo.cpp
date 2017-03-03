
#include "UndoInfo.h"

UndoInfo::UndoInfo(UndoTypes undoType, int start, int end) : type(undoType), startPos(start), endPos(end), inUndo(false), restoresToSaved(false) {
}
