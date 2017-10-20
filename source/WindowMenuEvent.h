
#ifndef WINDOW_MENU_EVENT_H_
#define WINDOW_MENU_EVENT_H_

#include <QEvent>
#include <QString>
#include <QStringList>

class WindowMenuEvent : public QEvent {
public:
    static constexpr QEvent::Type eventType = static_cast<QEvent::Type>(QEvent::User + 2);

public:
    explicit WindowMenuEvent(const QString &macroString);
    WindowMenuEvent(const QString &macroString, const QStringList &arguments);

public:
    QString argumentString() const;
    QString toString() const;
    QString actionString() const;

private:
    QString macroString_;
    QStringList arguments_;
};

#endif
