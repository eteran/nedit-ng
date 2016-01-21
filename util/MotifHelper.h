
#ifndef MOTIF_HELPER_H_
#define MOTIF_HELPER_H_

#include <Xm/Xm.h>
#include <Xm/Text.h>
#include <string>
#include <cassert>

inline std::string XmTextGetStringEx(Widget widget) {
	if(char *s = XmTextGetString(widget)) {
		std::string str(s);
		XtFree(s);
		return str;
	}
	
	return std::string();
}



//------------------------------------------------------------------------------
inline std::string XmStringGetLtoREx(XmString s, XmStringCharSet tag) {

	char *string;
	if(XmStringGetLtoR(s, tag, &string)) {
		std::string str(string);
		XtFree(string);
		return str;
	}
	
	return std::string();
}

//------------------------------------------------------------------------------
inline char *XtStringDup(const char *text) {
	assert(text);
	
	char *s = XtMalloc(strlen(text) + 1);
	if(s) {
		strcpy(s, text);
	}
	return s;
}

inline char *XtStringDup(const std::string &text) {
	char *s = XtMalloc(text.size() + 1);
	if(s) {
		strcpy(s, text.c_str());
	}	
	return s;
}

//------------------------------------------------------------------------------
inline XmString XmStringCreateSimpleEx(const char *text) {
	return XmStringCreateSimple(const_cast<char *>(text));
}

inline XmString XmStringCreateSimpleEx(const std::string &text) {
	return XmStringCreateSimpleEx(text.c_str());
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

inline void XmTextSetStringEx(Widget widget, const std::string &value) {
	return XmTextSetStringEx(widget, value.c_str());
}

//------------------------------------------------------------------------------
inline String XtNewStringEx(const std::string &string) {
	return XtNewString(const_cast<char *>(string.c_str()));
}

inline String XtNewStringEx(const char *string) {
	return XtNewString(const_cast<char *>(string));
}

//------------------------------------------------------------------------------
inline XmString XmStringCreateEx(const char *text, char *tag) {
	return XmStringCreate(const_cast<char *>(text), tag);
}

inline XmString XmStringCreateEx(const std::string &text, char *tag) {
	return XmStringCreateEx(text.c_str(), tag);
}

#endif
