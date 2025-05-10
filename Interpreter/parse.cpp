
#include "parse.h"
#include "interpret.h"

/**
 * @brief Checks if a macro expression is valid.
 *
 * @param expr The macro expression to check.
 * @param message A QString that will hold an error message if the macro is invalid.
 * @param stoppedAt An integer that will hold the position in the expression where
 *                  parsing stopped, if it was invalid. If the macro is valid, this will be set to 0.
 * @return `true` if the macro is valid, `false` otherwise.
 */
bool IsMacroValid(const QString &expr, QString *message, int *stoppedAt) {
	if (Program *prog = CompileMacro(expr, message, stoppedAt)) {
		delete prog;
		return true;
	}

	return false;
}
