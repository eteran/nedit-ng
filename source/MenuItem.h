
#ifndef MENU_ITEM_H_
#define MENU_ITEM_H_

// Structure representing a menu item for shell, macro and BG menus
struct MenuItem {
	XString name;
	unsigned int modifiers;
	KeySym keysym;
	char mnemonic;
	char input;
	char output;
	char repInput;
	char saveFirst;
	char loadAfter;
	char *cmd;
};

#endif
