
#ifndef SEARCH_TYPE_H_
#define SEARCH_TYPE_H_

/*
** Schwarzenberg: added SEARCH_LITERAL_WORD .. SEARCH_REGEX_NOCASE
**
** The order of the integers in this enumeration must be exactly
** the same as the order of the coressponding strings of the
** array  SearchMethodStrings defined in preferences.cpp (!!)
**
*/
enum SearchType {
	SEARCH_LITERAL,
	SEARCH_CASE_SENSE,
	SEARCH_REGEX,
	SEARCH_LITERAL_WORD,
	SEARCH_CASE_SENSE_WORD,
	SEARCH_REGEX_NOCASE,
	N_SEARCH_TYPES /* must be last in enum SearchType */
};

#endif
