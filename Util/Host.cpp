
#include "Util/Host.h"

#include <QHostInfo>
#include <QString>

/**
 * @brief
 *
 * @return the hostname of the system
 */
QString GetNameOfHost() {
	return QHostInfo::localHostName();
}
