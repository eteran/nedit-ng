
#ifndef FILEUTILS_H_
#define FILEUTILS_H_

#include <string>
#include "string_view.h"

class QString;

enum class FileFormats : int;

FileFormats FormatOfFileEx(view::string_view fileString);
bool ParseFilenameEx(const QString &fullname, QString *filename, QString *pathname);
QString ExpandTildeEx(const QString &pathname);
QString GetTrailingPathComponentsEx(const QString &path, int noOfComponents);
QString ReadAnyTextFileEx(const QString &fileName, bool forceNL);

void ConvertToDosFileStringEx(std::string &fileString);
void ConvertFromDosFileString(char *fileString, int64_t *length, char *pendingCR);
void ConvertFromDosFileString(char *fileString, size_t *length, char *pendingCR);
void ConvertFromDosFileStringEx(std::string *fileString, char *pendingCR);
void ConvertFromMacFileString(char *fileString, int64_t length);
void ConvertFromMacFileString(char *fileString, size_t length);
void ConvertFromMacFileStringEx(std::string *fileString);
void ConvertToMacFileStringEx(std::string &fileString);

QString NormalizePathnameEx(const QString &pathname);
QString CompressPathnameEx(const QString &pathname);

#endif
