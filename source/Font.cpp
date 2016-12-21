
#include "Font.h"
#include <QX11Info>
#include <X11/Xatom.h>

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
QFont toQFont(XFontStruct *fs) {
	unsigned long ret;  	  
    XGetFontProperty(fs, XA_FONT, &ret);
	const char *name = XGetAtomName(QX11Info::display(), (Atom)ret);	  

	QFont f;	  
	f.setRawName(QLatin1String(name));
	return f;

}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
XFontStruct *fromQFont(const QFont &font) {
    return XQueryFont(QX11Info::display(), font.handle());
}
