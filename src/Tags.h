
#ifndef TAGS_H_
#define TAGS_H_

#include "CallTip.h"
#include "Util/string_view.h"

#include <deque>
#include <array>

#include <QString>
#include <QDateTime>
#include <QCoreApplication>

class TextArea;
class QTextStream;

class Tags {
	Q_DECLARE_TR_FUNCTIONS(Tags)

public:
	Tags() = delete;

public:
	static constexpr int MAXDUPTAGS = 100;

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

public:
	static QList<Tag> LookupTag(const QString &name, SearchMode mode);
	static bool AddRelTagsFileEx(const QString &tagSpec, const QString &windowPath, SearchMode mode);
	static bool AddTagsFileEx(const QString &tagSpec, SearchMode mode);
	static bool DeleteTagsFileEx(const QString &tagSpec, SearchMode mode, bool force_unload);
	static bool fakeRegExSearchEx(view::string_view buffer, const QString &searchString, int64_t *startPos, int64_t *endPos);
	static int tagsShowCalltipEx(TextArea *area, const QString &text);
	static void showMatchingCalltip(QWidget *parent, TextArea *area, int id);

public:
	static QList<Tag> LookupTagFromList(std::deque<File> *FileList, const QString &name, SearchMode mode);
	static QList<Tag> getTag(const QString &name, SearchMode mode);
	static int addTag(const QString &name, const QString &file, size_t lang, const QString &search, int64_t posInf, const QString &path, int index);	
	static bool searchLine(const QString &line, const QRegularExpression &re);

private:
	static QMultiHash<QString, Tag> *hashTableByType(SearchMode mode);
	static int loadTagsFile(const QString &tagSpec, int index, int recLevel);
	static int loadTipsFile(const QString &tipsFile, int index, int recLevel);
	static int nextTFBlock(QTextStream &is, QString &header, QString &body, int *blkLine, int *currLine);
	static int scanETagsLine(const QString &line, const QString &tagPath, int index, QString &file, int recLevel);
	static std::deque<File> *tagListByType(SearchMode mode);
	static int scanCTagsLine(const QString &line, const QString &tagPath, int index);
	static bool delTag(int index);

public:
	static std::deque<File> TagsFileList; // list of loaded tags files
	static std::deque<File> TipsFileList; // list of loaded calltips tag files

	static SearchMode searchMode;
	static QString    tagName;

public:
	static QString tagFiles[MAXDUPTAGS];
	static QString tagSearch[MAXDUPTAGS];
	static int64_t tagPosInf[MAXDUPTAGS];

public:
	static bool            globAnchored;
	static CallTipPosition globPos;
	static TipHAlignMode   globHAlign;
	static TipVAlignMode   globVAlign;
	static TipAlignMode    globAlignMode;
};

#endif
