
#include "parse.h"
#include <string>
#include <QString>

std::shared_ptr<Program> ParseMacro(const std::string &expr, std::string *msg, int *stoppedAt);

/**
 * @brief ParseMacroEx
 * @param expr
 * @param message
 * @param stoppedAt
 * @return
 */
Program *ParseMacroEx(const QString &expr, QString *message, int *stoppedAt) {

    auto str = expr.toStdString();

    std::string msg;
    std::shared_ptr<Program> p = ParseMacro(str, &msg, stoppedAt);

    *message = QString::fromStdString(msg);
    return p;
}
