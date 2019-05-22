
#include "parse.h"
#include "interpret.h"

/**
 * @brief isMacroValid
 * @param expr
 * @param message
 * @param stoppedAt
 * @return
 */
bool isMacroValid(const QString &expr, QString *message, int *stoppedAt) {
	if (Program *prog = compileMacro(expr, message, stoppedAt)) {
		delete prog;
		return true;
	}

	return false;
}
