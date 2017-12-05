
#include "utils.h"
#include <gsl/gsl_util>

#include <QtGlobal>
#include <QDir>
#include <QHostInfo>
#include <QStandardPaths>
#include <QResource>

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <pwd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>


/* return non-nullptr value for the current working directory.
   If system call fails, provide a fallback value */
QString GetCurrentDirEx() {
	return QDir::currentPath();
}

/* return a non-nullptr value for the user's home directory,
   without trailing slash.
   We try the  environment var and the system user database. */
QString GetHomeDirEx() {
    return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

/*
** Return a pointer to the username of the current user in a statically
** allocated string.
*/
QString GetUserNameEx() {

#ifdef Q_OS_UNIX
	/* cuserid has apparently been dropped from the ansi C standard, and if
	   strict ansi compliance is turned on (on Sun anyhow, maybe others), calls
	   to cuserid fail to compile.  Older versions of nedit try to use the
	   getlogin call first, then if that fails, use getpwuid and getuid.  This
	   results in the user-name of the original terminal being used, which is
	   not correct when the user uses the su command.  Now, getpwuid only: */

	static QString user_name;

	if (!user_name.isNull()) {
        return user_name;
	}

	if(const struct passwd *passwdEntry = getpwuid(getuid())) {
		user_name = QString::fromLatin1(passwdEntry->pw_name);
        return user_name;
	}
	
	/* This is really serious, but sometimes username service
	   is misconfigured through no fault of the user.  Be nice
	   and let the user start nc anyway. */
	perror("nedit: getpwuid() failed - reverting to $USER");
    return QString::fromLatin1(qgetenv("USER"));
#else
	return QString();
#endif
}

/**
 * @brief GetNameOfHostEx
 * @return the hostname of the system
 */
QString GetNameOfHostEx() {
	return QHostInfo::localHostName();
}

/**
 * @brief PrependHomeEx
 * @param filename
 * @return $HOME/filename
 */
QString PrependHomeEx(const QString &filename) {	
	return QString(QLatin1String("%1/%2")).arg(GetHomeDirEx(), filename);
}

/**
 * @brief ErrorString
 * @param error
 * @return The result of strerror(error), but as a QString
 */
QString ErrorString(int error) {
    return QString::fromLatin1(strerror(error));
}

/**
 * @brief loadResource
 * @param resource
 * @return
 */
QByteArray loadResource(const QString &resource) {

    QResource res(resource);
    if(!res.isValid()) {
        qFatal("Failed to load internal resource");
    }

    // don't copy the data, if it's uncompressed, we can deal with it in place :-)
    auto defaults = QByteArray::fromRawData(reinterpret_cast<const char *>(res.data()), gsl::narrow<int>(res.size()));

    if(res.isCompressed()) {
        defaults = qUncompress(defaults);
    }

    return defaults;
}
