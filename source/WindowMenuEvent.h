
#ifndef WINDOW_MENU_EVENT_H_
#define WINDOW_MENU_EVENT_H_

#include "MainWindow.h"
#include <QEvent>
#include <QString>
#include <QStringList>

class WindowMenuEvent : public QEvent {
public:
    static constexpr QEvent::Type eventType = static_cast<QEvent::Type>(QEvent::User + 2);

public:
    explicit WindowMenuEvent(const QString &macroString) : QEvent(eventType), macroString_(macroString) {
    }

    WindowMenuEvent(const QString &macroString, const QString &argument) : QEvent(eventType), macroString_(macroString), argument_(argument)  {
    }

public:
    QString argumentString() const {
        return QString();
    }

    QString toString() const {
        return QString(QLatin1String("%1(%2)")).arg(macroString_, argumentString());
    }

public:
    QString actionString() const {
        return macroString_;
    }

private:
    QString macroString_;
    QString argument_;
};

#endif
