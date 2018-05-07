
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

bool LoadBGMenuCmdsStringEx(const QString &inString);
bool LoadMacroCmdsStringEx(const QString &inString);
bool LoadShellCmdsStringEx(const QString &inString);
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
