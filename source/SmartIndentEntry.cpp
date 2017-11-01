
#include "SmartIndentEntry.h"

// NOTE(eteran): name is NOT included in the comparisons
bool SmartIndentEntry::operator==(const SmartIndentEntry &rhs) const {
	return (initMacro == rhs.initMacro) && (newlineMacro == rhs.newlineMacro) && (modMacro == rhs.modMacro);
}

bool SmartIndentEntry::operator!=(const SmartIndentEntry &rhs) const {
	return (initMacro != rhs.initMacro) || (newlineMacro != rhs.newlineMacro) || (modMacro != rhs.modMacro);
}

