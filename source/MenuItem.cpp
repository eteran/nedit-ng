
#include "MenuItem.h"
#include <algorithm>

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
MenuItem::MenuItem() : input(FROM_NONE), output(TO_SAME_WINDOW), repInput(false), saveFirst(false), loadAfter(false) {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void MenuItem::swap(MenuItem &other) {
	using std::swap;

	swap(name,      other.name);
	swap(input,     other.input);
	swap(output,    other.output);
	swap(repInput,  other.repInput);
	swap(saveFirst, other.saveFirst);
	swap(loadAfter, other.loadAfter);
	swap(cmd,       other.cmd);
}
