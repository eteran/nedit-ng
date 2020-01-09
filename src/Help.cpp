
#include "Help.h"
#include "Util/version.h"
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

namespace Help {

/**
 * @brief displayTopic
 * @param topic
 */
void displayTopic(Topic topic) {

	QUrl url;
	switch(topic) {
	case Help::Topic::Start:
		url = QString(QLatin1String("https://eteran.github.io/nedit-ng/%1.%2/")).arg(NEDIT_VERSION_MAJ).arg(NEDIT_VERSION_REV);
		break;
	case Help::Topic::SmartIndent:
		url = QString(QLatin1String("https://eteran.github.io/nedit-ng/%1.%2/12")).arg(NEDIT_VERSION_MAJ).arg(NEDIT_VERSION_REV);
		break;
	case Help::Topic::Syntax:
		url = QString(QLatin1String("https://eteran.github.io/nedit-ng/%1.%2/13")).arg(NEDIT_VERSION_MAJ).arg(NEDIT_VERSION_REV);
		break;
	case Help::Topic::TabsDialog:
		url = QString(QLatin1String("https://eteran.github.io/nedit-ng/%1.%2/41")).arg(NEDIT_VERSION_MAJ).arg(NEDIT_VERSION_REV);
		break;
	case Help::Topic::CustomTitleDialog:
		url = QString(QLatin1String("https://eteran.github.io/nedit-ng/%1.%2/42")).arg(NEDIT_VERSION_MAJ).arg(NEDIT_VERSION_REV);
		break;
	default:
		url = QString(QLatin1String("https://eteran.github.io/nedit-ng/%1.%2/")).arg(NEDIT_VERSION_MAJ).arg(NEDIT_VERSION_REV);
		break;
	}

	QDesktopServices::openUrl(url);
}

}
