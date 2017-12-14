
#ifndef UTILS_H_
#define UTILS_H_

class QString;
class QByteArray;

QString GetCurrentDirEx();
QString GetHomeDirEx();
QString GetNameOfHostEx();
QString GetUserNameEx();
QString PrependHomeEx(const QString &filename);
QString ErrorString(int error);
QByteArray loadResource(const QString &resource);

template <int (&F)(int), class Ch>
int safe_ctype (Ch c) {
    return F(static_cast<unsigned char>(c));
}

#endif
