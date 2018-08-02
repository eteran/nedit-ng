
#include "Util/User.h"

#include <QtGlobal>
#include <QString>
#include <QDir>
#include <QStandardPaths>

#ifdef Q_OS_UNIX
#include <unistd.h>
#include <pwd.h>
#endif

/*
** Expand tilde characters which begin file names as done by the shell
** If it doesn't work just return the pathname originally passed pathname
*/
QString ExpandTilde(const QString &pathname) {
#ifdef Q_OS_UNIX
	struct passwd *passwdEntry;

	if (!pathname.startsWith(QLatin1Char('~'))) {
		return pathname;
	}

	int end = pathname.indexOf(QLatin1Char('/'));
	if(end == -1) {
		end = pathname.size();
	}

	QString username = pathname.mid(1, end - 1);

	/* We might consider to re-use the GetHomeDirEx() function,
	   but to keep the code more similar for both cases ... */
	if (username.isEmpty()) {
		passwdEntry = getpwuid(getuid());
		if ((passwdEntry == nullptr) || (*(passwdEntry->pw_dir) == '\0')) {
			/* This is really serious, so just exit. */
			qFatal("getpwuid() failed ");
		}
	} else {
		passwdEntry = getpwnam(username.toLatin1().data());
		if ((passwdEntry == nullptr) || (*(passwdEntry->pw_dir) == '\0')) {
			/* username was just an input by the user, this is no indication
			 * for some (serious) problems */
			return QString();
		}
	}

	return QString(QLatin1String("%1/%2")).arg(QString::fromUtf8(passwdEntry->pw_dir), pathname.mid(end));
#else
	return pathname;
#endif
}

/* return a non-nullptr value for the user's home directory,
   without trailing slash.
   We try the  environment var and the system user database. */
QString GetHomeDir() {
	return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

/**
 * @brief PrependHomeEx
 * @param filename
 * @return $HOME/filename
 */
QString PrependHome(const QString &filename) {
	return QString(QLatin1String("%1/%2")).arg(GetHomeDir(), filename);
}

/*
** Return a pointer to the username of the current user in a statically
** allocated string.
*/
QString GetUserName() {

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
	return QString::fromLocal8Bit(qgetenv("USER"));
#else
	return QString();
#endif
}
