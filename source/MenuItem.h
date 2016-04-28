
#ifndef MENU_ITEM_H_
#define MENU_ITEM_H_

#include <QString>
#include <algorithm>
#include <cstdint>
#include <X11/Intrinsic.h>
#include <X11/keysym.h>

/* sources for command input and destinations for command output */
enum InSrcs   : uint8_t { FROM_SELECTION, FROM_WINDOW, FROM_EITHER, FROM_NONE };
enum OutDests : uint8_t { TO_SAME_WINDOW, TO_NEW_WINDOW, TO_DIALOG };

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
	InSrcs input;
	OutDests output;
	bool repInput;
	bool saveFirst;
	bool loadAfter;
	QString cmd;
};

#endif
