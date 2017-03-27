
#include "CommandRecorder.h"
#include "TextEditEvent.h"
#include <QMutex>
#include <QtDebug>

namespace {

CommandRecorder *instance = nullptr;
QMutex instanceMutex;

}

/**
 * @brief CommandRecorder::CommandRecorder
 * @param parent
 */
CommandRecorder::CommandRecorder(QObject *parent) : QObject(parent) {

}

/**
 * @brief CommandRecorder::getInstance
 * @return global unique instance
 */
CommandRecorder *CommandRecorder::getInstance() {

    instanceMutex.lock();
    if(!instance) {
        instance = new CommandRecorder;
    }
    instanceMutex.unlock();

    return instance;
}

/**
 * @brief CommandRecorder::eventFilter
 * @param obj
 * @param event
 * @return
 */
bool CommandRecorder::eventFilter(QObject *obj, QEvent *event) {

    Q_UNUSED(obj);

    if(event->type() == TextEditEvent::eventType) {
        auto ev = static_cast<TextEditEvent *>(event);
        qDebug() << tr("Custom Event! : %1").arg(ev->toString());
    }

    return false;
}
