
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

private:
    SmartIndent() = delete;

public:
    static int LMHasSmartIndentMacros(const QString &languageMode);
    static int LoadSmartIndentCommonStringEx(const QString &string);
    static int LoadSmartIndentStringEx(const QString &string);
    static int SmartIndentMacrosAvailable(const QString &languageModeName);
    static QString  WriteSmartIndentCommonStringEx();
    static QString WriteSmartIndentStringEx();
    static void EditCommonSmartIndentMacro();
    static void RenameSmartIndentMacros(const QString &oldName, const QString &newName);
    static void UpdateLangModeMenuSmartIndent();
    static QByteArray defaultCommonMacros();
    static const SmartIndentEntry *findIndentSpec(const QString &name);
    static const SmartIndentEntry *findDefaultIndentSpec(const QString &name);

private:
    static bool siParseError(const Input &in, const QString &message);
};

extern QString CommonMacros;
extern std::vector<SmartIndentEntry> SmartIndentSpecs;
extern QPointer<DialogSmartIndent> SmartIndentDlg;

struct SmartIndentData {
    Program *newlineMacro = nullptr;
    Program *modMacro     = nullptr;
    int inNewLineMacro    = 0;
    int inModMacro        = 0;
};

#endif 
