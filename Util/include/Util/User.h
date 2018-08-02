
#ifndef UTIL_USER_H_
#define UTIL_USER_H_

class QString;

QString ExpandTilde(const QString &pathname);
QString GetHomeDir();
QString GetUserName();
QString PrependHome(const QString &filename);

#endif
