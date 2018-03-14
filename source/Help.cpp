
#include "Help.h"
#include <QMessageBox>

/**
 * @brief displayTopic
 * @param topic
 */
void Help::displayTopic(QWidget *parent, Help::Topic topic) {
    Q_UNUSED(topic);

    QMessageBox::warning(
                parent,
                tr("Not Implemented"),
                tr("Sorry, but the help system is not yet implemented!"));
}

