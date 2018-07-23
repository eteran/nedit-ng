
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
	QPointer<DocumentWidget> macroRecordDocument() const;
	QString lastCommand() const;
	QString replayMacro() const;
	bool isRecording() const;
	void cancelRecording();
	void startRecording(DocumentWidget *document);
	void stopRecording();

private:
	void setRecording(bool enabled);
    void lastActionHook(const TextEditEvent *ev);
    void lastActionHook(const WindowMenuEvent *ev);

private:
	// The last command executed (used by the Repeat command)
	QString replayMacro_;

	QString lastCommand_;
	QString macroRecordBuffer_;
    bool isRecording_ = false;

    // Window where macro recording is taking place
	QPointer<DocumentWidget> macroRecordDocument_;
};

#endif
