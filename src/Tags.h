
#ifndef TAGS_H_
#define TAGS_H_

#include "CallTip.h"
#include "Util/QtHelper.h"

#include <QDateTime>
#include <QString>

#include <deque>
#include <string_view>

class TextArea;
class QTextStream;

namespace Tags {

Q_DECLARE_NAMESPACE_TR(Tags)

constexpr int MaxDupTags = 100;

// file_type and search_type arguments are to select between tips and tags,
// and should be one of TAG or TIP. TIP_FROM_TAG is for ShowTipString.
enum class SearchMode {
	None = -1,
	TAG,
	TIP_FROM_TAG,
	TIP
};

struct File {
	QString filename;
	QDateTime date;
	bool loaded;
	int index;
	int refcount; // Only tips files are refcounted, not tags files
};

struct Tag {
	QString name;
	QString file;
	QString searchString;
	QString path;
	size_t language;
	int64_t posInf;
	int index;
};

enum CalltipToken {
	TF_EOF,
	TF_BLOCK,
	TF_VERSION,
	TF_INCLUDE,
	TF_LANGUAGE,
	TF_ALIAS,
	TF_ERROR,
	TF_ERROR_EOF
};

QList<Tag> LookupTag(const QString &name, SearchMode mode);
bool AddRelativeTagsFile(const QString &tagSpec, const QString &windowPath, SearchMode mode);
bool AddTagsFile(const QString &tagSpec, SearchMode mode);
bool DeleteTagsFile(const QString &tagSpec, SearchMode mode, bool force_unload);
bool FakeRegexSearch(std::string_view buffer, const QString &searchString, int64_t *startPos, int64_t *endPos);
int TagsShowCalltip(TextArea *area, const QString &text);
void ShowMatchingCalltip(QWidget *parent, TextArea *area, int id);

QList<Tag> LookupTagFromList(std::deque<File> *FileList, const QString &name, SearchMode mode);
QList<Tag> GetTag(const QString &name, SearchMode mode);
int AddTag(const QString &name, const QString &file, size_t lang, const QString &search, int64_t posInf, const QString &path, int index);
bool SearchLine(const QString &line, const QRegularExpression &re);

extern std::deque<File> TagsFileList; // list of loaded tags files
extern std::deque<File> TipsFileList; // list of loaded calltips tag files

extern SearchMode searchMode;
extern QString tagName;

extern QString tagFiles[MaxDupTags];
extern QString tagSearch[MaxDupTags];
extern int64_t tagPosInf[MaxDupTags];

extern bool globAnchored;
extern CallTipPosition globPos;
extern TipHAlignMode globHAlign;
extern TipVAlignMode globVAlign;
extern TipAlignMode globAlignMode;
}

#endif
