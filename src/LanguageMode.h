
#ifndef LANGUAGE_MODE_H_
#define LANGUAGE_MODE_H_

#include "IndentStyle.h"
#include "WrapStyle.h"
#include <QString>
#include <QStringList>

constexpr auto PLAIN_LANGUAGE_MODE = static_cast<size_t>(-1);

/* indicates, that an unknown (i.e. not existing) language mode
   is bound to an user menu item */
constexpr auto UNKNOWN_LANGUAGE_MODE = static_cast<size_t>(-2);

class LanguageMode {
public:
	/*
	** supplement wrap and indent styles w/ a value meaning "use default" for
	** the override fields in the language modes dialog
	*/
	static constexpr int DEFAULT_TAB_DIST    = -1;
	static constexpr int DEFAULT_EM_TAB_DIST = -1;
	static constexpr int DEFAULT_INSERT_TABS = -1;

public:
	QString name;
	QStringList extensions;
	QString recognitionExpr;
	QString defTipsFile;
	QString delimiters;
	WrapStyle wrapStyle     = WrapStyle::Default;
	IndentStyle indentStyle = IndentStyle::Default;
	int tabDist             = DEFAULT_TAB_DIST;
	int emTabDist           = DEFAULT_EM_TAB_DIST;
	int insertTabs          = DEFAULT_INSERT_TABS;
};

#endif
