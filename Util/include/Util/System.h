
#ifndef UTIL_SYSTEM_H_
#define UTIL_SYSTEM_H_

#include <QLatin1String>
#include <QString>

QLatin1String buildOperatingSystem();
QLatin1String buildArchitecture();
QString       buildCompiler();

#endif
