
#include "CommandRecorder.h"
#include <QMutex>

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
