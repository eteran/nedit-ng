
#include "Help.h"
#include <QMessageBox>

namespace Help {

/**
 * @brief displayTopic
 * @param topic
 */
void displayTopic(Help::Topic topic) {
    Q_UNUSED(topic);

    QMessageBox::warning(
                nullptr,
                QLatin1String("Not Implemented"),
                QLatin1String("Sorry, but the help system is not yet implemented!"));
}

}
