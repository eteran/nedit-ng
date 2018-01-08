
#include "UndoInfo.h"

UndoInfo::UndoInfo(UndoTypes undoType, int64_t start, int64_t end) : type(undoType), startPos(start), endPos(end) {
}
