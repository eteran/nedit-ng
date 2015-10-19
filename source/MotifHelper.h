
#ifndef MOTIF_HELPER_H_
#define MOTIF_HELPER_H_

#include <Xm/Xm.h>

inline XmString XmStringCreateSimpleEx(const char * text) {
	return XmStringCreateSimple((char *)text);
}

#endif
