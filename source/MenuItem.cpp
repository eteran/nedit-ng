
#include "MenuItem.h"

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
MenuItem::MenuItem() : modifiers(0), keysym(0), mnemonic('\0'), input(FROM_NONE), output(TO_SAME_WINDOW), repInput(false), saveFirst(false), loadAfter(false) {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
MenuItem::MenuItem(const MenuItem &other) : name(other.name), modifiers(other.modifiers), keysym(other.keysym), mnemonic(other.mnemonic), input(other.input), output(other.output), repInput(other.repInput), saveFirst(other.saveFirst), loadAfter(other.loadAfter), cmd(other.cmd) {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
MenuItem& MenuItem::operator=(const MenuItem &rhs) {
	MenuItem(rhs).swap(*this);
	return *this;
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
