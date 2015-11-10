
#ifndef HIGHLIGHT_PATTERN_H_
#define HIGHLIGHT_PATTERN_H_

/* Pattern specification structure */
struct highlightPattern {
	const char *name;
	char *startRE;
	char *endRE;
	char *errorRE;
	char *style;
	char *subPatternOf;
	int flags;
};


#endif
