
#ifndef USER_MENU_LIST_ELEMENT_H_
#define USER_MENU_LIST_ELEMENT_H_

#include <vector>
#include <Xm/Xm.h>

// cache user menus: manage mode of user menu list element
enum UserMenuManageMode {
	UMMM_UNMANAGE,     // user menu item is unmanaged
	UMMM_UNMANAGE_ALL, // user menu item is a sub menu and is completely unmanaged (including nested sub menus)
	UMMM_MANAGE,       // user menu item is managed; menu items of potential sub menu are (un)managed individually
	UMMM_MANAGE_ALL    // user menu item is a sub menu and is completely managed
};

struct UserMenuListElement;

typedef std::vector<UserMenuListElement*> UserMenuList;

// structure representing one user menu item
struct UserMenuListElement {
	UserMenuManageMode umleManageMode;     //current manage mode
	UserMenuManageMode umlePrevManageMode; // previous manage mode
	char *umleAccKeys;                     // accelerator keys of item
	bool umleAccLockPatchApplied;          // indicates, if accelerator lock patch is applied
	Widget umleMenuItem;                   // menu item represented by this element
	Widget umleSubMenuPane;                // holds menu pane, if item represents a sub menu
	UserMenuList *umleSubMenuList;         // elements of sub menu, if item represents a sub menu
};

#endif
