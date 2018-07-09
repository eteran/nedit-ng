
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
    static CommandRecorder *instance();

private:
    explicit CommandRecorder(QObject *parent = nullptr);
    ~CommandRecorder() noexcept override = default;

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
    QPointer<DocumentWidget> macroRecordWindow() const;

private:
    void lastActionHook(const TextEditEvent *ev);
    void lastActionHook(const WindowMenuEvent *ev);

public:
    // The last command executed (used by the Repeat command)
    QString lastCommand;    
    QString replayMacro;

private:
    QString macroRecordBuffer_;
    bool isRecording_ = false;

    // Window where macro recording is taking place
    QPointer<DocumentWidget> macroRecordWindow_;
};

#endif
