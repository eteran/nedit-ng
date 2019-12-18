
#ifndef MENU_ITEM_H_
#define MENU_ITEM_H_

#include <QKeySequence>
#include <QString>
#include <cstdint>
#include <memory>

// sources for command input and destinations for command output
enum InSrcs : uint8_t {
	FROM_SELECTION,
	FROM_WINDOW,
	FROM_EITHER,
	FROM_NONE,
};

enum OutDests : uint8_t {
	TO_SAME_WINDOW,
	TO_NEW_WINDOW,
	TO_DIALOG,
};

// Structure representing a menu item for shell, macro and BG menus
struct MenuItem {
	QString name;
	QKeySequence shortcut;
	InSrcs input    = FROM_NONE;
	OutDests output = TO_SAME_WINDOW;
	bool repInput   = false;
	bool saveFirst  = false;
	bool loadAfter  = false;
	QString cmd;
};

#endif
