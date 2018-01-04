
#ifndef NEDIT_SERVER_H_
#define NEDIT_SERVER_H_

#include <QObject>

class QLocalServer;
class QString;

class NeditServer : public QObject {
    Q_OBJECT

public:
    explicit NeditServer(QObject *parent = nullptr);
    virtual ~NeditServer() = default;

public Q_SLOTS:
    void newConnection();

private:
    QLocalServer *server_;


};

#endif
