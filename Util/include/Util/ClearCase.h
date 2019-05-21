
#ifndef CLEARCASE_H_
#define CLEARCASE_H_

class QString;

namespace ClearCase {

QString GetVersionExtendedPath(const QString &fullname);
QString GetViewTag();
int     GetVersionExtendedPathIndex(const QString &fullname);

}

#endif
