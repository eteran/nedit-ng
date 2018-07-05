
#ifndef SMART_INDENT_ENTRY_H_
#define SMART_INDENT_ENTRY_H_

#include <QString>

class SmartIndentEntry {
public:
    bool operator==(const SmartIndentEntry &rhs) const;
    bool operator!=(const SmartIndentEntry &rhs) const;

public:
    QString languageMode;
	QString initMacro;
	QString newlineMacro;
	QString modMacro;
};

#endif
