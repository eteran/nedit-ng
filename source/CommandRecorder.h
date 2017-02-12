
#ifndef COMMAND_RECORDER_H_
#define COMMAND_RECORDER_H_

#include <QObject>

class CommandRecorder : public QObject {
	Q_OBJECT
public:
    static CommandRecorder *getInstance();

private:
    CommandRecorder(QObject *parent = Q_NULLPTR);
	virtual ~CommandRecorder() = default;
};

#endif
