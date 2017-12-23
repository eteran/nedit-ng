
#ifndef CLEARCASE_H_
#define CLEARCASE_H_

class QString;

namespace ClearCase {

QString GetVersionExtendedPath(const QString &fullname);
int GetVersionExtendedPathIndex(const QString &fullname);
QString GetViewTag();

}

#endif
