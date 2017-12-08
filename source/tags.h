
#ifndef TAGS_H_
#define TAGS_H_

#include "CallTip.h"
#include <deque>

#include <QString>
#include <QDateTime>

class DocumentWidget;
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

extern std::deque<TagFile> TagsFileList; // list of loaded tags files
extern std::deque<TagFile> TipsFileList; // list of loaded calltips tag files

extern TagSearchMode  searchMode;
extern QString        tagName;

extern bool           globAnchored;
extern int            globPos;
extern TipHAlignMode  globHAlign;
extern TipVAlignMode  globVAlign;
extern TipAlignStrict globAlignMode;

bool AddRelTagsFileEx(const QString &tagSpec, const QString &windowPath, TagSearchMode file_type);

// tagSpec is a colon-delimited list of filenames 
bool AddTagsFileEx(const QString &tagSpec, TagSearchMode file_type);
int DeleteTagsFileEx(const QString &tagSpec, TagSearchMode file_type, bool force_unload);
int tagsShowCalltipEx(TextArea *area, const QString &text);

// Routines for handling tags or tips from the current selection

//  Display (possibly finding first) a calltip.  Search type can only be TIP or TIP_FROM_TAG here.
int ShowTipStringEx(DocumentWidget *document, const QString &text, bool anchored, int pos, bool lookup, TagSearchMode search_type, TipHAlignMode hAlign, TipVAlignMode vAlign, TipAlignStrict alignMode);

void editTaggedLocationEx(TextArea *area, int i);
void showMatchingCalltipEx(TextArea *area, int i);

int findAllMatchesEx(DocumentWidget *document, TextArea *area, const QString &string);

#endif
