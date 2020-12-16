
#include "DialogAbout.h"
#include "Util/System.h"
#include "Util/version.h"

#include <QLocale>

/**
 * @brief DialogAbout::DialogAbout
 * @param parent
 * @param f
 */
DialogAbout::DialogAbout(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {
	ui.setupUi(this);

	ui.textAbout->setText(tr("%1"
							 "\n"
							 "nedit-ng was written by Evan Teran. It is intended to be a modern replacement for the Nirvana Editor (aka NEdit). The author has been using NEdit as his primary code editor for many years, and while it continues to be a superior editor in many ways, it is unfortunately showing its age. So nedit-ng was born out of a desire to have an editor that functions as close to the original as possible, but utilizing a modern toolkit (Qt). This will allow nedit-ng to enjoy the benefit of modern features such as:\n"
							 "\n"
							 "* Anti-aliased font rendering.\n"
							 "* Support for internationalization.\n"
							 "* Modern look and feel.\n"
							 "* Internally, counted strings are used instead of NUL terminated strings.\n"
							 "* Use of C++ containers means many internal size limits are no longer present.\n"
							 "* Code as been reworked using modern C++ techniques using a toolkit with an active developer community, making it significantly easier for contributions to be made by the open source community.\n")
							  .arg(createInfoString()));
}

/**
 * @brief DialogAbout::createInfoString
 * @return
 */
QString DialogAbout::createInfoString() {

#ifdef NEDIT_VERSION_GIT
	auto versionString = tr(NEDIT_VERSION_GIT);
#else
	auto versionString = tr("%1.%2").arg(NEDIT_VERSION_MAJ).arg(NEDIT_VERSION_REV);
#endif

	QString localeString = QLocale::system().bcp47Name();
	QString gitExtra;

#ifdef NEDIT_BRANCH_GIT
	gitExtra += tr("   Git branch: %1\n").arg(QLatin1String(NEDIT_BRANCH_GIT));
#endif
#ifdef NEDIT_COMMIT_GIT
	gitExtra += tr("   Git commit: %1\n").arg(QLatin1String(NEDIT_COMMIT_GIT));
#endif

	return tr("nedit-ng version %1\n"
			  "\n"
			  "     Built on: %2, %3, %4\n"
			  "      With Qt: %7\n"
			  "   Running Qt: %8\n"
			  "       Locale: %9\n"
			  "%10\n")
		.arg(versionString,
			 buildOperatingSystem(),
			 buildArchitecture(),
			 buildCompiler(),
			 QLatin1String(QT_VERSION_STR),
			 QString::fromLatin1(qVersion()),
			 localeString,
			 gitExtra);
}
