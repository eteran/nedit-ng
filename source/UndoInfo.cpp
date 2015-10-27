
#include "UndoInfo.h"
#include <Xm/Xm.h>
#include <Xm/Text.h>

UndoInfo::UndoInfo() {

}

UndoInfo::~UndoInfo() {
	XtFree(oldText);
}

#if 0
UndoInfo::UndoInfo(const UndoInfo &other) : type(other.type), startPos(other.startPos), endPos(other.endPos), oldLen(other.oldLen), oldText(other.oldText), inUndo(other.inUndo), restoresToSaved(other.restoresToSaved) {
}

UndoInfo &UndoInfo::operator=(const UndoInfo &rhs) {
	if(this != &rhs) {
		UndoInfo(rhs).swap(*this);
	}
	return *this;
}

void UndoInfo::swap(UndoInfo &other) {
	using std::swap;
	
	swap(type,            other.type);
	swap(startPos,        other.startPos);
	swap(endPos,          other.endPos);
	swap(oldLen,          other.oldLen);
	swap(oldText,         other.oldText);
	swap(inUndo,          other.inUndo);
	swap(restoresToSaved, other.restoresToSaved);
}
#endif
