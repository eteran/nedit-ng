
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

class Tags {
    Q_DECLARE_TR_FUNCTIONS(Tags)

private:
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

    struct TagFile {
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
        int     posInf;
        int     index;
    };

public:
    static QList<Tag> LookupTag(const QString &name, SearchMode mode);
    static bool AddRelTagsFileEx(const QString &tagSpec, const QString &windowPath, SearchMode mode);
    static bool AddTagsFileEx(const QString &tagSpec, SearchMode mode);
    static bool DeleteTagsFileEx(const QString &tagSpec, SearchMode mode, bool force_unload);
    static bool fakeRegExSearchEx(view::string_view buffer, const QString &searchString, int64_t *startPos, int64_t *endPos);
    static int tagsShowCalltipEx(TextArea *area, const QString &text);
    static void showMatchingCalltipEx(TextArea *area, size_t i);

public:
    static QList<Tag> LookupTagFromList(std::deque<TagFile> *FileList, const QString &name, SearchMode mode);
    static QList<Tag> getTag(const QString &name, SearchMode mode);
    static int addTag(const QString &name, const QString &file, size_t lang, const QString &search, int posInf, const QString &path, int index);
    static bool searchLine(const std::string &line, const std::string &regex);

private:
    static QMultiHash<QString, Tag> *hashTableByType(SearchMode mode);
    static int loadTagsFile(const QString &tagSpec, int index, int recLevel);
    static int loadTipsFile(const QString &tipsFile, int index, int recLevel);
    static int nextTFBlock(std::istream &is, QString &header, QString &body, int *blkLine, int *currLine);
    static int scanETagsLine(const QString &line, const QString &tagPath, int index, QString &file, int recLevel);
    static std::deque<TagFile> *tagListByType(SearchMode mode);
    static int scanCTagsLine(const QString &line, const QString &tagPath, int index);
    static bool delTag(int index);

public:
    static std::deque<TagFile> TagsFileList; // list of loaded tags files
    static std::deque<TagFile> TipsFileList; // list of loaded calltips tag files

    static SearchMode searchMode;
    static QString    tagName;

public:
    static std::array<QString, MAXDUPTAGS> tagFiles;
    static std::array<QString, MAXDUPTAGS> tagSearch;
    static std::array<int,     MAXDUPTAGS> tagPosInf;

public:
    static bool           globAnchored;
    static int            globPos;
    static TipHAlignMode  globHAlign;
    static TipVAlignMode  globVAlign;
    static TipAlignStrict globAlignMode;
};

#endif
