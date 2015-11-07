
#ifndef MOTIF_HELPER_H_
#define MOTIF_HELPER_H_

#include <Xm/Xm.h>
#include <string>

//------------------------------------------------------------------------------
inline XmString XmStringCreateSimpleEx(const char *text) {
	return XmStringCreateSimple(const_cast<char *>(text));
}

//------------------------------------------------------------------------------
inline XmString XmStringCreateLtoREx(const char *text, char *tag) {
	return XmStringCreateLtoR(const_cast<char *>(text), tag);
}

inline XmString XmStringCreateLtoREx(const std::string &text, char *tag) {
	return XmStringCreateLtoREx(text.c_str(), tag);
}

inline XmString XmStringCreateLtoREx(const char *text) {
	return XmStringCreateLtoR(const_cast<char *>(text), XmSTRING_DEFAULT_CHARSET);
}

inline XmString XmStringCreateLtoREx(const std::string &text) {
	return XmStringCreateLtoREx(text.c_str(), XmSTRING_DEFAULT_CHARSET);
}

//------------------------------------------------------------------------------

#endif
