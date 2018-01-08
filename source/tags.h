
#ifndef TAGS_H_
#define TAGS_H_

#include "CallTip.h"
#include "Util/string_view.h"
#include <deque>

#include <QString>
#include <QDateTime>

constexpr int MAXDUPTAGS = 100;

class TextArea;

// file_type and search_type arguments are to select between tips and tags,
// and should be one of TAG or TIP.  TIP_FROM_TAG is for ShowTipString.
enum class TagSearchMode {
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

extern std::deque<TagFile> TagsFileList; // list of loaded tags files
extern std::deque<TagFile> TipsFileList; // list of loaded calltips tag files

extern TagSearchMode  searchMode;
extern QString        tagName;

extern bool           globAnchored;
extern int            globPos;
extern TipHAlignMode  globHAlign;
extern TipVAlignMode  globVAlign;
extern TipAlignStrict globAlignMode;

extern QString tagFiles[MAXDUPTAGS];
extern QString tagSearch[MAXDUPTAGS];
extern int tagPosInf[MAXDUPTAGS];

bool AddRelTagsFileEx(const QString &tagSpec, const QString &windowPath, TagSearchMode file_type);
bool AddTagsFileEx(const QString &tagSpec, TagSearchMode file_type);
int DeleteTagsFileEx(const QString &tagSpec, TagSearchMode file_type, bool force_unload);
int tagsShowCalltipEx(TextArea *area, const QString &text);
QList<Tag> LookupTag(const QString &name, TagSearchMode search_type);
void showMatchingCalltipEx(TextArea *area, int i);
bool fakeRegExSearchEx(view::string_view buffer, const QString &searchString, int64_t *startPos, int64_t *endPos);

#endif
