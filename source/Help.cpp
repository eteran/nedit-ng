
#include "Help.h"
#include <QMessageBox>

/**
 * @brief displayTopic
 * @param topic
 */
void Help::displayTopic(Help::Topic topic) {
    Q_UNUSED(topic);

    QMessageBox::warning(
                nullptr,
                tr("Not Implemented"),
                tr("Sorry, but the help system is not yet implemented!"));
}

