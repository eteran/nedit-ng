
#include "LanguageMode.h"
#include <algorithm>

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
LanguageMode::LanguageMode() : wrapStyle(0), indentStyle(NO_AUTO_INDENT), tabDist(DEFAULT_TAB_DIST), emTabDist(DEFAULT_EM_TAB_DIST) {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void LanguageMode::swap(LanguageMode &other) {
	using std::swap;
	
	swap(name,            other.name);
	swap(extensions,      other.extensions);
	swap(recognitionExpr, other.recognitionExpr);
	swap(defTipsFile,     other.defTipsFile);
	swap(delimiters,      other.delimiters);
	swap(wrapStyle,       other.wrapStyle);
	swap(indentStyle,     other.indentStyle);
	swap(tabDist,         other.tabDist);
	swap(emTabDist,       other.emTabDist);
}
