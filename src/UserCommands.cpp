
#include "UserCommands.h"
#include "LanguageMode.h"
#include "MenuData.h"
#include "MenuItem.h"
#include "Preferences.h"
#include "Settings.h"
#include "Util/Input.h"
#include "Util/Raise.h"
#include "Util/Resource.h"
#include "Yaml.h"
#include "parse.h"

#include <QFileInfo>
#include <QTextStream>

#include <memory>
#include <optional>

#include <yaml-cpp/yaml.h>

/* Descriptions of the current user programmed menu items for re-generating
   menus and processing shell, macro, and background menu selections */
std::vector<MenuData> ShellMenuData;
std::vector<MenuData> MacroMenuData;
std::vector<MenuData> BGMenuData;

namespace {

/**
 * @brief Copy a macro to the end of the input stream.
 * Scan text from `in` to the end of macro input (matching brace),
 * advancing `in`, and return macro text as function return value.
 *
 * This is kind of wasteful in that it throws away the compiled macro,
 * to be re-generated from the text as needed, but compile time is
 * negligible for most macros.
 *
 * @param in The input stream to read from.
 * @return The copied macro.
 */
QString CopyMacroToEnd(Input &in) {

	Input input = in;

	// Skip over whitespace to find make sure there's a beginning brace
	// to anchor the parse (if not, it will take the whole file)
	input.skipWhitespaceNL();

	const QString code = input.mid();

	if (!code.startsWith(QLatin1Char('{'))) {
		Preferences::ReportError(
			nullptr,
			code,
			input.index() - in.index(),
			QLatin1String("macro menu item"),
			QStringLiteral("expecting '{'"));

		return QString();
	}

	// Parse the input
	int stoppedAt;
	QString errMsg;
	if (!IsMacroValid(code, &errMsg, &stoppedAt)) {
		Preferences::ReportError(
			nullptr,
			code,
			stoppedAt,
			QLatin1String("macro menu item"),
			errMsg);

		return QString();
	}

	// Copy and return the body of the macro, stripping outer braces and
	// extra leading tabs added by the writer routine
	++input;
	input.skipWhitespace();

	if (*input == QLatin1Char('\n')) {
		++input;
	}

	if (*input == QLatin1Char('\t')) {
		++input;
	}

	if (*input == QLatin1Char('\t')) {
		++input;
	}

	QString retStr;
	retStr.reserve(stoppedAt - input.index());

	auto retPtr = std::back_inserter(retStr);

	while (input.index() != stoppedAt + in.index()) {
		if (input.match(QStringLiteral("\n\t\t"))) {
			*retPtr++ = QLatin1Char('\n');
		} else {
			*retPtr++ = *input++;
		}
	}

	if (retStr.endsWith(QLatin1Char('\t'))) {
		retStr.chop(1);
	}

	// move past the trailing '}'
	++input;

	in = input;

	return retStr;
}

/**
 * @brief Creates a string for the menu items in YAML format.
 *
 * @param menuItems The menu items to be written to the YAML file.
 * @return The YAML string representation of the menu items.
 */
QString WriteMacroMenuYaml(const std::vector<MenuData> &menuItems) {
	const QString filename = Settings::MactoMenuFile();

	try {
		YAML::Emitter out;
		for (const MenuData &entry : menuItems) {

			const MenuItem &item = entry.item;

			out << YAML::BeginDoc;
			out << YAML::BeginMap;
			out << YAML::Key << "name" << YAML::Value << item.name.toUtf8().data();

			if (!item.shortcut.isEmpty()) {
				out << YAML::Key << "shortcut" << YAML::Value << item.shortcut.toString().toUtf8().data();
			}

			if (item.input == FROM_SELECTION) {
				out << YAML::Key << "input" << YAML::Value << "selection";
			}

			out << YAML::Key << "command" << YAML::Value << YAML::Literal << item.cmd.toUtf8().data();
			out << YAML::EndMap;
		}

		QFile file(filename);
		if (file.open(QIODevice::WriteOnly)) {
			file.write(out.c_str());
			file.write("\n");
		}

		return QStringLiteral("*");
	} catch (const YAML::Exception &ex) {
		qWarning("NEdit: Error writing %s in config directory:\n%s", qPrintable(filename), ex.what());
	}

	return QString();
}

/**
 * @brief Creates a string for the menu items in YAML format.
 *
 * @param menuItems The menu items to be written to the YAML file.
 * @return The YAML string representation of the menu items.
 */
QString WriteShellMenuYaml(const std::vector<MenuData> &menuItems) {
	const QString filename = Settings::ShellMenuFile();

	try {
		YAML::Emitter out;
		for (const MenuData &entry : menuItems) {

			const MenuItem &item = entry.item;

			out << YAML::BeginDoc;
			out << YAML::BeginMap;
			out << YAML::Key << "name" << YAML::Value << item.name.toUtf8().data();

			if (!item.shortcut.isEmpty()) {
				out << YAML::Key << "shortcut" << YAML::Value << item.shortcut.toString().toUtf8().data();
			}

			switch (item.input) {
			case FROM_SELECTION:
				out << YAML::Key << "input" << YAML::Value << "selection";
				break;
			case FROM_WINDOW:
				out << YAML::Key << "input" << YAML::Value << "window";
				break;
			case FROM_EITHER:
				out << YAML::Key << "input" << YAML::Value << "either";
				break;
			case FROM_NONE:
				out << YAML::Key << "input" << YAML::Value << "none";
				break;
			default:
				break;
			}

			switch (item.output) {
			case TO_DIALOG:
				out << YAML::Key << "output" << YAML::Value << "dialog";
				break;
			case TO_NEW_WINDOW:
				out << YAML::Key << "output" << YAML::Value << "new_window";
				break;
			case TO_SAME_WINDOW:
				out << YAML::Key << "output" << YAML::Value << "same_window";
				break;
			default:
				break;
			}

			if (item.repInput) {
				out << YAML::Key << "replace_input" << YAML::Value << true;
			}
			if (item.saveFirst) {
				out << YAML::Key << "save_first" << YAML::Value << true;
			}
			if (item.loadAfter) {
				out << YAML::Key << "load_after" << YAML::Value << true;
			}

			out << YAML::Key << "command" << YAML::Value << YAML::Literal << item.cmd.toUtf8().data();
			out << YAML::EndMap;
		}

		QFile file(filename);
		if (file.open(QIODevice::WriteOnly)) {
			file.write(out.c_str());
			file.write("\n");
		}

		return QStringLiteral("*");
	} catch (const YAML::Exception &ex) {
		qWarning("NEdit: Error writing %s in config directory:\n%s", qPrintable(filename), ex.what());
	}

	return QString();
}

/**
 * @brief Creates a string for the menu items in YAML format.
 *
 * @param menuItems The menu items to be written to the YAML file.
 * @return The YAML string representation of the menu items.
 */
QString WriteContextMenuYaml(const std::vector<MenuData> &menuItems) {
	const QString filename = Settings::ContextMenuFile();

	try {
		YAML::Emitter out;
		for (const MenuData &entry : menuItems) {

			const MenuItem &item = entry.item;

			out << YAML::BeginDoc;
			out << YAML::BeginMap;
			out << YAML::Key << "name" << YAML::Value << item.name.toUtf8().data();

			if (!item.shortcut.isEmpty()) {
				out << YAML::Key << "shortcut" << YAML::Value << item.shortcut.toString().toUtf8().data();
			}

			if (item.input == FROM_SELECTION) {
				out << YAML::Key << "input" << YAML::Value << "selection";
			}

			out << YAML::Key << "command" << YAML::Value << YAML::Literal << item.cmd.toUtf8().data();
			out << YAML::EndMap;
		}

		QFile file(filename);
		if (file.open(QIODevice::WriteOnly)) {
			file.write(out.c_str());
			file.write("\n");
		}

		return QStringLiteral("*");
	} catch (const YAML::Exception &ex) {
		qWarning("NEdit: Error writing %s in config directory:\n%s", qPrintable(filename), ex.what());
	}

	return QString();
}

/**
 * @brief Reads in a menu item from the input stream.
 *
 * @param in The input stream to read from.
 * @param listType The type of command list (Shell, Macro, Context).
 * @return The menu item read from the input stream, or an empty optional if there was an error.
 */
std::optional<MenuItem> ReadMenuItem(Input &in, CommandTypes listType) {

	struct ParseError {
		std::string message;
	};

	try {
		// read name field
		const QString nameStr = in.readUntil(QLatin1Char(':'));
		if (nameStr.isEmpty()) {
			Raise<ParseError>("no name field");
		}

		if (in.atEnd()) {
			Raise<ParseError>("end not expected");
		}

		++in;

		// read accelerator field
		const QString accStr = in.readUntil(QLatin1Char(':'));

		if (in.atEnd()) {
			Raise<ParseError>("end not expected");
		}

		++in;

		// read flags field
		InSrcs input    = FROM_NONE;
		OutDests output = TO_SAME_WINDOW;
		bool repInput   = false;
		bool saveFirst  = false;
		bool loadAfter  = false;

		for (; !in.atEnd() && *in != QLatin1Char(':'); ++in) {

			if (listType == CommandTypes::Shell) {

				switch ((*in).toLatin1()) {
				case 'I':
					input = FROM_SELECTION;
					break;
				case 'A':
					input = FROM_WINDOW;
					break;
				case 'E':
					input = FROM_EITHER;
					break;
				case 'W':
					output = TO_NEW_WINDOW;
					break;
				case 'D':
					output = TO_DIALOG;
					break;
				case 'X':
					repInput = true;
					break;
				case 'S':
					saveFirst = true;
					break;
				case 'L':
					loadAfter = true;
					break;
				default:
					Raise<ParseError>("unreadable flag field");
				}
			} else {
				switch ((*in).toLatin1()) {
				case 'R':
					input = FROM_SELECTION;
					break;
				default:
					Raise<ParseError>("unreadable flag field");
				}
			}
		}
		++in;

		QString cmdStr;

		// read command field
		if (listType == CommandTypes::Shell) {

			if (*in++ != QLatin1Char('\n')) {
				Raise<ParseError>("command must begin with newline");
			}

			// leading whitespace
			in.skipWhitespace();

			cmdStr = in.readUntil(QLatin1Char('\n'));

			if (cmdStr.isEmpty()) {
				Raise<ParseError>("shell command field is empty");
			}

		} else {

			QString p = CopyMacroToEnd(in);
			if (p.isNull()) {
				return {};
			}

			cmdStr = std::move(p);
		}

		in.skipWhitespaceNL();

		// create a menu item record
		MenuItem menuItem;
		menuItem.name      = nameStr;
		menuItem.cmd       = cmdStr;
		menuItem.input     = input;
		menuItem.output    = output;
		menuItem.repInput  = repInput;
		menuItem.saveFirst = saveFirst;
		menuItem.loadAfter = loadAfter;
		menuItem.shortcut  = QKeySequence::fromString(accStr);

		return menuItem;
	} catch (const ParseError &error) {
		qWarning("NEdit: Parse error in user defined menu item, %s", error.message.c_str());
		return {};
	}
}

/**
 * @brief Load the macro menu from a YAML file.
 *
 * @param menuItems Where to store the loaded menu items.
 */
void LoadMacroMenuYaml(std::vector<MenuData> &menuItems) {
	try {
		std::vector<YAML::Node> menu;
		const QString filename = Settings::MactoMenuFile();
		if (QFileInfo::exists(filename)) {
			menu = YAML::LoadAllFromFile(filename.toUtf8().data());
		} else {
			static QByteArray defaultMenu = LoadResource(QStringLiteral("DefaultMacroMenu.yaml"));
			menu                          = YAML::LoadAll(defaultMenu.data());
		}

		for (YAML::Node entry : menu) {
			MenuItem menuItem;

			for (auto &&it : entry) {
				const std::string key  = it.first.as<std::string>();
				const YAML::Node value = it.second;

				if (key == "name") {
					menuItem.name = value.as<QString>();
				} else if (key == "command") {
					menuItem.cmd = value.as<QString>();
				} else if (key == "shortcut") {
					menuItem.shortcut = QKeySequence::fromString(value.as<QString>());
				} else if (key == "input") {
					const std::string &input_type = value.as<std::string>();
					if (input_type == "selection") {
						menuItem.input = FROM_SELECTION;
					} else {
						qWarning("Invalid input type: %s", input_type.c_str());
					}
				}
			}

			// add/replace menu record in the list
			auto it2 = std::find_if(menuItems.begin(), menuItems.end(), [&menuItem](const MenuData &data) {
				return data.item.name == menuItem.name;
			});

			if (it2 == menuItems.end()) {
				menuItems.push_back({menuItem, nullptr});
			} else {
				it2->item = menuItem;
			}
		}
	} catch (const YAML::Exception &ex) {
		qWarning("NEdit: Invalid YAML:\n%s", ex.what());
	}
}

/**
 * @brief Load the shell menu from a YAML file.
 *
 * @param menuItems Where to store the loaded menu items.
 */
void LoadShellMenuYaml(std::vector<MenuData> &menuItems) {
	try {
		std::vector<YAML::Node> menu;
		const QString filename = Settings::ShellMenuFile();
		if (QFileInfo::exists(filename)) {
			menu = YAML::LoadAllFromFile(filename.toUtf8().data());
		} else {
#if defined(Q_OS_LINUX)
			static QByteArray defaultMenu = LoadResource(QStringLiteral("DefaultShellMenuLinux.yaml"));
#elif defined(Q_OS_FREEBSD)
			static QByteArray defaultMenu = LoadResource(QStringLiteral("DefaultShellMenuFreeBSD.yaml"));
#elif defined(Q_OS_UNIX)
			static QByteArray defaultMenu = LoadResource(QStringLiteral("DefaultShellMenuUnix.yaml"));
#elif defined(Q_OS_WIN)
			static QByteArray defaultMenu = LoadResource(QStringLiteral("DefaultShellMenuWindows.yaml"));
#else
			// Using an "unsupported" system so we don't have a set of defaults. Just use an empty one
			static QByteArray defaultMenu;
#endif
			menu = YAML::LoadAll(defaultMenu.data());
		}

		for (YAML::Node entry : menu) {
			MenuItem menuItem;

			for (auto &&it : entry) {
				const std::string key  = it.first.as<std::string>();
				const YAML::Node value = it.second;

				if (key == "name") {
					menuItem.name = value.as<QString>();
				} else if (key == "command") {
					menuItem.cmd = value.as<QString>();
				} else if (key == "shortcut") {
					menuItem.shortcut = QKeySequence::fromString(value.as<QString>());
				} else if (key == "input") {
					const std::string &input_type = value.as<std::string>();
					if (input_type == "selection") {
						menuItem.input = FROM_SELECTION;
					} else if (input_type == "window") {
						menuItem.input = FROM_WINDOW;
					} else if (input_type == "either") {
						menuItem.input = FROM_EITHER;
					} else if (input_type == "none") {
						menuItem.input = FROM_NONE;
					} else {
						qWarning("Invalid input type: %s", input_type.c_str());
					}
				} else if (key == "output") {
					const std::string &output_type = value.as<std::string>();
					if (output_type == "dialog") {
						menuItem.output = TO_DIALOG;
					} else if (output_type == "new_window") {
						menuItem.output = TO_NEW_WINDOW;
					} else if (output_type == "same_window") {
						menuItem.output = TO_SAME_WINDOW;
					} else {
						qWarning("Invalid output type: %s", output_type.c_str());
					}
				} else if (key == "replace_input") {
					menuItem.repInput = value.as<bool>();
				} else if (key == "save_first") {
					menuItem.saveFirst = value.as<bool>();
				} else if (key == "load_after") {
					menuItem.loadAfter = value.as<bool>();
				}
			}

			// add/replace menu record in the list
			auto it2 = std::find_if(menuItems.begin(), menuItems.end(), [&menuItem](const MenuData &data) {
				return data.item.name == menuItem.name;
			});

			if (it2 == menuItems.end()) {
				menuItems.push_back({menuItem, nullptr});
			} else {
				it2->item = menuItem;
			}
		}
	} catch (const YAML::Exception &ex) {
		qWarning("NEdit: Invalid YAML:\n%s", ex.what());
	}
}

/**
 * @brief Load the context menu from a YAML file.
 *
 * @param menuItems Where to store the loaded menu items.
 */
void LoadContextMenuYaml(std::vector<MenuData> &menuItems) {
	try {
		std::vector<YAML::Node> menu;
		const QString filename = Settings::ContextMenuFile();
		if (QFileInfo::exists(filename)) {
			menu = YAML::LoadAllFromFile(filename.toUtf8().data());
		} else {
			static QByteArray defaultMenu = LoadResource(QStringLiteral("DefaultContextMenu.yaml"));
			menu                          = YAML::LoadAll(defaultMenu.data());
		}

		for (YAML::Node entry : menu) {
			MenuItem menuItem;

			for (auto &&it : entry) {
				const std::string key  = it.first.as<std::string>();
				const YAML::Node value = it.second;

				if (key == "name") {
					menuItem.name = value.as<QString>();
				} else if (key == "command") {
					menuItem.cmd = value.as<QString>();
				} else if (key == "shortcut") {
					menuItem.shortcut = QKeySequence::fromString(value.as<QString>());
				} else if (key == "input") {
					const std::string input_type = value.as<std::string>();
					if (input_type == "selection") {
						menuItem.input = FROM_SELECTION;
					} else {
						qWarning("Invalid input type: %s", input_type.c_str());
					}
				}
			}

			// add/replace menu record in the list
			auto it2 = std::find_if(menuItems.begin(), menuItems.end(), [&menuItem](const MenuData &data) {
				return data.item.name == menuItem.name;
			});

			if (it2 == menuItems.end()) {
				menuItems.push_back({menuItem, nullptr});
			} else {
				it2->item = menuItem;
			}
		}
	} catch (const YAML::Exception &ex) {
		qWarning("NEdit: Invalid YAML:\n%s", ex.what());
	}
}

/**
 * @brief Load menu items from a string.
 *
 * @param inString The input string containing the menu items.
 * @param menuItems Where to store the loaded menu items.
 * @param listType The type of command list (Shell, Macro, Context).
 */
void LoadMenuItemString(const QString &inString, std::vector<MenuData> &menuItems, CommandTypes listType) {

	if (inString == QStringLiteral("*")) {
		if (listType == CommandTypes::Context) {
			LoadContextMenuYaml(menuItems);
		} else if (listType == CommandTypes::Macro) {
			LoadMacroMenuYaml(menuItems);
		} else if (listType == CommandTypes::Shell) {
			LoadShellMenuYaml(menuItems);
		}
	} else {
		Input in(&inString);

		Q_FOREVER {

			// remove leading whitespace
			in.skipWhitespace();

			// end of string in proper place
			if (in.atEnd()) {
				return;
			}

			std::optional<MenuItem> f = ReadMenuItem(in, listType);
			if (!f) {
				break;
			}

			// add/replace menu record in the list
			auto it = std::find_if(menuItems.begin(), menuItems.end(), [&f](const MenuData &data) {
				return data.item.name == f->name;
			});

			if (it == menuItems.end()) {
				menuItems.push_back({*f, nullptr});
			} else {
				it->item = *f;
			}
		}
	}
}

/**
 * @brief Set the default index for the menu items.
 *
 * @param infoList The list of menu items.
 * @param index The index of the default item.
 */
void SetDefaultIndex(const std::vector<MenuData> &infoList, size_t index) {
	const QString defaultMenuName = infoList[index].info->umiName;

	/* Scan the list for items with the same name and a language mode
	   specified. If one is found, then set the default index to the
	   index of the current default item. */
	for (const MenuData &data : infoList) {
		if (const std::unique_ptr<UserMenuInfo> &info = data.info) {
			if (!info->umiIsDefault && info->umiName == defaultMenuName) {
				info->umiDefaultIndex = index;
			}
		}
	}
}

/**
 * @brief Parse the menu item name and extract language mode information.
 *
 *
 * @param menuItemName  The name of the menu item to parse.
 * @param info          The user menu info structure to store the parsed information.
 */
void ParseMenuItemName(const QString &menuItemName, const std::unique_ptr<UserMenuInfo> &info) {

	const int index = menuItemName.indexOf(QLatin1Char('@'));
	if (index != -1) {
		const QString languageString = menuItemName.mid(index);
		if (languageString == QStringLiteral("*")) {
			/* only language is "*": this is for all but language specific macros */
			info->umiIsDefault = true;
			return;
		}
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
		const QStringList languages = languageString.split(QLatin1Char('@'), Qt::SkipEmptyParts);
#else
		const QStringList languages = languageString.split(QLatin1Char('@'), QString::SkipEmptyParts);
#endif
		std::vector<size_t> languageModes;

		// setup a list of all language modes related to given menu item
		for (const QString &language : languages) {
			/* lookup corresponding language mode index; if PLAIN is
			   returned then this means, that language mode name after
			   "@" is unknown (i.e. not defined) */

			const size_t languageMode = Preferences::FindLanguageMode(language);
			if (languageMode == PLAIN_LANGUAGE_MODE) {
				languageModes.push_back(UNKNOWN_LANGUAGE_MODE);
			} else {
				languageModes.push_back(languageMode);
			}
		}

		if (!languageModes.empty()) {
			info->umiLanguageModes = languageModes;
		}
	}
}

/**
 * @brief Strips the language mode from the menu item name.
 * Removes the language mode part from the menu item name, which
 * is indicated by the presence of an '@' character.
 *
 * @param menuItemName The name of the menu item to strip.
 * @return The stripped menu item name.
 */
QString StripLanguageMode(const QString &menuItemName) {

	const int index = menuItemName.indexOf(QLatin1Char('@'));
	if (index != -1) {
		return menuItemName.left(index);
	}

	return menuItemName;
}

/**
 * @brief Parse a menu item record and create a UserMenuInfo object.
 *
 * @param item The menu item to parse.
 * @return The created UserMenuInfo object.
 */
std::unique_ptr<UserMenuInfo> ParseMenuItemRecord(const MenuItem &item) {

	auto newInfo = std::make_unique<UserMenuInfo>();

	newInfo->umiName = StripLanguageMode(item.name);

	// init. remaining parts of user menu info element
	newInfo->umiIsDefault    = false;
	newInfo->umiDefaultIndex = static_cast<size_t>(-1);

	// assign language mode info to new user menu info element
	ParseMenuItemName(item.name, newInfo);

	return newInfo;
}

/**
 * @brief Select the menu based on the command type.
 *
 * @param type The command type (Shell, Macro, Context).
 * @return The selected menu.
 */
std::vector<MenuData> &SelectMenu(CommandTypes type) {
	switch (type) {
	case CommandTypes::Shell:
		return ShellMenuData;
	case CommandTypes::Macro:
		return MacroMenuData;
	case CommandTypes::Context:
		return BGMenuData;
	}

	Q_UNREACHABLE();
}

}

