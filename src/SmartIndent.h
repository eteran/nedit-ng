
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
bool loadSmartIndentCommonString(const QString &string);
bool loadSmartIndentString(const QString &string);
bool smartIndentMacrosAvailable(const QString &languageModeName);
QString writeSmartIndentCommonString();
QString writeSmartIndentString();
void editCommonSmartIndentMacro();
void renameSmartIndentMacros(const QString &oldName, const QString &newName);
void updateLangModeMenuSmartIndent();
QByteArray defaultCommonMacros();
const SmartIndentEntry *findIndentSpec(const QString &name);
const SmartIndentEntry *findDefaultIndentSpec(const QString &name);

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
