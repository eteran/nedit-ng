
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

#define EMIT_EVENT(name)                                \
    do {                                                \
        WindowMenuEvent menuEvent(QLatin1String(name)); \
        QApplication::sendEvent(this, &menuEvent);      \
    } while(0)

#define EMIT_EVENT_ARG_1(name, arg)                            \
    do {                                                       \
        WindowMenuEvent menuEvent(QLatin1String(name), {arg}); \
        QApplication::sendEvent(this, &menuEvent);             \
    } while(0)

#define EMIT_EVENT_ARG_2(name, arg1, arg2)                            \
    do {                                                              \
        WindowMenuEvent menuEvent(QLatin1String(name), {arg1, arg2}); \
        QApplication::sendEvent(this, &menuEvent);                    \
    } while(0)

#define EMIT_EVENT_ARG_3(name, arg1, arg2, arg3)                            \
    do {                                                                    \
        WindowMenuEvent menuEvent(QLatin1String(name), {arg1, arg2, arg3}); \
        QApplication::sendEvent(this, &menuEvent);                          \
    } while(0)

#define EMIT_EVENT_ARG_4(name, arg1, arg2, arg3, arg4)                            \
    do {                                                                          \
        WindowMenuEvent menuEvent(QLatin1String(name), {arg1, arg2, arg3, arg4}); \
        QApplication::sendEvent(this, &menuEvent);                                \
    } while(0)

#endif