/**
 * @brief Find a menu item by name and type.
 *
 * @param name The name of the menu item to find.
 * @param type The command type (Shell, Macro, Context).
 * @return The found menu item, or nullptr if not found.
 */
MenuData *FindMenuItem(const QString &name, CommandTypes type) {

	std::vector<MenuData> &menu = SelectMenu(type);

	auto it = std::find_if(menu.begin(), menu.end(), [&name](const MenuData &entry) {
		return entry.item.name == name;
	});

	if (it != menu.end()) {
		return &*it;
	}

	return nullptr;
}

/*
** Generate a text string for the preferences file describing the contents
** of the shell cmd list, macro menu and background menus.
*/
QString WriteShellCommandsString() {
	return WriteShellMenuYaml(ShellMenuData);
}

QString WriteMacroCommandsString() {
	return WriteMacroMenuYaml(MacroMenuData);
}

QString WriteContextMenuCommandsString() {
	return WriteContextMenuYaml(BGMenuData);
}

/**
 * @brief Load shell commands from a string.
 *
 * @param inString The input string containing the shell commands.
 */
void LoadShellCommandsString(const QString &inString) {
	LoadMenuItemString(inString, ShellMenuData, CommandTypes::Shell);
}

/**
 * @brief Load macro commands from a string.
 *
 * @param inString The input string containing the macro commands.
 */
