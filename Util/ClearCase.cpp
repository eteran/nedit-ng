
#include "Util/ClearCase.h"
#include <QString>

namespace ClearCase {

/**
 * @brief GetVersionExtendedPathIndex
 * @param fullname
 * @return
 */
int GetVersionExtendedPathIndex(const QString &fullname) {
	return fullname.indexOf(QLatin1String("@@/"));
}

/**
 * @brief GetVersionExtendedPath
 * @param fullname
 * @return
 */
QString GetVersionExtendedPath(const QString &fullname) {
	const int n = GetVersionExtendedPathIndex(fullname);
	if (n == -1) {
		return QString();
	}
	return fullname.mid(n);
}

/*
** Return a string showing the ClearCase view tag.  If ClearCase is not in
** use, or a view is not set, nullptr is returned.
**
** If user has ClearCase and is in a view, CLEARCASE_ROOT will be set and
** the view tag can be extracted.  This check is safe and efficient enough
** that it doesn't impact non-clearcase users, so it is not conditionally
** compiled. (Thanks to Max Vohlken)
*/
QString GetViewTag() {

	static bool ClearCaseViewTagFound = false;
	static QString ClearCaseViewRoot;
	static QString ClearCaseViewTag;

	if (!ClearCaseViewTagFound) {
		/* Extract the view name from the CLEARCASE_ROOT environment variable */
		const QByteArray envPtr = qgetenv("CLEARCASE_ROOT");
		if (!envPtr.isNull()) {

			ClearCaseViewRoot = QString::fromLocal8Bit(envPtr);

			const int tagPtr = ClearCaseViewRoot.lastIndexOf(QLatin1Char('/'));
			if (tagPtr != -1) {
				ClearCaseViewTag = ClearCaseViewRoot.mid(tagPtr + 1);
			}
		}
	}
	/* If we don't find it first time, we will never find it, so may just as
	 * well say that we have found it.
	 */
	ClearCaseViewTagFound = true;
	return ClearCaseViewTag;
}

}
