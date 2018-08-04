
#ifndef MAIN_H_
#define MAIN_H_

#include <QCoreApplication>
#include <memory>

class QStringList;
class QString;
class NeditServer;

class Main {
	Q_DECLARE_TR_FUNCTIONS(Main)

public:
    explicit Main(const QStringList &args);
    ~Main();

private:
	bool checkDoMacroArg(const QString &macro) const;

private:
	std::unique_ptr<NeditServer> server_;
};

#endif
