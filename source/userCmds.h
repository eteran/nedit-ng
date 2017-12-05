
#ifndef USER_CMDS_H_
#define USER_CMDS_H_

#include <vector>

struct MenuItem;
struct MenuData;

class QString;

// types of current dialog and/or menu
enum class DialogTypes {
    SHELL_CMDS,
    MACRO_CMDS,
    BG_MENU_CMDS
};

int LoadBGMenuCmdsStringEx(const QString &inString);
int LoadMacroCmdsStringEx(const QString &inString);
int LoadShellCmdsStringEx(const QString &inString);
QString WriteBGMenuCmdsStringEx();
QString WriteMacroCmdsStringEx();
QString WriteShellCmdsStringEx();
void SetupUserMenuInfo();
void UpdateUserMenuInfo();
void parseMenuItemList(std::vector<MenuData> &itemList);
MenuData *findMenuItem(const QString &name, DialogTypes type);

extern std::vector<MenuData> ShellMenuData;
extern std::vector<MenuData> BGMenuData;
extern std::vector<MenuData> MacroMenuData;

#endif
