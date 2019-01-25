
#include "Util/String.h"
#include <QString>

/*
** If "string" is not terminated with a newline character, return a
** string which does end in a newline.
**
** (The macro language requires newline terminators for statements, but the
** text widget doesn't force it like the NEdit text buffer does, so this might
** avoid some confusion.)
*/
QString ensureNewline(const QString &string) {

	if(string.isNull()) {
		return QString();
	}

	if(string.endsWith(QLatin1Char('\n'))) {
		return string;
	}

	return string + QLatin1Char('\n');
}
