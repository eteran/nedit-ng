
#ifndef USER_COMMANDS_H_
#define USER_COMMANDS_H_

#include <vector>

struct MenuItem;
struct MenuData;

class QString;

// types of current dialog and/or menu
enum class CommandTypes {
	Shell,
	Macro,
	Context
};

void LoadContextMenuCommandsString(const QString &inString);
void LoadMacroCommandsString(const QString &inString);
void LoadShellCommandsString(const QString &inString);
QString WriteContextMenuCommandsString();
QString WriteMacroCommandsString();
QString WriteShellCommandsString();
void SetupUserMenuInfo();
void UpdateUserMenuInfo();
void ParseMenuItemList(std::vector<MenuData> &itemList);
MenuData *FindMenuItem(const QString &name, CommandTypes type);

extern std::vector<MenuData> ShellMenuData;
extern std::vector<MenuData> BGMenuData;
extern std::vector<MenuData> MacroMenuData;

#endif
