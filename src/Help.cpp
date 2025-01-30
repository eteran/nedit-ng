
#include "Help.h"
#include "Util/version.h"

#include <QDesktopServices>
#include <QMessageBox>
#include <QUrl>

namespace Help {

/**
 * @brief displayTopic
 * @param topic
 */
void displayTopic(Topic topic) {

	QUrl url;
	switch (topic) {
	case Help::Topic::Start:
		url = QStringLiteral("https://eteran.github.io/nedit-ng/%1.%2/").arg(NEDIT_VERSION_MAJ).arg(NEDIT_VERSION_REV);
		break;
	case Help::Topic::SmartIndent:
		url = QStringLiteral("https://eteran.github.io/nedit-ng/%1.%2/12").arg(NEDIT_VERSION_MAJ).arg(NEDIT_VERSION_REV);
		break;
	case Help::Topic::Syntax:
		url = QStringLiteral("https://eteran.github.io/nedit-ng/%1.%2/13").arg(NEDIT_VERSION_MAJ).arg(NEDIT_VERSION_REV);
		break;
	case Help::Topic::TabsDialog:
		url = QStringLiteral("https://eteran.github.io/nedit-ng/%1.%2/41").arg(NEDIT_VERSION_MAJ).arg(NEDIT_VERSION_REV);
		break;
	case Help::Topic::CustomTitleDialog:
		url = QStringLiteral("https://eteran.github.io/nedit-ng/%1.%2/42").arg(NEDIT_VERSION_MAJ).arg(NEDIT_VERSION_REV);
		break;
	default:
		url = QStringLiteral("https://eteran.github.io/nedit-ng/%1.%2/").arg(NEDIT_VERSION_MAJ).arg(NEDIT_VERSION_REV);
		break;
	}

	QDesktopServices::openUrl(url);
}

}
