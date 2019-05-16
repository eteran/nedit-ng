
#include "Help.h"
#include <QMessageBox>

namespace Help {

/**
 * @brief displayTopic
 * @param topic
 */
void displayTopic(QWidget *parent, Topic topic) {
	Q_UNUSED(topic)

	QMessageBox::warning(
				parent,
				tr("Not Implemented"),
				tr("Sorry, but the help system is not yet implemented!"));
}

}
