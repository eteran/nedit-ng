
#ifndef MOTIF_HELPER_H_
#define MOTIF_HELPER_H_

#include <Xm/Xm.h>
#include <Xm/Text.h>
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
inline XmString XmStringCreateLocalizedEx(const char *text) {
	return XmStringCreateLocalized(const_cast<char *>(text));
}

//------------------------------------------------------------------------------
inline void XmTextSetStringEx(Widget widget, const char *value) {
	return XmTextSetString(widget, const_cast<char *>(value));
}

//------------------------------------------------------------------------------
inline String XtNewStringEx(const std::string &string) {
	return XtNewString(const_cast<char *>(string.c_str()));
}

inline String XtNewStringEx(const char *string) {
	return XtNewString(const_cast<char *>(string));
}

#endif
