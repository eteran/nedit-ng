
#include "Util/User.h"
#include "Util/Environment.h"

#include <QDir>
#include <QStandardPaths>
#include <QString>
#include <QtGlobal>

#ifdef Q_OS_UNIX
#include <pwd.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <Lmcons.h>
#include <Windows.h>
#endif

/**
 * @brief Expands a tilde in a pathname to the user's home directory.
 *
 * @param pathname the pathname to expand, which may start with a tilde (~).
 * @return the expanded pathname. If expansion fails, returns the original pathname.
 */
QString expandTilde(const QString &pathname) {
#ifdef Q_OS_UNIX
	struct passwd *passwdEntry;

	if (!pathname.startsWith(QLatin1Char('~'))) {
		return pathname;
	}

	int end = pathname.indexOf(QLatin1Char('/'));
	if (end == -1) {
		end = pathname.size();
	}

	const QString username = pathname.mid(1, end - 1);

	/* We might consider to re-use the getHomeDir() function,
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

	return QStringLiteral("%1/%2").arg(QString::fromUtf8(passwdEntry->pw_dir), pathname.mid(end));
#else
	return pathname;
#endif
}

/**
 * @brief Get the user's home directory.
 *
 * @return The path to the user's home directory.
 *         On Unix, this is typically /home/username or /Users/username on macOS.
 *         On Windows, this is typically C:\Users\username.
 */
QString getHomeDir() {
	return QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
}

/**
 * @brief Prepend the user's home directory to a filename.
 *
 * @param filename the filename to prepend with the home directory.
 * @return "$HOME/filename"
 */
QString prependHome(const QString &filename) {
	return QStringLiteral("%1/%2").arg(getHomeDir(), filename);
}

/**
 * @brief Get the username of the current user.
 *
 * @return The username of the current user.
 */
QString getUserName() {
#ifdef Q_OS_UNIX
	static QString user_name;

	if (!user_name.isNull()) {
		return user_name;
	}

	if (const struct passwd *passwdEntry = getpwuid(getuid())) {
		user_name = QString::fromLatin1(passwdEntry->pw_name);
		return user_name;
	}

	/* This is really serious, but sometimes username service
	   is misconfigured through no fault of the user.  Be nice
	   and let the user start nc anyway. */
	perror("nedit: getpwuid() failed - reverting to $USER");
	return GetEnvironmentVariable("USER");
#elif defined(Q_OS_WIN)
	wchar_t name[UNLEN + 1] = {};
	DWORD size              = UNLEN + 1;

	if (GetUserNameW(name, &size)) {
		return QString::fromWCharArray(name, size);
	}
#endif
	return QString();
}

/*
**  This function returns the user's login shell.
**  In case of errors, the fallback of "sh" will be returned.
*/
QString getDefaultShell() {
#ifdef Q_OS_UNIX
	struct passwd *passwdEntry = getpwuid(getuid()); //  getuid() never fails.

	if (!passwdEntry) {
		//  Something bad happened! Do something, quick!
		perror("NEdit: Failed to get passwd entry (falling back to 'sh')");
		return QLatin1String("sh");
	}

	return QString::fromUtf8(passwdEntry->pw_shell);
#else
	// TODO(eteran): maybe return powershell on windows?
	return QString();
#endif
}
