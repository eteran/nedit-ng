
#include "Util/Host.h"

#include <QHostInfo>
#include <QString>

/**
 * @brief Get the hostname of the system.
 *
 * @return The hostname of the system
 */
QString GetNameOfHost() {
	return QHostInfo::localHostName();
}
