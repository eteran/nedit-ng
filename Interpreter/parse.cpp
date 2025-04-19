
#include "parse.h"
#include "interpret.h"

/**
 * @brief Checks if a macro expression is valid.
 *
 * @param expr the macro expression to check.
 * @param message a pointer to a QString that will hold an error message if the macro is invalid.
 * @param stoppedAt a pointer to an integer that will hold the position in the expression where
 *                  parsing stopped, if it was invalid. If the macro is valid, this will be set to 0.
 * @return true if the macro is valid, false otherwise.
 */
bool isMacroValid(const QString &expr, QString *message, int *stoppedAt) {
	if (Program *prog = compileMacro(expr, message, stoppedAt)) {
		delete prog;
		return true;
	}

	return false;
}
