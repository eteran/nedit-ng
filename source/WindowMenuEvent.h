
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

    WindowMenuEvent(const QString &macroString, const QStringList &arguments) : QEvent(eventType), macroString_(macroString), arguments_(arguments)  {
    }

public:
    QString argumentString() const {
        QStringList args;
        for(const QString &arg : arguments_) {
            // TODO(eteran): escape some characters as needed
            args << QLatin1Char('"') + arg + QLatin1Char('"');
        }
        return args.join(QLatin1String(","));
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
    QStringList arguments_;
};

#endif
