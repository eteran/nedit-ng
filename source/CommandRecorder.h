
#ifndef COMMAND_RECORDER_H_
#define COMMAND_RECORDER_H_

#include <QObject>
#include <QEvent>
#include <QString>

class TextEditEvent;

class CommandRecorder : public QObject {
	Q_OBJECT
public:
    static CommandRecorder *getInstance();

private:
    explicit CommandRecorder(QObject *parent = Q_NULLPTR);
	virtual ~CommandRecorder() = default;

public:
    virtual bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void lastActionHook(QObject *obj, const TextEditEvent *ev);
    QString actionToString(const TextEditEvent *ev);
    bool isMouseAction(const TextEditEvent *ev) const;
    bool isRedundantAction(const TextEditEvent *ev) const;
    bool isIgnoredAction(const TextEditEvent *ev) const;

public:
    // The last command executed (used by the Repeat command)
    QString lastCommand;
};

#endif
