
#ifndef MAIN_H_
#define MAIN_H_

#include <memory>

class QStringList;
class NeditServer;

class Main {
public:
    Main(const QStringList &args);
    ~Main();

private:
    std::unique_ptr<NeditServer> server;
};

#endif
