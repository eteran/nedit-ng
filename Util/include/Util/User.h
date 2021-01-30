
#ifndef UTIL_USER_H_
#define UTIL_USER_H_

class QString;

QString expandTilde(const QString &pathname);
QString getHomeDir();
QString getUserName();
QString prependHome(const QString &filename);
QString getDefaultShell();

#endif
