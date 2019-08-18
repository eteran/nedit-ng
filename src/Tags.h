
#ifndef TAGS_H_
#define TAGS_H_

#include "CallTip.h"
#include "Util/QtHelper.h"

#include <deque>

#include <boost/utility/string_view.hpp>

#include <QString>
#include <QDateTime>

class TextArea;
class QTextStream;

namespace Tags {

Q_DECLARE_NAMESPACE_TR(Tags)

constexpr int MAXDUPTAGS = 100;

// file_type and search_type arguments are to select between tips and tags,
// and should be one of TAG or TIP.  TIP_FROM_TAG is for ShowTipString.
enum class SearchMode {
	None = -1,
	TAG,
	TIP_FROM_TAG,
	TIP
};

struct File {
	QString   filename;
	QDateTime date;
	bool      loaded;
	int       index;
	int       refcount; // Only tips files are refcounted, not tags files
};

struct Tag {
	QString name;
	QString file;
	QString searchString;
	QString path;
	size_t  language;
	int64_t posInf;
	int     index;
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


QList<Tag> lookupTag(const QString &name, SearchMode mode);
bool addRelTagsFile(const QString &tagSpec, const QString &windowPath, SearchMode mode);
bool addTagsFile(const QString &tagSpec, SearchMode mode);
bool deleteTagsFile(const QString &tagSpec, SearchMode mode, bool force_unload);
bool fakeRegExSearch(boost::string_view buffer, const QString &searchString, int64_t *startPos, int64_t *endPos);
int tagsShowCalltip(TextArea *area, const QString &text);
void showMatchingCalltip(QWidget *parent, TextArea *area, int id);

QList<Tag> lookupTagFromList(std::deque<File> *FileList, const QString &name, SearchMode mode);
QList<Tag> getTag(const QString &name, SearchMode mode);
int addTag(const QString &name, const QString &file, size_t lang, const QString &search, int64_t posInf, const QString &path, int index);
bool searchLine(const QString &line, const QRegularExpression &re);

extern std::deque<File> TagsFileList; // list of loaded tags files
extern std::deque<File> TipsFileList; // list of loaded calltips tag files

extern SearchMode searchMode;
extern QString    tagName;

extern QString tagFiles[MAXDUPTAGS];
extern QString tagSearch[MAXDUPTAGS];
extern int64_t tagPosInf[MAXDUPTAGS];

extern bool            globAnchored;
extern CallTipPosition globPos;
extern TipHAlignMode   globHAlign;
extern TipVAlignMode   globVAlign;
extern TipAlignMode    globAlignMode;
}

#endif
