
#ifndef LANGUAGE_MODE_H_
#define LANGUAGE_MODE_H_

#include "IndentStyle.h"
#include "WrapStyle.h"
#include <QString>
#include <QStringList>

class LanguageMode {
public:
	/* 
	** suplement wrap and indent styles w/ a value meaning "use default" for
    ** the override fields in the language modes dialog
	*/
    static constexpr int DEFAULT_TAB_DIST    = -1;
    static constexpr int DEFAULT_EM_TAB_DIST = -1;
	
public:
	LanguageMode();
	LanguageMode(const LanguageMode &)            = default;
	LanguageMode& operator=(const LanguageMode &) = default;
	LanguageMode(LanguageMode &&)                 = default;
	LanguageMode& operator=(LanguageMode &&)      = default;
	
public:
	void swap(LanguageMode &other);
	
public:
	QString     name;
	QStringList extensions;
	QString     recognitionExpr;
	QString     defTipsFile;
	QString     delimiters;
	int         wrapStyle;
    IndentStyle indentStyle;
	int         tabDist;
	int         emTabDist;
};

#endif
