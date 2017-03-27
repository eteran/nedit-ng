
#ifndef COMMAND_RECORDER_H_
#define COMMAND_RECORDER_H_

#include <QObject>
#include <QEvent>

class CommandRecorder : public QObject {
	Q_OBJECT
public:
    static CommandRecorder *getInstance();

private:
    explicit CommandRecorder(QObject *parent = Q_NULLPTR);
	virtual ~CommandRecorder() = default;

public:
    virtual bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif
