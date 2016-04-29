
#ifndef LANGUAGE_MODE_H_
#define LANGUAGE_MODE_H_

#include <QString>
#include <QStringList>
#include "WrapStyle.h"
#include "IndentStyle.h"

class LanguageMode {
public:
	/* suplement wrap and indent styles w/ a value meaning "use default" for
    ** the override fields in the language modes dialog
	*/

	constexpr static const int DEFAULT_TAB_DIST    = -1;
	constexpr static const int DEFAULT_EM_TAB_DIST = -1;
	
public:
	LanguageMode();
	LanguageMode(const LanguageMode &other);
	LanguageMode& operator=(const LanguageMode &rhs);
	~LanguageMode();
	
public:
	void swap(LanguageMode &other);
	
public:
	QString name;
	int nExtensions;
	char **extensions;
	char *recognitionExpr;
	QString defTipsFile;
	char *delimiters;
	int wrapStyle;
	int indentStyle;
	int tabDist;
	int emTabDist;
};

#endif
