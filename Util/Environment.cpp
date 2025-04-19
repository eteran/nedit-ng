
#include "Util/Environment.h"

/**
 * @brief Get an environment variable's value.
 *
 * @param name the name of the environment variable to retrieve.
 * @return a QString containing the value of the environment variable, or QString() if not found.
 */
QString GetEnvironmentVariable(const char *name) {
	const QByteArray envValue = qgetenv(name);
	if (envValue.isNull()) {
		return QString();
	}
	return QString::fromLocal8Bit(envValue);
}

/**
 * @brief Get an environment variable's value.
 *
 * @param name the name of the environment variable to retrieve.
 * @return a QString containing the value of the environment variable, or QString() if not found.
 */
QString GetEnvironmentVariable(const QString &name) {
	return GetEnvironmentVariable(name.toLocal8Bit().constData());
}

/**
 * @brief Get an environment variable's value.
 *
 * @param name the name of the environment variable to retrieve.
 * @return a QString containing the value of the environment variable, or QString() if not found.
 */
QString GetEnvironmentVariable(const std::string &name) {
    return GetEnvironmentVariable(name.c_str());
}
