
#ifndef HIGHLIGHT_STYLE_REC_H_
#define HIGHLIGHT_STYLE_REC_H_

#include <string>

struct highlightStyleRec {
public:
	highlightStyleRec();	
	~highlightStyleRec();

public:
	std::string name;
	char        *color;
	char        *bgColor;
	int          font;
};

#endif
