
#ifndef WINDOW_MENU_EVENT_H_
#define WINDOW_MENU_EVENT_H_

#include <QEvent>
#include <QString>
#include <QStringList>
#include <QApplication>

class WindowMenuEvent : public QEvent {
public:
    static constexpr auto eventType = static_cast<QEvent::Type>(QEvent::User + 2);

public:
    WindowMenuEvent(const QString &macroString, const QStringList &arguments);

public:
    QString argumentString() const;
    QString toString() const;
    QString actionString() const;

private:
    QString macroString_;
    QStringList arguments_;
};

template<class ... Types>
void emit_event(const char *name, Types ... args) {
    WindowMenuEvent menuEvent(QString::fromLatin1(name), {args...});
    QApplication::sendEvent(qApp, &menuEvent);
}

#endif
