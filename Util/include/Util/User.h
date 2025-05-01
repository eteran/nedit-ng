
#ifndef UTIL_USER_H_
#define UTIL_USER_H_

class QString;

QString ExpandTilde(const QString &pathname);
QString GetHomeDir();
QString GetUser();
QString PrependHome(const QString &filename);
QString GetDefaultShell();
bool IsAdministrator();

#endif
