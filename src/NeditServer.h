
#ifndef NEDIT_SERVER_H_
#define NEDIT_SERVER_H_

#include <QObject>

class QLocalServer;
class QString;

class NeditServer final : public QObject {
	Q_OBJECT

public:
	explicit NeditServer(QObject *parent = nullptr);
	~NeditServer() override = default;

private:
	void newConnection();

private:
	QLocalServer *server_;
};

#endif
