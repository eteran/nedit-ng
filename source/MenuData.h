
#ifndef MENU_DATA_H_
#define MENU_DATA_H_

#include <memory>

struct MenuItem;
struct userMenuInfo;

struct MenuData {
    std::shared_ptr<MenuItem>     item;
    std::shared_ptr<userMenuInfo> info;
};

#endif
