
#include "UserMenuListElement.h"
#include "userCmds.h"

UserMenuListElement::UserMenuListElement(Widget menuItem, const QString &accKeys) {

	this->umleManageMode          = UMMM_UNMANAGE;
	this->umlePrevManageMode      = UMMM_UNMANAGE;
	this->umleAccKeys             = accKeys;
	this->umleAccLockPatchApplied = false;
	this->umleMenuItem            = menuItem;
	this->umleSubMenuPane         = nullptr;
	this->umleSubMenuList         = nullptr;
}

UserMenuListElement::~UserMenuListElement() {
	if (this->umleSubMenuList) {	
		freeUserMenuList(this->umleSubMenuList);
		delete this->umleSubMenuList;
	}
}

