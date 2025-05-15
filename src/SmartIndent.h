
#ifndef SMART_INDENT_H_
#define SMART_INDENT_H_

#include "Util/QtHelper.h"

#include <QPointer>

#include <memory>
#include <vector>

class SmartIndentEntry;
class DialogSmartIndent;
class Input;
struct Program;

class QString;
class QByteArray;

namespace SmartIndent {
Q_DECLARE_NAMESPACE_TR(SmartIndent)

bool LMHasSmartIndentMacros(const QString &languageMode);
bool SmartIndentMacrosAvailable(const QString &languageModeName);
const SmartIndentEntry *FindDefaultIndentSpec(const QString &language);
const SmartIndentEntry *FindIndentSpec(const QString &language);
QString LoadDefaultCommonMacros();
QString WriteSmartIndentCommonString();
QString WriteSmartIndentString();
void LoadSmartIndentCommonString(const QString &string);
void LoadSmartIndentFromYaml();
void LoadSmartIndentString(const QString &string);
void RenameSmartIndentMacros(const QString &oldName, const QString &newName);
void UpdateLangModeMenuSmartIndent();

extern QString CommonMacros;
extern std::vector<SmartIndentEntry> SmartIndentSpecs;
extern QPointer<DialogSmartIndent> SmartIndentDialog;
}

struct SmartIndentData {
	std::unique_ptr<Program> newlineMacro;
	std::unique_ptr<Program> modMacro;
	int inNewLineMacro = 0;
	int inModMacro     = 0;
};

#endif
