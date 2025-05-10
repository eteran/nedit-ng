
#ifndef PARSE_H_
#define PARSE_H_

struct Program;
class QString;

Program *CompileMacro(const QString &expr, QString *message, int *stoppedAt);
bool IsMacroValid(const QString &expr, QString *message, int *stoppedAt);

#endif
