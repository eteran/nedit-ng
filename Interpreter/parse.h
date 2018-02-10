
#ifndef PARSE_H_
#define PARSE_H_

struct Program;
class QString;

Program *ParseMacroEx(const QString &expr, QString *message, int *stoppedAt);

#endif
