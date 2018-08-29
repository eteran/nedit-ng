
#ifndef MACRO_H_
#define MACRO_H_

#include <QTimer>
#include <memory>

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

bool CheckMacroStringEx(QWidget *dialogParent, const QString &string, const QString &errIn, int *errPos);
bool readCheckMacroString(QWidget *dialogParent, const QString &string, DocumentWidget *runDocument, const QString &errIn, int *errPos);

void RegisterMacroSubroutines();
void returnShellCommandOutput(DocumentWidget *document, const QString &outText, int status);

/* Data attached to window during shell command execution with
   information for controling and communicating with the process */
struct MacroCommandData {
	QTimer                        bannerTimer;
	QTimer                        continuationTimer;
	Program *                     program           = nullptr;
	bool                          bannerIsUp        = false;
	bool                          closeOnCompletion = false;
	std::shared_ptr<MacroContext> context;
};



#endif
