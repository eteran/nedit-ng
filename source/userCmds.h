
#ifndef USER_CMDS_H_
#define USER_CMDS_H_

#include <vector>

struct MenuItem;
struct MenuData;

class QString;

// types of current dialog and/or menu
enum class CommandTypes {
	SHELL_CMDS,
	MACRO_CMDS,
	BG_MENU_CMDS
};

bool LoadBGMenuCmdsString(const QString &inString);
bool LoadMacroCmdsString(const QString &inString);
bool LoadShellCmdsString(const QString &inString);
QString WriteBGMenuCmdsString();
QString WriteMacroCmdsString();
QString WriteShellCmdsString();
void SetupUserMenuInfo();
void UpdateUserMenuInfo();
void parseMenuItemList(std::vector<MenuData> &itemList);
MenuData *findMenuItem(const QString &name, CommandTypes type);

extern std::vector<MenuData> ShellMenuData;
extern std::vector<MenuData> BGMenuData;
extern std::vector<MenuData> MacroMenuData;

#endif
