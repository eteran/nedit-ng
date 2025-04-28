
#ifndef MACRO_H_
#define MACRO_H_

#include <QTimer>

#include <memory>
#include <string_view>

class DocumentWidget;
class MainWindow;
struct MacroContext;
struct Program;

class QString;
class QWidget;

enum RepeatMethod {
	REPEAT_TO_END = -1,
	REPEAT_IN_SEL = -2,
};

bool CheckMacroString(QWidget *dialogParent, const QString &string, const QString &errIn, int *errPos);
bool ReadCheckMacroString(QWidget *dialogParent, const QString &string, DocumentWidget *runDocument, const QString &errIn, int *errPos);

void RegisterMacroSubroutines();
void ReturnShellCommandOutput(DocumentWidget *document, std::string_view outText, int status);

/* Data attached to window during shell command execution with
   information for controlling and communicating with the process */
struct MacroCommandData {
	QTimer bannerTimer;
	QTimer continuationTimer;
	bool bannerIsUp        = false;
	bool closeOnCompletion = false;
	std::shared_ptr<MacroContext> context;
	std::unique_ptr<Program> program;
};

#endif