void LoadMacroCommandsString(const QString &inString) {
	LoadMenuItemString(inString, MacroMenuData, CommandTypes::Macro);
}

/**
 * @brief Load context menu commands from a string.
 *
 * @param inString The input string containing the context menu commands.
 */
void LoadContextMenuCommandsString(const QString &inString) {
	LoadMenuItemString(inString, BGMenuData, CommandTypes::Context);
}

/**
 * @brief Setup user menu information.
 */
void SetupUserMenuInfo() {
	ParseMenuItemList(ShellMenuData);
	ParseMenuItemList(MacroMenuData);
	ParseMenuItemList(BGMenuData);
}

/**
 * @brief Update user menu info to take into account changes such as change of language modes.
 * (ex. add / move / delete of language modes etc).
 */
void UpdateUserMenuInfo() {
	for (auto &item : ShellMenuData) {
		item.info = nullptr;
	}
	ParseMenuItemList(ShellMenuData);

	for (auto &item : MacroMenuData) {
		item.info = nullptr;
	}
	ParseMenuItemList(MacroMenuData);

	for (auto &item : BGMenuData) {
		item.info = nullptr;
	}
	ParseMenuItemList(BGMenuData);
}

/**
 * @brief Parse given menu item list and setup a user menu info list for
 * management of user menu.
 *
 * @param itemList The list of menu items to parse.
 */
void ParseMenuItemList(std::vector<MenuData> &itemList) {

	/* 1st pass: setup user menu info: extract language modes, menu name &
	   default indication; build user menu ID */
	for (MenuData &data : itemList) {
		data.info = ParseMenuItemRecord(data.item);
	}

	// 2nd pass: solve "default" dependencies
	for (size_t i = 0; i < itemList.size(); i++) {
		/* If the user menu item is a default one, then scan the list for
		   items with the same name and a language mode specified.
		   If one is found, then set the default index to the index of the
		   current default item. */
		if (itemList[i].info->umiIsDefault) {
			SetDefaultIndex(itemList, i);
		}
	}
}
