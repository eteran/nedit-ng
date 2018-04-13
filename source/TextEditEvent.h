
#ifndef TEXT_EDIT_EVENT_H_
#define TEXT_EDIT_EVENT_H_

#include "TextArea.h"
#include <QEvent>
#include <QString>
#include <QStringList>

class TextEditEvent : public QEvent {
public:
    static constexpr auto eventType = static_cast<QEvent::Type>(QEvent::User + 1);

public:
    TextEditEvent(QString macroString, QString argument, TextArea::EventFlags flags);
    TextEditEvent(QString macroString, TextArea::EventFlags flags);

public:
    QString argumentString() const;
    QString toString() const;
    QString actionString() const;

private:
    QString              macroString_;
    QString              argument_;
    TextArea::EventFlags flags_;
};

#endif
