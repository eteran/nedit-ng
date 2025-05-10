
#ifndef WINDOW_MENU_EVENT_H_
#define WINDOW_MENU_EVENT_H_

#include <QApplication>
#include <QEvent>
#include <QString>
#include <QStringList>

class WindowMenuEvent : public QEvent {
public:
	static constexpr auto eventType = static_cast<QEvent::Type>(QEvent::User + 2);

public:
	WindowMenuEvent(QString action, QStringList arguments);

public:
	QString argumentString() const;
	QString toString() const;
	QString actionString() const;

private:
	QString action_;
	QStringList arguments_;
};

template <class... Ts>
void EmitEvent(const char *name, Ts &&...args) {
	WindowMenuEvent menuEvent(QString::fromLatin1(name), {std::forward<Ts>(args)...});
	QApplication::sendEvent(qApp, &menuEvent);
}

#endif
