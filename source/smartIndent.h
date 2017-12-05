
#ifndef X_SMART_INDENT_H_
#define X_SMART_INDENT_H_

#include <vector>
#include <QPointer>

class SmartIndentEntry;
class Program;
class DialogSmartIndent;

class QString;
class QByteArray;

int LMHasSmartIndentMacros(const QString &languageMode);
int LoadSmartIndentCommonStringEx(const QString &string);
int LoadSmartIndentStringEx(const QString &string);
int SmartIndentMacrosAvailable(const QString &languageModeName);
QString  WriteSmartIndentCommonStringEx();
QString WriteSmartIndentStringEx();
void EditCommonSmartIndentMacro();
void RenameSmartIndentMacros(const QString &oldName, const QString &newName);
void UpdateLangModeMenuSmartIndent();
QByteArray defaultCommonMacros();
const SmartIndentEntry *findIndentSpec(const QString &name);
const SmartIndentEntry *findDefaultIndentSpec(const QString &name);

extern QString CommonMacros;
extern std::vector<SmartIndentEntry> SmartIndentSpecs;
extern QPointer<DialogSmartIndent> SmartIndentDlg;

struct SmartIndentData {
    Program *newlineMacro;
    int      inNewLineMacro;
    Program *modMacro;
    int      inModMacro;
};

#endif 
