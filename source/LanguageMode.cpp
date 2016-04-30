
#include "LanguageMode.h"
#include <algorithm>

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
LanguageMode::LanguageMode() : wrapStyle(0), indentStyle(0), tabDist(DEFAULT_TAB_DIST), emTabDist(DEFAULT_EM_TAB_DIST) {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
LanguageMode::LanguageMode(const LanguageMode &other) {
	name			= other.name;
	extensions  	= other.extensions;
	recognitionExpr = other.recognitionExpr;
	defTipsFile 	= other.defTipsFile;
	delimiters  	= other.delimiters;
	wrapStyle		= other.wrapStyle;
	indentStyle 	= other.indentStyle;
	tabDist 		= other.tabDist;
	emTabDist		= other.emTabDist;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
LanguageMode& LanguageMode::operator=(const LanguageMode &rhs) {
	LanguageMode(rhs).swap(*this);
	return *this;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
LanguageMode::~LanguageMode() {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void LanguageMode::swap(LanguageMode &other) {
	using std::swap;
	
	std::swap(name, other.name);
	std::swap(extensions, other.extensions);
	std::swap(recognitionExpr, other.recognitionExpr);
	std::swap(defTipsFile, other.defTipsFile);
	std::swap(delimiters, other.delimiters);
	std::swap(wrapStyle, other.wrapStyle);
	std::swap(indentStyle, other.indentStyle);
	std::swap(tabDist, other.tabDist);
	std::swap(emTabDist, other.emTabDist);
}
