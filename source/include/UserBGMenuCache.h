
#ifndef USER_BG_MENU_CACHE_H_
#define USER_BG_MENU_CACHE_H_

#include "UserMenuListElement.h"

/* structure holding cache info about Background menu, which is
   owned by each document individually (needed to manage/unmanage this
   user definable menu when language mode changes) */
struct UserBGMenuCache {
	int ubmcLanguageMode;      /* language mode applied for background user menu */
	bool ubmcMenuCreated;      /* indicating, if all background menu items were created */
	UserMenuList ubmcMenuList; /* list of all background menu items */
};

#endif
