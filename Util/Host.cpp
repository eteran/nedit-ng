
#include "Util/Host.h"

#include <QHostInfo>
#include <QString>

/**
 * @brief GetNameOfHostEx
 * @return the hostname of the system
 */
QString GetNameOfHostEx() {
	return QHostInfo::localHostName();
}
