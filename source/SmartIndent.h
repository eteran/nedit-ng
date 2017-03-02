
#ifndef SMART_INDENT_H_
#define SMART_INDENT_H_

#include <QString>

class SmartIndent {
public:
    SmartIndent()                               = default;
    SmartIndent(const SmartIndent &)            = default;
    SmartIndent& operator=(const SmartIndent &) = default;
    SmartIndent(SmartIndent &&)                 = default;
    SmartIndent& operator=(SmartIndent &&)      = default;

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
