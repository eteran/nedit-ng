
#include "MenuItem.h"
#include <algorithm>

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
MenuItem::MenuItem() : modifiers(0), keysym(0), mnemonic('\0'), input(FROM_NONE), output(TO_SAME_WINDOW), repInput(false), saveFirst(false), loadAfter(false) {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
MenuItem::~MenuItem() {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void MenuItem::swap(MenuItem &other) {
	using std::swap;

	swap(name,      other.name);
	swap(modifiers, other.modifiers);
	swap(keysym,    other.keysym);
	swap(mnemonic,  other.mnemonic);
	swap(input,     other.input);
	swap(output,    other.output);
	swap(repInput,  other.repInput);
	swap(saveFirst, other.saveFirst);
	swap(loadAfter, other.loadAfter);
	swap(cmd,       other.cmd);
}
