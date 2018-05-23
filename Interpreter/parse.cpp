
#include "parse.h"

Program *ParseMacro(const QString &expr, QString *msg, int *stoppedAt);

Program *ParseMacroEx(const QString &expr, QString *msg, int *stoppedAt) {
    return ParseMacro(expr, msg, stoppedAt);
}
