#ifndef NEDIT_SERVER_H_
#define NEDIT_SERVER_H_

#include <QObject>
#include <QString>

class NeditServer : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-BUS Interface", "com.nedit.IServer")
public:
    NeditServer(QObject *parent = 0);
    virtual ~NeditServer() = default;

public Q_SLOTS:
    void processCommand(const QString &command);


};

#endif
