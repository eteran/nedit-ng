
#include "SmartIndent.h"

bool SmartIndent::operator==(const SmartIndent &rhs) const {
	return (initMacro == rhs.initMacro) && (newlineMacro == rhs.newlineMacro) && (modMacro == rhs.modMacro);
}

bool SmartIndent::operator!=(const SmartIndent &rhs) const {
	return (initMacro != rhs.initMacro) || (newlineMacro != rhs.newlineMacro) || (modMacro != rhs.modMacro);
}

