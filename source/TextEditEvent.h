
#ifndef TEXT_EDIT_EVENT_H_
#define TEXT_EDIT_EVENT_H_

#include "TextArea.h"
#include <QEvent>
#include <QString>
#include <QStringList>

class TextEditEvent : public QEvent {
public:
    static const QEvent::Type eventType = static_cast<QEvent::Type>(QEvent::User + 1);

public:
    TextEditEvent(const QString &macroString, const QString &arguments, TextArea::EventFlags flags) : QEvent(eventType), macroString_(macroString), arguments_(arguments), flags_(flags) {
    }

    TextEditEvent(const QString &macroString, TextArea::EventFlags flags) : QEvent(eventType), macroString_(macroString), flags_(flags) {
    }

public:
    QString toString() const {
        if(!arguments_.isEmpty()) {
            return QString(QLatin1String("%1(\"%2\")")).arg(macroString_, arguments_);
        } else {
            return QString(QLatin1String("%1()")).arg(macroString_);
        }
    }

private:
    QString              macroString_;
    QString              arguments_;
    TextArea::EventFlags flags_;
};

#endif
