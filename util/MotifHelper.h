
#ifndef MOTIF_HELPER_H_
#define MOTIF_HELPER_H_

#include <QString>
#include <string>
#include <cassert>
#include <Xm/Xm.h>
#include <Xm/Text.h>


inline void XtCallActionProcEx(Widget widget, const char *action, XEvent* event, std::initializer_list<char *> params) {
	assert(params.size() < 10);
	
	char *args[10] = {};
	int i = 0;
	for(char *param: params) {
		args[i++] = param;
	}
	
	XtCallActionProc(widget, const_cast<String>(action), event, args, i);
	
}

template <class ...Args>
void XtCallActionProcEx(Widget widget, const char *action, XEvent* event, Args ... params) {
	XtCallActionProcEx(widget, action, event, {params...});
}

inline QString XmTextGetStringEx(Widget widget) {
	if(char *s = XmTextGetString(widget)) {
		QString str = QLatin1String(s);
		XtFree(s);
		return str;
	}
	
	return QString();
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

inline XmString XmStringCreateSimpleEx(QString text) {
	return XmStringCreateSimpleEx(text.toLatin1().data());
}

//------------------------------------------------------------------------------
inline XmString XmStringCreateLtoREx(const char *text, const char *tag) {
	return XmStringCreateLtoR(const_cast<char *>(text), const_cast<char *>(tag));
}


inline XmString XmStringCreateLtoREx(const std::string &text, const char *tag) {
	return XmStringCreateLtoREx(text.c_str(), tag);
}

inline XmString XmStringCreateLtoREx(const QString &text, const char *tag) {
	return XmStringCreateLtoREx(text.toLatin1().data(), tag);
}

//------------------------------------------------------------------------------
inline void XmTextSetStringEx(Widget widget, const char *value) {
	return XmTextSetString(widget, const_cast<char *>(value));
}


inline void XmTextSetStringEx(Widget widget, const QString &value) {
	return XmTextSetString(widget, value.toLatin1().data());
}


inline void XmTextSetStringEx(Widget widget, const std::string &value) {
	return XmTextSetStringEx(widget, value.c_str());
}


//------------------------------------------------------------------------------
inline String XtNewStringEx(QString string) {
	return XtNewString(string.toLatin1().data());
}


inline String XtNewStringEx(const std::string &string) {
	return XtNewString(const_cast<char *>(string.c_str()));
}


inline String XtNewStringEx(const char *string) {
	return XtNewString(const_cast<char *>(string));
}


#endif
