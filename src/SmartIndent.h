
#ifndef SMART_INDENT_H_
#define SMART_INDENT_H_

#include <vector>
#include <memory>
#include <QPointer>
#include "Util/QtHelper.h"

class SmartIndentEntry;
class DialogSmartIndent;
class Input;
struct Program;

class QString;
class QByteArray;

namespace SmartIndent {
	Q_DECLARE_NAMESPACE_TR(SmartIndent)

	bool LMHasSmartIndentMacros(const QString &languageMode);
	bool LoadSmartIndentCommonString(const QString &string);
	bool LoadSmartIndentString(const QString &string);
	bool SmartIndentMacrosAvailable(const QString &languageModeName);
	QString WriteSmartIndentCommonStringEx();
	QString WriteSmartIndentStringEx();
	void EditCommonSmartIndentMacro();
	void RenameSmartIndentMacros(const QString &oldName, const QString &newName);
	void UpdateLangModeMenuSmartIndent();
	QByteArray defaultCommonMacros();
	const SmartIndentEntry *findIndentSpec(const QString &name);
	const SmartIndentEntry *findDefaultIndentSpec(const QString &name);

	extern QString                       CommonMacros;
	extern std::vector<SmartIndentEntry> SmartIndentSpecs;
	extern QPointer<DialogSmartIndent>   SmartIndentDialog;
}

struct SmartIndentData {
	std::unique_ptr<Program> newlineMacro;
	std::unique_ptr<Program> modMacro;
	int inNewLineMacro    = 0;
	int inModMacro        = 0;
};

#endif
