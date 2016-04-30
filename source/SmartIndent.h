
#ifndef SMART_INDENT_H_
#define SMART_INDENT_H_

#include <QString>

class SmartIndent {
public:
	bool operator==(const SmartIndent &rhs) const;
	bool operator!=(const SmartIndent &rhs) const;
	
public:
	QString lmName;
	QString initMacro;
	QString newlineMacro;
	QString modMacro;
};

#endif
