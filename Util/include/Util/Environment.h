
#ifndef ENVIRONMENT_H
#define ENVIRONMENT_H

#include <QString>

#include <string>

QString GetEnvironmentVariable(const QString &name);
QString GetEnvironmentVariable(const char *name);
QString GetEnvironmentVariable(const std::string &name);

#endif
