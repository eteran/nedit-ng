
#ifndef COMMAND_RECORDER_H_
#define COMMAND_RECORDER_H_

#include <QObject>
#include <QEvent>
#include <QString>
#include <QPointer>

class TextEditEvent;
class DocumentWidget;
class WindowMenuEvent;

class CommandRecorder : public QObject {
	Q_OBJECT
public:
    static CommandRecorder &getInstance();

private:
    explicit CommandRecorder(QObject *parent = nullptr);
    ~CommandRecorder() override = default;

public:
    bool eventFilter(QObject *obj, QEvent *event) override;

public:
    static QString escapeString(const QString &s);
    static QString quoteString(const QString &s);

public:
    void startRecording(DocumentWidget *document);
    void stopRecording();
    void cancelRecording();
    bool isRecording() const;
    void setRecording(bool enabled);

private:
    void lastActionHook(const TextEditEvent *ev);
    void lastActionHook(const WindowMenuEvent *ev);
    QString actionToString(const TextEditEvent *ev);
    QString actionToString(const WindowMenuEvent *ev);

    bool isMouseAction(const TextEditEvent *ev) const;
    bool isRedundantAction(const TextEditEvent *ev) const;
    bool isIgnoredAction(const TextEditEvent *ev) const;

    bool isMouseAction(const WindowMenuEvent *ev) const;
    bool isRedundantAction(const WindowMenuEvent *ev) const;
    bool isIgnoredAction(const WindowMenuEvent *ev) const;

public:
    // The last command executed (used by the Repeat command)
    QString lastCommand;    
    QString replayMacro;

    // Window where macro recording is taking place
    QPointer<DocumentWidget> macroRecordWindowEx;

private:
    QString macroRecordBuffer;
    bool isRecording_;
};

#endif
