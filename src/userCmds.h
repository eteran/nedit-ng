
#ifndef USER_CMDS_H_
#define USER_CMDS_H_

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

bool load_bg_menu_commands_string(const QString &inString);
bool load_macro_commands_string(const QString &inString);
bool load_shell_commands_string(const QString &inString);
QString write_bg_menu_commands_string();
QString write_macro_commands_string();
QString write_shell_commands_string();
void setup_user_menu_info();
void update_user_menu_info();
void parse_menu_item_list(std::vector<MenuData> &itemList);
MenuData *find_menu_item(const QString &name, CommandTypes type);

extern std::vector<MenuData> ShellMenuData;
extern std::vector<MenuData> BGMenuData;
extern std::vector<MenuData> MacroMenuData;

#endif
