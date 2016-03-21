
#ifndef MENU_ITEM_H_
#define MENU_ITEM_H_

#include <QString>
#include <X11/keysym.h>

// Structure representing a menu item for shell, macro and BG menus
struct MenuItem {
	QString name;
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
