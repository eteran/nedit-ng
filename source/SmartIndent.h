
#ifndef SMART_INDENT_H_
#define SMART_INDENT_H_

#include <vector>
#include <QPointer>
#include <QCoreApplication>

class SmartIndentEntry;
class DialogSmartIndent;
class Input;
struct Program;

class QString;
class QByteArray;

class SmartIndent {
	Q_DECLARE_TR_FUNCTIONS(SmartIndent)

public:
	SmartIndent() = delete;

public:
	static bool LMHasSmartIndentMacros(const QString &languageMode);
	static bool LoadSmartIndentCommonStringEx(const QString &string);
	static bool LoadSmartIndentStringEx(const QString &string);
	static bool SmartIndentMacrosAvailable(const QString &languageModeName);
	static QString  WriteSmartIndentCommonStringEx();
	static QString WriteSmartIndentStringEx();
	static void EditCommonSmartIndentMacro();
	static void RenameSmartIndentMacros(const QString &oldName, const QString &newName);
	static void UpdateLangModeMenuSmartIndent();
	static QByteArray defaultCommonMacros();
	static const SmartIndentEntry *findIndentSpec(const QString &name);
	static const SmartIndentEntry *findDefaultIndentSpec(const QString &name);

public:
	static QString                       CommonMacros;
	static std::vector<SmartIndentEntry> SmartIndentSpecs;
	static QPointer<DialogSmartIndent>   SmartIndentDlg;
};

struct SmartIndentData {
	Program *newlineMacro = nullptr;
	Program *modMacro     = nullptr;
	int inNewLineMacro    = 0;
	int inModMacro        = 0;
};

#endif
