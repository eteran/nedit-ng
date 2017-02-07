
#ifndef MOTIF_HELPER_H_
#define MOTIF_HELPER_H_

#include <QString>
#include <X11/Intrinsic.h>


//------------------------------------------------------------------------------
inline String XtNewStringEx(const QString &string) {
	return XtNewString(string.toLatin1().data());
}




#endif
