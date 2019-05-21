
#include "Util/ServerCommon.h"
#include "Util/Host.h"
#include "Util/User.h"
#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

/*
 * Create the server socket name for the server with serverName.
 * names are generated as follows as either of the following
 *
 * $RUNTIME_PATH/nedit-ng_<host_name>_<server_name>_<display>
 *
 * nedit-ng_<host_name>_<user>_<server_name>_<display>
 *
 * <server_name> is the name that can be set by the user to allow
 * for multiple servers to run on the same display. <server_name>
 * defaults to "" if not supplied by the user.
 *
 * <user> is the user name of the current user.
 *
 * A typical example of $RUNTIME_PATH would be something like:
 * /var/run/user/1000/
 */
QString LocalSocketName(const QString &server_name) {

	const QString runtimePath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
	const QString hostname    = GetNameOfHost();

	if (!runtimePath.isEmpty()) {
		QDir().mkpath(runtimePath);
#ifdef Q_OS_LINUX
		QByteArray display = qgetenv("DISPLAY");
		return QString(QLatin1String("%1/nedit-ng_%2_%3_%4")).arg(runtimePath, hostname, server_name, QString::fromLocal8Bit(display));
#else
		return QString(QLatin1String("%1/nedit-ng_%2_%3")).arg(runtimePath, hostname, server_name);
#endif
	} else {
#ifdef Q_OS_LINUX
		QByteArray display = qgetenv("DISPLAY");
		return QString(QLatin1String("nedit-ng_%1_%2_%3_%4")).arg(GetUserName(), hostname, server_name, QString::fromLocal8Bit(display));
#else
		return QString(QLatin1String("nedit-ng_%1_%2_%3")).arg(GetUserName(), hostname, server_name);
#endif
	}
}
