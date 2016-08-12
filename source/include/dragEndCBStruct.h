#ifndef DRAGENDCBSTRUCT_H_
#define DRAGENDCBSTRUCT_H_

struct dragEndCBStruct {
	int               startPos;
	int               nCharsDeleted;
	int               nCharsInserted;
	view::string_view deletedText;
};

#endif
