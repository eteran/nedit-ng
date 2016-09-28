
#ifndef FONT_H_
#define FONT_H_

#include <QFont>
#include <X11/Xlib.h>

QFont toQFont(XFontStruct *fs);
XFontStruct *fromQFont(const QFont &font);

#endif
