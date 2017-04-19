
#include "SmartIndentEntry.h"

bool SmartIndentEntry::operator==(const SmartIndentEntry &rhs) const {
	return (initMacro == rhs.initMacro) && (newlineMacro == rhs.newlineMacro) && (modMacro == rhs.modMacro);
}

bool SmartIndentEntry::operator!=(const SmartIndentEntry &rhs) const {
	return (initMacro != rhs.initMacro) || (newlineMacro != rhs.newlineMacro) || (modMacro != rhs.modMacro);
}

