
#include "Util/ServerCommon.h"
#include "Util/Host.h"
#include "Util/User.h"
#include <QCryptographicHash>
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
 *
 * NOTE(eteran): to avoid path length issues, server_name is hashed in order
 * to create a predictable, fixed length string. Which is then rendered to the path
 * as a hex-string
 */
QString LocalSocketName(const QString &server_name) {

	const QString runtimePath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
	const QString hostname    = GetNameOfHost();

	// convert the server_name to a hex-string of the SHA-1 hash in order
	// to get a predictable, fixed length string from every user input
	// https://github.com/eteran/nedit-ng/issues/328
	const QByteArray hashed_server_name = QCryptographicHash::hash(server_name.toUtf8(), QCryptographicHash::Sha1);
	const QString server_id             = QString::fromUtf8(hashed_server_name.toHex());

	if (!runtimePath.isEmpty()) {
		QDir().mkpath(runtimePath);
		return QStringLiteral("%1/nedit-ng_%2_%3").arg(runtimePath, hostname, server_id);
	}

	return QStringLiteral("nedit-ng_%1_%2_%3").arg(getUserName(), hostname, server_id);
}
