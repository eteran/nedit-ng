
#ifndef LANGUAGE_MODE_H_
#define LANGUAGE_MODE_H_

#include <QString>
#include <QStringList>
#include "WrapStyle.h"
#include "IndentStyle.h"

class LanguageMode {
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
	char *defTipsFile;
	char *delimiters;
	int wrapStyle;
	int indentStyle;
	int tabDist;
	int emTabDist;
};

#endif
