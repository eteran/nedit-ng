
#ifndef MENU_ITEM_H_
#define MENU_ITEM_H_

#include <QString>
#include <algorithm>
#include <cstdint>
#include <X11/Intrinsic.h>
#include <X11/keysym.h>

// Structure representing a menu item for shell, macro and BG menus
class MenuItem {
public:
	MenuItem();
	MenuItem(const MenuItem &other);
	MenuItem& operator=(const MenuItem &rhs);
	~MenuItem();
	
public:
	void swap(MenuItem &other);

public:
	QString name;
	unsigned int modifiers;
	KeySym keysym;
	char mnemonic;
	uint8_t input;
	uint8_t output;
	bool repInput;
	bool saveFirst;
	bool loadAfter;
	char *cmd;
};

#endif
