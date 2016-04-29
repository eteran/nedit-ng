
#include "LanguageMode.h"
#include "MotifHelper.h"
#include <algorithm>

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
LanguageMode::LanguageMode() : nExtensions(0), extensions(nullptr), recognitionExpr(nullptr), wrapStyle(0), indentStyle(0), tabDist(DEFAULT_TAB_DIST), emTabDist(DEFAULT_EM_TAB_DIST) {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
LanguageMode::LanguageMode(const LanguageMode &other) {
	this->name        = other.name;
	this->nExtensions = other.nExtensions;
	this->extensions  = other.extensions ? new char *[other.nExtensions] : nullptr;
	
	for (int i = 0; i < other.nExtensions; i++) {
		this->extensions[i] = XtStringDup(other.extensions[i]);
	}
	
	this->recognitionExpr = other.recognitionExpr ? XtStringDup(other.recognitionExpr) : nullptr;
	this->defTipsFile     = other.defTipsFile;
	this->delimiters      = other.delimiters;
	this->wrapStyle       = other.wrapStyle;
	this->indentStyle     = other.indentStyle;
	this->tabDist         = other.tabDist;
	this->emTabDist       = other.emTabDist;
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
	XtFree(recognitionExpr);
	for (int i = 0; i < nExtensions; i++) {
		XtFree(extensions[i]);
	}

	delete [] extensions;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void LanguageMode::swap(LanguageMode &other) {
	using std::swap;
	
	std::swap(name, other.name);
	std::swap(nExtensions, other.nExtensions);
	std::swap(extensions, other.extensions);
	std::swap(recognitionExpr, other.recognitionExpr);
	std::swap(defTipsFile, other.defTipsFile);
	std::swap(delimiters, other.delimiters);
	std::swap(wrapStyle, other.wrapStyle);
	std::swap(indentStyle, other.indentStyle);
	std::swap(tabDist, other.tabDist);
	std::swap(emTabDist, other.emTabDist);
}
