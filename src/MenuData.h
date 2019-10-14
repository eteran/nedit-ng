
#ifndef MENU_DATA_H_
#define MENU_DATA_H_

#include "MenuItem.h"
#include <memory>
#include <vector>

/* Structure holding info about a single menu item.
   According to above example there exist 5 user menu items:
   a.) "menuItem1"  (hierarchical ID = {0} means: element nbr. "0" of main menu)
   b.) "menuItemA1" (hierarchical ID = {1, 0} means: el. nbr. "0" of
					 "subMenuA", which itself is el. nbr. "1" of main menu)
   c.) "menuItemA2" (hierarchical ID = {1, 1})
   d.) "menuItemB1" (hierarchical ID = {1, 2, 0})
   e.) "menuItemB2" (hierarchical ID = {1, 2, 1})
 */
struct UserMenuInfo {
	QString             umiName;          // hierarchical name of menu item (w.o. language mode info)
	bool                umiIsDefault;     // menu item is default one ("@*")
	std::vector<size_t> umiLanguageModes; // list of applicable lang. modes
	size_t              umiDefaultIndex;  // array index of menu item to be used as default, if no lang. mode matches
};

struct MenuData {
	MenuItem                      item;
	std::unique_ptr<UserMenuInfo> info;
};

#endif
