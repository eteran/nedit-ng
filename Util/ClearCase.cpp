
#include "Util/ClearCase.h"
#include "Util/Environment.h"

#include <QString>

namespace ClearCase {

/**
 * @brief Get the index of the version extended path in a ClearCase filename.
 *
 * @param fullname The full ClearCase filename, which may include a version extended path.
 * @return The index of the version extended path, or -1 if not found.
 */
int GetVersionExtendedPathIndex(const QString &fullname) {
	return fullname.indexOf(QLatin1String("@@/"));
}

/**
 * @brief Get the version extended path from a ClearCase filename.
 *
 * @param fullname The full ClearCase filename, which may include a version extended path.
 * @return The version extended path if found, or an empty string if not found.
 */
QString GetVersionExtendedPath(const QString &fullname) {
	const int n = GetVersionExtendedPathIndex(fullname);
	if (n == -1) {
		return QString();
	}
	return fullname.mid(n);
}

/**
 * @brief Get the View Tag object
 *
 * @return A string showing the ClearCase view tag.  If ClearCase is not in
 * use, or a view is not set, QString() is returned.
 *
 * @note This function caches the view tag after the first call, so subsequent calls
 * will return the cached value without re-evaluating the environment variable.
 * @note If user has ClearCase and is in a view, CLEARCASE_ROOT will be set and
 * the view tag can be extracted.  This check is safe and efficient enough
 * that it doesn't impact non-clearcase users, so it is not conditionally
 * compiled. (Thanks to Max Vohlken)
 */
QString GetViewTag() {

	static bool ClearCaseViewTagFound = false;
	static QString ClearCaseViewRoot;
	static QString ClearCaseViewTag;

	if (!ClearCaseViewTagFound) {
		/* Extract the view name from the CLEARCASE_ROOT environment variable */
		const QString ClearCaseViewRoot = GetEnvironmentVariable("CLEARCASE_ROOT");
		if (!ClearCaseViewRoot.isNull()) {
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
