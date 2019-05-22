
#ifndef PARSE_H_
#define PARSE_H_

struct Program;
class QString;

Program *compileMacro(const QString &expr, QString *message, int *stoppedAt);
bool isMacroValid(const QString &expr, QString *message, int *stoppedAt);

#endif
