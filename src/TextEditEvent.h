
#ifndef TEXT_EDIT_EVENT_H_
#define TEXT_EDIT_EVENT_H_

#include "TextArea.h"

#include <QEvent>
#include <QString>

class TextEditEvent : public QEvent {
public:
	static constexpr auto eventType = static_cast<QEvent::Type>(QEvent::User + 1);

public:
	TextEditEvent(QString macroString, TextArea::EventFlags flags, QString argument);
	TextEditEvent(QString macroString, TextArea::EventFlags flags);

public:
	QString argumentString() const;
	QString toString() const;
	QString actionString() const;

private:
	QString macroString_;
	QString argument_;
	TextArea::EventFlags flags_;
};

#endif
