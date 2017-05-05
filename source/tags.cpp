/*******************************************************************************
*                                                                              *
* tags.c -- Nirvana editor tag file handling                                   *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* July, 1993                                                                   *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/


#include "tags.h"
#include "DialogDuplicateTags.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "calltips.h"
#include "file.h"
#include "nedit.h"
#include "preferences.h"
#include "search.h"
#include "selection.h"
#include "util/fileUtils.h"
#include "util/utils.h"
#include <QApplication>
#include <QMessageBox>
#include <cctype>
#include <cstdio>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unordered_map>


namespace {

constexpr const int MAXLINE                         = 2048;
constexpr const int MAX_TAG_LEN                     = 256;
constexpr const int MAXDUPTAGS                      = 100;
constexpr const int MAX_TAG_INCLUDE_RECURSION_LEVEL = 5;

/* Take this many lines when making a tip from a tag.
   (should probably be a language-dependent option, but...) */
constexpr const int TIP_DEFAULT_LINES = 4;

}

struct Tag;

/*
**  contents of                   tag->searchString   | tag->posInf
**    ctags, line num specified:  ""                  | line num
**    ctags, search expr specfd:  ctags search expr   | -1
**    etags  (emacs tags)         etags search string | search start pos
*/

enum searchDirection { FORWARD, BACKWARD };

static int loadTagsFile(const QString &tagSpec, int index, int recLevel);
static int fakeRegExSearchEx(view::string_view buffer, const char *searchString, int *startPos, int *endPos);
static void updateMenuItems();
static int addTag(const char *name, const char *file, int lang, const char *search, int posInf, const char *path, int index);
static bool delTag(int index);
static QList<Tag> getTag(const char *name, int search_type);
static void createSelectMenuEx(DocumentWidget *document, TextArea *area, const QVector<char *> &args);
static QList<Tag> LookupTag(const char *name, int search_type);

static int searchLine(char *line, const char *regex);
static void rstrip(char *dst, const char *src);
static int nextTFBlock(FILE *fp, char *header, char **tiptext, int *lineAt, int *lineNo);
static int loadTipsFile(const QString &tipsFile, int index, int recLevel);
static QMultiHash<QString, Tag> *hashTableByType(int type);
static QList<tagFile> *tagListByType(int type);

struct Tag {
    std::string name;
    std::string file;
    int language;
    std::string searchString; // see comment below
    int posInf;               // see comment below
    std::string path;
    int index;
};

/* Hash table of tags, implemented as an array.  Each bin contains a
    nullptr-terminated linked list of parsed tags */
QMultiHash<QString, Tag> Tags;

// list of loaded tags files 
QList<tagFile> TagsFileList;

// Hash table of calltip tags 
QMultiHash<QString, Tag> Tips;
QList<tagFile> TipsFileList;

/* These are all transient global variables -- they don't hold any state
    between tag/tip lookups */
int searchMode = TAG;
const char *tagName;
static char tagFiles[MAXDUPTAGS][MAXPATHLEN];
static char tagSearch[MAXDUPTAGS][MAXPATHLEN];
static int tagPosInf[MAXDUPTAGS];

bool globAnchored;
int globPos;
int globHAlign;
int globVAlign;
int globAlignMode;

// A wrapper for calling TextDShowCalltip
int tagsShowCalltipEx(TextArea *area, const QString &text) {
    if (!text.isNull()) {
        return area->TextDShowCalltip(text, globAnchored, globPos, globHAlign, globVAlign, globAlignMode);
    } else {
        return 0;
    }
}

static QMultiHash<QString, Tag> *hashTableByType(int type) {
    if (type == TIP) {
        return &Tips;
    } else {
        return &Tags;
    }
}

static QList<tagFile> *tagListByType(int type) {
    if (type == TAG) {
        return &TagsFileList;
    } else {
        return &TipsFileList;
    }
}

static QList<Tag> getTagFromTable(QMultiHash<QString, Tag> *table, const char *name) {
    return table->values(QString::fromLatin1(name));
}

//      Retrieve a Tag structure from the hash table 
static QList<Tag> getTag(const char *name, int search_type) {

	if (search_type == TIP) {
        return getTagFromTable(&Tips, name);
	} else {
        return getTagFromTable(&Tags, name);
	}	
}

/* Add a tag specification to the hash table
**   Return Value:  0 ... tag already existing, spec not added
**                  1 ... tag spec is new, added.
**   (We don't return boolean as the return value is used as counter increment!)
**
*/
static int addTag(const char *name, const char *file, int lang, const char *search, int posInf, const char *path, int index) {

    QMultiHash<QString, Tag> *table;

	if (searchMode == TIP) {
        table = &Tips;
    } else {
        table = &Tags;
	}

    QString newFile;
	if (*file == '/') {
        newFile = QString::fromLatin1(file);
	} else {
        newFile = QString(QLatin1String("%1%2")).arg(QString::fromLatin1(path), QString::fromLatin1(file));
	}

    newFile = NormalizePathnameEx(newFile);

    QList<Tag> tags = table->values(QString::fromLatin1(name));
    for(const Tag &t : tags) {

        if (lang != t.language) {
			continue;
        }

        if (search != t.searchString) {
			continue;
        }

        if (posInf != t.posInf) {
			continue;
        }

        Q_ASSERT(!t.file.empty());

        if (t.file[0] == '/' && (newFile != QString::fromStdString(t.file))) {
			continue;
        }

        if (t.file[0] != '/') {

            auto tmpFile = QString(QLatin1String("%1%2")).arg(QString::fromStdString(t.path), QString::fromStdString(t.file));

            tmpFile = NormalizePathnameEx(tmpFile);

            if(newFile != tmpFile) {
                continue;
            }
		}
        return 0;
    }
	
    Tag t;
    t.name         = name;
    t.file         = file;
    t.language     = lang;
    t.searchString = search;
    t.posInf       = posInf;
    t.path         = path;
    t.index        = index;

    table->insert(QString::fromLatin1(name), t);
    return 1;
}

/*  Delete a tag from the cache.
 *  Search is limited to valid matches of 'name','file', 'search', posInf, and 'index'.
 *  EX: delete all tags matching index 2 ==>
 *                      delTag(tagname,nullptr,-2,nullptr,-2,2);
 *  (posInf = -2 is an invalid match, posInf range: -1 .. +MAXINT,
     lang = -2 is also an invalid match)
 */
static bool delTag(int index) {
    int del = 0;

    QMultiHash<QString, Tag> *table = hashTableByType(searchMode);

    if(table->isEmpty()) {
        return false;
    }

    for(auto it = table->begin(); it != table->end(); ) {
        if(it->index == index) {
            it = table->erase(it);
            ++del;
        } else {
            ++it;
        }
    }

    return del > 0;
}

// used  in AddRelTagsFile and AddTagsFile
static int tagFileIndex = 0;

/*
** AddRelTagsFile():  Rescan tagSpec for relative tag file specs
** (not starting with [/~]) and extend the tag files list if in
** windowPath a tags file matching the relative spec has been found.
*/
bool AddRelTagsFileEx(const QString &tagSpec, const QString &windowPath, int file_type) {

    bool added = false;


    searchMode = file_type;
    QList<tagFile> *FileList = tagListByType(searchMode);

    if(tagSpec.isEmpty()) {
        return false;
    }

    QStringList filenames = tagSpec.split(QLatin1Char(':'));

    for(const QString &filename : filenames) {
        if(filename.startsWith(QLatin1Char('/')) || filename.startsWith(QLatin1Char('~'))) {
            continue;
        }

        QString pathName;
        if(!windowPath.isEmpty()) {
            pathName = QString(QLatin1String("%1/%2")).arg(windowPath, filename);
        } else {
            pathName = QString(QLatin1String("%1/%2")).arg(GetCurrentDirEx(), filename);
        }

        pathName = NormalizePathnameEx(pathName);

        auto it = std::find_if(FileList->begin(), FileList->end(), [pathName](const tagFile &tag) {
            return tag.filename == pathName;
        });

        // if we found an entry with the same pathname, we're done..
        if (it != FileList->end()) {
            added = true;
            continue;
        }

        // or if the file isn't found...
        struct stat statbuf;
		if (::stat(pathName.toLatin1().data(), &statbuf) != 0) {
            continue;
        }

        tagFile tag;
        tag.filename = pathName;
        tag.loaded   = false;
        tag.date     = statbuf.st_mtime;
        tag.index    = ++tagFileIndex;
        tag.refcount = 1; // NOTE(eteran): added just so there aren't any uninitialized members

        FileList->push_front(tag);
        added = true;
    }

    updateMenuItems();
    return added;
}

/*
**  AddTagsFile():  Set up the the list of tag files to manage from a file spec.
**  The file spec comes from the X-Resource Nedit.tags: It can list multiple
**  tags files, specified by separating them with colons. The .Xdefaults would
**  look like this:
**    Nedit.tags: <tagfile1>:<tagfile2>
**  Returns True if all files were found in the FileList or loaded successfully,
**  FALSE otherwise.
*/
bool AddTagsFileEx(const QString &tagSpec, int file_type) {

    bool added = true;

    searchMode = file_type;
    QList<tagFile> *FileList = tagListByType(searchMode);

    if(tagSpec.isEmpty()) {
        return false;
    }

    QStringList filenames = tagSpec.split(QLatin1Char(':'));

    for(const QString &filename : filenames) {

		QString pathName;
        if(!filename.startsWith(QLatin1Char('/'))) {
			pathName = QString(QLatin1String("%1/%2")).arg(GetCurrentDirEx(), filename);
        } else {
			pathName = filename;
        }

		pathName = NormalizePathnameEx(pathName);

        auto it = std::find_if(FileList->begin(), FileList->end(), [pathName](const tagFile &tag) {
			return tag.filename == pathName;
        });

        if (it != FileList->end()) {
            /* This file is already in the list.  It's easiest to just
                refcount all tag/tip files even though we only actually care
                about tip files. */
            ++(it->refcount);
            added = true;
            continue;
        }

        struct stat statbuf;
		if (::stat(pathName.toLatin1().data(), &statbuf) != 0) {
            // Problem reading this tags file.  Return FALSE
            added = false;
            continue;
        }

        tagFile tag;
		tag.filename = pathName;
        tag.loaded   = false;
        tag.date     = statbuf.st_mtime;
        tag.index    = ++tagFileIndex;
        tag.refcount = 1;

        FileList->push_front(tag);
    }

    updateMenuItems();
    return added;
}

/* Un-manage a colon-delimited set of tags files
 * Return TRUE if all files were found in the FileList and unloaded, FALSE
 * if any file was not found in the FileList.
 * "file_type" is either TAG or TIP
 * If "force_unload" is true, a calltips file will be deleted even if its
 * refcount is nonzero.
 */
int DeleteTagsFileEx(const QString &tagSpec, int file_type, bool force_unload) {

    if(tagSpec.isEmpty()) {
        return false;
    }

    searchMode = file_type;
    QList<tagFile> *FileList = tagListByType(searchMode);

    bool removed = true;

    QStringList filenames = tagSpec.split(QLatin1Char(':'));

    for(const QString &filename : filenames) {

        QString pathName;
        if(!filename.startsWith(QLatin1Char('/'))) {
            pathName = QString(QLatin1String("%1/%2")).arg(GetCurrentDirEx(), filename);
        } else {
            pathName = filename;
        }

        pathName = NormalizePathnameEx(pathName);

        auto it = FileList->begin();
        while(it != FileList->end()) {
            tagFile &t = *it;

            if (t.filename != pathName) {
                ++it;
                continue;
            }

            // Don't unload tips files with nonzero refcounts unless forced
            if (searchMode == TIP && !force_unload && --t.refcount > 0) {
                break;
            }

            if (t.loaded) {
                delTag(t.index);
            }

            FileList->erase(it);

            updateMenuItems();
            break;
        }

        // If any file can't be removed, return false
        if (it == FileList->end()) {
            removed = false;
        }
    }

    return removed != 0;
}

/*
** Update the "Find Definition", "Unload Tags File", "Show Calltip",
** and "Unload Calltips File" menu items in the existing windows.
*/
static void updateMenuItems() {
    bool tipStat = false;
    bool tagStat = false;

    if (!TipsFileList.isEmpty()) {
        tipStat = true;
    }

    if (!TagsFileList.isEmpty()) {
        tagStat = true;
    }

    for(MainWindow *window : MainWindow::allWindows()) {
        window->ui.action_Unload_Calltips_File->setEnabled(tipStat);
        window->ui.action_Unload_Tags_File->setEnabled(tagStat);
        window->ui.action_Show_Calltip->setEnabled(tipStat || tagStat);
        window->ui.action_Find_Definition->setEnabled(tagStat);
    }
}

/*
** Scans one <line> from a ctags tags file (<index>) in tagPath.
** Return value: Number of tag specs added.
*/
static int scanCTagsLine(const char *line, const char *tagPath, int index) {
	char name[MAXLINE];
	char searchString[MAXLINE];
	char file[MAXPATHLEN];
	char *posTagRENull;
	int pos;

	int nRead = sscanf(line, "%s\t%s\t%[^\n]", name, file, searchString);
	if (nRead != 3) {
		return 0;
	}

	if (*name == '!') {
		return 0;
	}

	/*
	** Guess the end of searchString:
	** Try to handle original ctags and exuberant ctags format:
	*/
	if (searchString[0] == '/' || searchString[0] == '?') {

		pos = -1; // "search expr without pos info" 

		/* Situations: /<ANY expr>/\0
		**             ?<ANY expr>?\0          --> original ctags
		**             /<ANY expr>/;"  <flags>
		**             ?<ANY expr>?;"  <flags> --> exuberant ctags
		*/
		char *posTagREEnd = strrchr(searchString, ';');
		posTagRENull = strchr(searchString, 0);
		if (!posTagREEnd || (posTagREEnd[1] != '"') || (posTagRENull[-1] == searchString[0])) {
			//  -> original ctags format = exuberant ctags format 1 
			posTagREEnd = posTagRENull;
		} else {
			// looks like exuberant ctags format 2 
			*posTagREEnd = '\0';
		}

		/*
		** Hide the last delimiter:
		**   /<expression>/    becomes   /<expression>
		**   ?<expression>?    becomes   ?<expression>
		** This will save a little work in fakeRegExSearch.
		*/
		if (posTagREEnd > (searchString + 2)) {
			posTagREEnd--;
			if (searchString[0] == *posTagREEnd)
				*posTagREEnd = 0;
		}
	} else {
		pos = atoi(searchString);
		*searchString = 0;
	}
	// No ability to read language mode right now 
	return addTag(name, file, PLAIN_LANGUAGE_MODE, searchString, pos, tagPath, index);
}

/*
 * Scans one <line> from an etags (emacs) tags file (<index>) in tagPath.
 * recLevel = current recursion level for tags file including
 * file = destination definition file. possibly modified. len=MAXPATHLEN!
 * Return value: Number of tag specs added.
 */
static int scanETagsLine(const char *line, const char *tagPath, int index, char *file, int recLevel) {
	char name[MAXLINE], searchString[MAXLINE];	
	int pos;
	int len;
	const char *posDEL;
	const char *posSOH;
	const char *posCOM;

	// check for destination file separator  
	if (line[0] == 12) { // <np> 
		*file = 0;
		return 0;
	}

	// check for standard definition line 
	posDEL = strchr(line, '\177');
	posSOH = strchr(line, '\001');
	posCOM = strrchr(line, ',');
	if (*file && posDEL && (posSOH > posDEL) && (posCOM > posSOH)) {
		// exuberant ctags -e style  
		len = std::min<int>(MAXLINE - 1, posDEL - line);
		strncpy(searchString, line, len);
		searchString[len] = 0;
		len = std::min<int>(MAXLINE - 1, (posSOH - posDEL) - 1);
		strncpy(name, posDEL + 1, len);
		name[len] = 0;
		pos = atoi(posCOM + 1);
		// No ability to set language mode for the moment 
		return addTag(name, file, PLAIN_LANGUAGE_MODE, searchString, pos, tagPath, index);
	}
	if (*file && posDEL && (posCOM > posDEL)) {
		// old etags style, part  name<soh>  is missing here! 
		len = std::min<int>(MAXLINE - 1, posDEL - line);
		strncpy(searchString, line, len);
		searchString[len] = 0;
		// guess name: take the last alnum (plus _) part of searchString 
		while (--len >= 0) {
			if (isalnum((uint8_t)searchString[len]) || (searchString[len] == '_'))
				break;
		}
		if (len < 0)
			return 0;
		pos = len;
		while (pos >= 0 && (isalnum((uint8_t)searchString[pos]) || (searchString[pos] == '_')))
			pos--;
		strncpy(name, searchString + pos + 1, len - pos);
		name[len - pos] = 0; // name ready 
		pos = atoi(posCOM + 1);
		return addTag(name, file, PLAIN_LANGUAGE_MODE, searchString, pos, tagPath, index);
	}
	// check for destination file spec 
	if (*line && posCOM) {
		len = std::min<int>(MAXPATHLEN - 1, posCOM - line);
		strncpy(file, line, len);
		file[len] = 0;
		// check if that's an include file ... 
		if (!(strncmp(posCOM + 1, "include", 7))) {
			if (*file != '/') {
				if ((strlen(tagPath) + strlen(file)) >= MAXPATHLEN) {
					qWarning("tags.c: MAXPATHLEN overflow");
					*file = 0; // invalidate 
					return 0;
				}
				
				char incPath[MAXPATHLEN];
				snprintf(incPath, sizeof(incPath), "%s%s", tagPath, file);

				CompressPathname(incPath);
                return loadTagsFile(QString::fromLatin1(incPath), index, recLevel + 1);
			} else {
                return loadTagsFile(QString::fromLatin1(file), index, recLevel + 1);
			}
		}
	}
	return 0;
}

// Tag File Type 
enum TFT { TFT_CHECK, TFT_ETAGS, TFT_CTAGS };

/*
** Loads tagsFile into the hash table.
** Returns the number of added tag specifications.
*/
static int loadTagsFile(const QString &tagsFile, int index, int recLevel) {
	FILE *fp = nullptr;
	char line[MAXLINE];
    char file[MAXPATHLEN];
    QString tagPath;
	int nTagsAdded = 0;
	int tagFileType = TFT_CHECK;

	if (recLevel > MAX_TAG_INCLUDE_RECURSION_LEVEL) {
		return 0;
	}
	/* the path of the tags file must be resolved to find the right files:
	 * definition source files are (in most cases) specified relatively inside
	 * the tags file to the tags files directory.
	 */
    QString resolvedTagsFile = ResolvePathEx(tagsFile);
    if (resolvedTagsFile.isNull()) {
		return 0;
	}

	// Open the file 
    if ((fp = fopen(resolvedTagsFile.toLatin1().data(), "r")) == nullptr) {
		return 0;
	}

    ParseFilenameEx(resolvedTagsFile, nullptr, &tagPath);

	// Read the file and store its contents 
	while (fgets(line, MAXLINE, fp)) {

		/* This might take a while if you have a huge tags file (like I do)..
		   keep the windows up to date and post a busy cursor so the user
		   doesn't think we died. */

        MainWindow::AllWindowsBusyEx(QLatin1String("Loading tags file..."));

		/* the first character in the file decides if the file is treat as
		   etags or ctags file.
		 */
		if (tagFileType == TFT_CHECK) {
			if (line[0] == 12) // <np> 
				tagFileType = TFT_ETAGS;
			else
				tagFileType = TFT_CTAGS;
		}
		if (tagFileType == TFT_CTAGS) {
            nTagsAdded += scanCTagsLine(line, tagPath.toLatin1().data(), index);
		} else {
            nTagsAdded += scanETagsLine(line, tagPath.toLatin1().data(), index, file, recLevel);
		}
	}
	fclose(fp);

    MainWindow::AllWindowsUnbusyEx();
	return nTagsAdded;
}

static QList<Tag> LookupTagFromList(QList<tagFile> *FileList, const char *name, int search_type) {

	/*
	** Go through the list of all tags Files:
	**   - load them (if not already loaded)
	**   - check for update of the tags file and reload it in that case
	**   - save the modification date of the tags file
	**
	** Do this only as long as name != nullptr, not for sucessive calls
	** to find multiple tags specs.
	**
	*/
	if (name) {
        for(tagFile &tf : *FileList) {

			struct stat statbuf;
			int load_status;		
		
            if (tf.loaded) {
                if (stat(tf.filename.toLatin1().data(), &statbuf) != 0) { //
					qWarning("NEdit: Error getting status for tag file %s", tf.filename.toLatin1().data());
				} else {
                    if (tf.date == statbuf.st_mtime) {
						// current tags file tf is already loaded and up to date 
						continue;
					}
				}
				// tags file has been modified, delete it's entries and reload it 
                delTag(tf.index);
			}

			// If we get here we have to try to (re-) load the tags file 
            if (FileList == &TipsFileList) {
                load_status = loadTipsFile(tf.filename, tf.index, 0);
            } else {
                load_status = loadTagsFile(tf.filename, tf.index, 0);
			}

			if (load_status) {
                if (stat(tf.filename.toLatin1().data(), &statbuf) != 0) {
                    if (!tf.loaded) {
						// if tf->loaded == true we already have seen the error msg 
						qWarning("NEdit: Error getting status for tag file %s", tf.filename.toLatin1().data());
					}
				} else {
                    tf.date = statbuf.st_mtime;
				}
                tf.loaded = true;
			} else {
                tf.loaded = false;
			}
		}
	}

    return getTag(name, search_type);
}

/*
** Given a tag name, lookup the file and path of the definition
** and the proper search string. Returned strings are pointers
** to internal storage which are valid until the next loadTagsFile call.
**
** Invocation with name != nullptr (containing the searched definition)
**    --> returns first definition of  name
** Successive invocation with name == nullptr
**    --> returns further definitions (resulting from multiple tags files)
**
** Return Value: TRUE:  tag spec found
**               FALSE: no (more) definitions found.
*/
static QList<Tag> LookupTag(const char *name, int search_type) {

	searchMode = search_type;
	if (searchMode == TIP) {
        return LookupTagFromList(&TipsFileList, name, search_type);
	} else {
        return LookupTagFromList(&TagsFileList, name, search_type);
	}
}

/*
** Try to display a calltip
**  anchored:       If true, tip appears at position pos
**  lookup:         If true, text is considered a key to be searched for in the
**                  tip and/or tag database depending on search_type
**  search_type:    Either TIP or TIP_FROM_TAG
*/
int ShowTipStringEx(DocumentWidget *window, const char *text, bool anchored, int pos, bool lookup, int search_type, int hAlign, int vAlign, int alignMode) {
    if (search_type == TAG) {
        return 0;
    }

    // So we don't have to carry all of the calltip alignment info around
    globAnchored  = anchored;
    globPos       = pos;
    globHAlign    = hAlign;
    globVAlign    = vAlign;
    globAlignMode = alignMode;

    // If this isn't a lookup request, just display it.
    if (!lookup)
        return tagsShowCalltipEx(window->toWindow()->lastFocus_, QString::fromLatin1(text));
    else
		return window->findDef(window->toWindow()->lastFocus_, QString::fromLatin1(text), static_cast<Mode>(search_type));
}

/*
** ctags search expressions are literal strings with a search direction flag,
** line starting "^" and ending "$" delimiters. This routine translates them
** into NEdit compatible regular expressions and does the search.
** Etags search expressions are plain literals strings, which
*/
static int fakeRegExSearchEx(view::string_view in_buffer, const char *searchString, int *startPos, int *endPos) {
	int found, searchStartPos;
	SearchDirection dir;
	int ctagsMode;
	char searchSubs[3 * MAXLINE + 3];
	char *outPtr;
	const char *inPtr;

	view::string_view fileString = in_buffer;

	// determine search direction and start position 
	if (*startPos != -1) { // etags mode! 
		dir = SEARCH_FORWARD;
		searchStartPos = *startPos;
		ctagsMode = 0;
	} else if (searchString[0] == '/') {
		dir = SEARCH_FORWARD;
		searchStartPos = 0;
		ctagsMode = 1;
	} else if (searchString[0] == '?') {
		dir = SEARCH_BACKWARD;
		// searchStartPos = window->buffer_->length; 
		searchStartPos = fileString.size();
		ctagsMode = 1;
	} else {
		qWarning("NEdit: Error parsing tag file search string");
        return false;
	}

	// Build the search regex. 
	outPtr = searchSubs;
	if (ctagsMode) {
		inPtr = searchString + 1; // searchString[0] is / or ? --> search dir 
		if (*inPtr == '^') {
			// If the first char is a caret then it's a RE line start delim 
			*outPtr++ = *inPtr++;
		}
	} else { // etags mode, no search dir spec, no leading caret 
		inPtr = searchString;
	}
	while (*inPtr) {
		if ((*inPtr == '\\' && inPtr[1] == '/') || (*inPtr == '\r' && inPtr[1] == '$' && !inPtr[2])) {
			/* Remove:
			   - escapes (added by standard and exuberant ctags) from slashes
			   - literal CRs generated by standard ctags for DOSified sources
			*/
			inPtr++;
		} else if (strchr("()-[]<>{}.|^*+?&\\", *inPtr) || (*inPtr == '$' && (inPtr[1] || (!ctagsMode)))) {
			/* Escape RE Meta Characters to match them literally.
			   Don't escape $ if it's the last charcter of the search expr
			   in ctags mode; always escape $ in etags mode.
			 */
			*outPtr++ = '\\';
			*outPtr++ = *inPtr++;
		} else if (isspace((uint8_t)*inPtr)) { // col. multiple spaces 
			*outPtr++ = '\\';
			*outPtr++ = 's';
			*outPtr++ = '+';
			do {
				inPtr++;
			} while (isspace((uint8_t)*inPtr));
		} else { // simply copy all other characters 
			*outPtr++ = *inPtr++;
		}
	}
	*outPtr = 0; // Terminate searchSubs 

    found = SearchString(fileString, QString::fromLatin1(searchSubs), dir, SEARCH_REGEX, false, searchStartPos, startPos, endPos, nullptr, nullptr, nullptr);

	if (!found && !ctagsMode) {
		/* position of the target definition could have been drifted before
		   startPos, if nothing has been found by now try searching backward
		   again from startPos.
		*/
        found = SearchString(fileString, QString::fromLatin1(searchSubs), SEARCH_BACKWARD, SEARCH_REGEX, false, searchStartPos, startPos, endPos, nullptr, nullptr, nullptr);
	}

	// return the result 
	if (found) {
		// *startPos and *endPos are set in SearchString
        return true;
	} else {
		// startPos, endPos left untouched by SearchString if search failed. 
		QApplication::beep();
        return false;
	}
}

/*      Finds all matches and handles tag "collisions". Prompts user with a
        list of collided tags in the hash table and allows the user to select
        the correct one. */
int findAllMatchesEx(DocumentWidget *document, TextArea *area, const char *string) {

    QString filename;
    QString pathname;

    int i;
    int pathMatch = 0;
    int samePath = 0;    
    int nMatches = 0;

    // verify that the string is reasonable as a tag
    if (*string == '\0' || strlen(string) > MAX_TAG_LEN) {
        QApplication::beep();
        return -1;
    }
    tagName = string;

    QList<Tag> tags = LookupTag(string, searchMode);

    // First look up all of the matching tags
    for(const Tag &tag : tags) {

        std::string fileToSearch = tag.file;
        std::string searchString = tag.searchString;
        std::string tagPath      = tag.path;
        int langMode             = tag.language;
        int startPos             = tag.posInf;

        /*
        ** Skip this tag if it has a language mode that doesn't match the
        ** current language mode, but don't skip anything if the window is in
        ** PLAIN_LANGUAGE_MODE.
        */
        if (document->languageMode_ != PLAIN_LANGUAGE_MODE && GetPrefSmartTags() && langMode != PLAIN_LANGUAGE_MODE && langMode != document->languageMode_) {
            continue;
        }

        if (fileToSearch[0] == '/') {
            strcpy(tagFiles[nMatches], fileToSearch.c_str());
        } else {
            sprintf(tagFiles[nMatches], "%s%s", tagPath.c_str(), fileToSearch.c_str());
        }

        strcpy(tagSearch[nMatches], searchString.c_str());
        tagPosInf[nMatches] = startPos;

        ParseFilenameEx(QString::fromLatin1(tagFiles[nMatches]), &filename, &pathname);

        // Is this match in the current file?  If so, use it!
        if (GetPrefSmartTags() && document->filename_ == filename && document->path_ == pathname) {
            if (nMatches) {
                strcpy(tagFiles[0],  tagFiles[nMatches]);
                strcpy(tagSearch[0], tagSearch[nMatches]);
                tagPosInf[0] = tagPosInf[nMatches];
            }
            nMatches = 1;
            break;
        }

        // Is this match in the same dir. as the current file?
        if (document->path_ == pathname) {
            samePath++;
            pathMatch = nMatches;
        }

        if (++nMatches >= MAXDUPTAGS) {
            QMessageBox::warning(nullptr /*parent*/, QLatin1String("Tags"), QString(QLatin1String("Too many duplicate tags, first %1 shown")).arg(MAXDUPTAGS));
            break;
        }
    }

    // Did we find any matches?
    if (!nMatches) {
        return 0;
    }

    // Only one of the matches is in the same dir. as this file.  Use it.
    if (GetPrefSmartTags() && samePath == 1 && nMatches > 1) {
        strcpy(tagFiles[0], tagFiles[pathMatch]);
        strcpy(tagSearch[0], tagSearch[pathMatch]);
        tagPosInf[0] = tagPosInf[pathMatch];
        nMatches = 1;
    }

    /*  If all of the tag entries are the same file, just use the first.
     */
    if (GetPrefSmartTags()) {
        for (i = 1; i < nMatches; i++) {
            if (strcmp(tagFiles[i], tagFiles[i - 1])) {
                break;
            }
        }

        if (i == nMatches) {
            nMatches = 1;
        }
    }

    if (nMatches > 1) {

        QVector<char *> dupTagsList;

        for (i = 0; i < nMatches; i++) {

            char temp[32 + 2 * MAXPATHLEN + MAXLINE];

            ParseFilenameEx(QString::fromLatin1(tagFiles[i]), &filename, &pathname);
            if ((i < nMatches - 1 && !strcmp(tagFiles[i], tagFiles[i + 1])) || (i > 0 && !strcmp(tagFiles[i], tagFiles[i - 1]))) {


                if (*(tagSearch[i]) && (tagPosInf[i] != -1)) { // etags
                    sprintf(temp, "%2d. %s%s %8i %s", i + 1, pathname.toLatin1().data(), filename.toLatin1().data(), tagPosInf[i], tagSearch[i]);
                } else if (*(tagSearch[i])) { // ctags search expr
                    sprintf(temp, "%2d. %s%s          %s", i + 1, pathname.toLatin1().data(), filename.toLatin1().data(), tagSearch[i]);
                } else { // line number only
                    sprintf(temp, "%2d. %s%s %8i", i + 1, pathname.toLatin1().data(), filename.toLatin1().data(), tagPosInf[i]);
                }
            } else {
                sprintf(temp, "%2d. %s%s", i + 1, pathname.toLatin1().data(), filename.toLatin1().data());
            }

            auto str = new char[strlen(temp) + 1];
            strcpy(str, temp);
            dupTagsList.push_back(str);
        }

        createSelectMenuEx(document, area, dupTagsList);

        qDeleteAll(dupTagsList);
        return 1;
    }

    /*
    **  No need for a dialog list, there is only one tag matching --
    **  Go directly to the tag
    */
    if (searchMode == TAG) {
        editTaggedLocationEx(area, 0);
    } else {
        showMatchingCalltipEx(area, 0);
    }
    return 1;
}

/*
 * Given a \0 terminated string and a position, advance the position
 * by n lines, where line separators (for now) are \n.  If the end of
 * string is reached before n lines, return the number of lines advanced,
 * else normally return -1.
 */
static int moveAheadNLines(const char *str, int *pos, int n) {
	int i = n;
	while (str[*pos] != '\0' && n > 0) {
		if (str[*pos] == '\n')
			--n;
		++(*pos);
	}
	if (n == 0)
		return -1;
	else
		return i - n;
}

/*
** Show the calltip specified by tagFiles[i], tagSearch[i], tagPosInf[i]
** This reads from either a source code file (if searchMode == TIP_FROM_TAG)
** or a calltips file (if searchMode == TIP).
*/
void showMatchingCalltipEx(TextArea *area, int i) {
    try {
        int startPos = 0;
        int tipLen;
        int endPos = 0;
        struct stat statbuf;

        // 1. Open the target file
        NormalizePathname(tagFiles[i]);
        FILE *fp = fopen(tagFiles[i], "r");
        if(!fp) {
            QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error opening File"), QString(QLatin1String("Error opening %1")).arg(QString::fromLatin1(tagFiles[i])));
            return;
        }

        if (fstat(fileno(fp), &statbuf) != 0) {
            fclose(fp);
            QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error opening File"), QString(QLatin1String("Error opening %1")).arg(QString::fromLatin1(tagFiles[i])));
            return;
        }

        // 2. Read the target file
        // Allocate space for the whole contents of the file (unfortunately)
        int fileLen = statbuf.st_size;
        auto fileString = std::make_unique<char[]>(fileLen + 1); // +1 = space for null

        // Read the file into fileString and terminate with a null
        int readLen = fread(&fileString[0], 1, fileLen, fp);
        if (ferror(fp)) {
            fclose(fp);
            QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error reading File"), QString(QLatin1String("Error reading %1")).arg(QString::fromLatin1(tagFiles[i])));
            return;
        }
        fileString[readLen] = '\0';

        // Close the file
        if (fclose(fp) != 0) {
            // unlikely error
            QMessageBox::critical(nullptr /*parent*/, QLatin1String("Error closing File"), QLatin1String("Unable to close file"));
            // we read it successfully, so continue
        }

        // 3. Search for the tagged location (set startPos)
        if (!*(tagSearch[i])) {
            // It's a line number, just go for it
            if ((moveAheadNLines(&fileString[0], &startPos, tagPosInf[i] - 1)) >= 0) {
                QMessageBox::critical(nullptr /*parent*/, QLatin1String("Tags Error"), QString(QLatin1String("%1\n not long enough for definition to be on line %2")).arg(QString::fromLatin1(tagFiles[i])).arg(tagPosInf[i]));
                return;
            }
        } else {
            startPos = tagPosInf[i];
            if (!fakeRegExSearchEx(view::string_view(&fileString[0], readLen), tagSearch[i], &startPos, &endPos)) {
                QMessageBox::critical(nullptr /*parent*/, QLatin1String("Tag not found"), QString(QLatin1String("Definition for %1\nnot found in %2")).arg(QString::fromLatin1(tagName), QString::fromLatin1(tagFiles[i])));
                return;
            }
        }

        if (searchMode == TIP) {
            int dummy, found;

            // 4. Find the end of the calltip (delimited by an empty line)
            endPos = startPos;
            found = SearchString(&fileString[0], QLatin1String("\\n\\s*\\n"), SEARCH_FORWARD, SEARCH_REGEX, false, startPos, &endPos, &dummy, nullptr, nullptr, nullptr);
            if (!found) {
                // Just take 4 lines
                moveAheadNLines(&fileString[0], &endPos, TIP_DEFAULT_LINES);
                --endPos; // Lose the last \n
            }
        } else { // Mode = TIP_FROM_TAG
            // 4. Copy TIP_DEFAULT_LINES lines of text to the calltip string
            endPos = startPos;
            moveAheadNLines(&fileString[0], &endPos, TIP_DEFAULT_LINES);
            // Make sure not to overrun the fileString with ". . ."
            if (((size_t)endPos) <= (strlen(&fileString[0]) - 5)) {
                sprintf(&fileString[endPos], ". . .");
                endPos += 5;
            }
        }

        // 5. Copy the calltip to a string
        tipLen = endPos - startPos;

        auto message = QString::fromLatin1(&fileString[startPos], tipLen);

        // 6. Display it
        tagsShowCalltipEx(area, message);
    } catch(const std::bad_alloc &) {
        QMessageBox::critical(nullptr /*parent*/, QLatin1String("Out of Memory"), QLatin1String("Can't allocate memory"));
    }
}

/*  Open a new (or existing) editor window to the location specified in
    tagFiles[i], tagSearch[i], tagPosInf[i] */
void editTaggedLocationEx(TextArea *area, int i) {

    int startPos;
    int endPos;
    int lineNum;
    int rows;
    QString filename;
    QString pathname;
    DocumentWidget *windowToSearch;
    auto document = DocumentWidget::documentFrom(area);

    ParseFilenameEx(QString::fromLatin1(tagFiles[i]), &filename, &pathname);
    // open the file containing the definition
    DocumentWidget::EditExistingFileEx(
                document,
                filename,
                pathname,
                0,
                QString(),
                false,
                QString(),
                GetPrefOpenInTab(),
                false);

    windowToSearch = MainWindow::FindWindowWithFile(filename, pathname);
    if(!windowToSearch) {
        QMessageBox::warning(nullptr /*parent*/, QLatin1String("File not found"), QString(QLatin1String("File %1 not found")).arg(QString::fromLatin1(tagFiles[i])));
        return;
    }

    startPos = tagPosInf[i];

    if (!*(tagSearch[i])) {
        // if the search string is empty, select the numbered line
        SelectNumberedLineEx(document, area, startPos);
        return;
    }

    // search for the tags file search string in the newly opened file
    if (!fakeRegExSearchEx(windowToSearch->buffer_->BufAsStringEx(), tagSearch[i], &startPos, &endPos)) {
        QMessageBox::warning(nullptr /*parent*/, QLatin1String("Tag Error"), QString(QLatin1String("Definition for %1\nnot found in %2")).arg(QString::fromLatin1(tagName), QString::fromLatin1(tagFiles[i])));
        return;
    }

    // select the matched string
    windowToSearch->buffer_->BufSelect(startPos, endPos);
    windowToSearch->RaiseFocusDocumentWindow(true);

    /* Position it nicely in the window,
       about 1/4 of the way down from the top */
    lineNum = windowToSearch->buffer_->BufCountLines(0, startPos);

    rows = area->getRows();

    area->TextDSetScroll(lineNum - rows / 4, 0);
    area->TextSetCursorPos(endPos);
}

//      Create a Menu for user to select from the collided tags 
static void createSelectMenuEx(DocumentWidget *document, TextArea *area, const QVector<char *> &args) {

    auto dialog = new DialogDuplicateTags(document, area, document);
    dialog->setTag(QString::fromLatin1(tagName));
    for(int i = 0; i < args.size(); ++i) {
        dialog->addListItem(QString::fromLatin1(args[i]), i);
    }
    dialog->show();
}

/********************************************************************
 *           Functions for loading Calltips files                   *
 ********************************************************************/

enum tftoken_types { TF_EOF, TF_BLOCK, TF_VERSION, TF_INCLUDE, TF_LANGUAGE, TF_ALIAS, TF_ERROR, TF_ERROR_EOF };

// A wrapper for SearchString 
static int searchLine(char *line, const char *regex) {
    int dummy1;
    int dummy2;
    return SearchString(line, QString::fromLatin1(regex), SEARCH_FORWARD, SEARCH_REGEX, false, 0, &dummy1, &dummy2, nullptr, nullptr, nullptr);
}

// Check if a line has non-ws characters 
static bool lineEmpty(const char *line) {
	while (*line && *line != '\n') {
		if (*line != ' ' && *line != '\t')
            return false;
		++line;
	}
    return true;
}

// Remove trailing whitespace from a line 
static void rstrip(char *dst, const char *src) {
	int wsStart, dummy2;
	// Strip trailing whitespace 
    if (SearchString(src, QLatin1String("\\s*\\n"), SEARCH_FORWARD, SEARCH_REGEX, false, 0, &wsStart, &dummy2, nullptr, nullptr, nullptr)) {
		if (dst != src)
			memcpy(dst, src, wsStart);
		dst[wsStart] = 0;
	} else if (dst != src)
		strcpy(dst, src);
}

/*
** Get the next block from a tips file.  A block is a \n\n+ delimited set of
** lines in a calltips file.  All of the parameters except <fp> are return
** values, and most have different roles depending on the type of block
** that is found.
**      header:     Depends on the block type
**      body:       Depends on the block type.  Used to return a new
**                  dynamically allocated string.
**      blkLine:    Returns the line number of the first line of the block
**                  after the "* xxxx *" line.
**      currLine:   Used to keep track of the current line in the file.
*/
static int nextTFBlock(FILE *fp, char *header, char **body, int *blkLine, int *currLine) {
	// These are the different kinds of tokens 
	const char *commenTF_regex = "^\\s*\\* comment \\*\\s*$";
	const char *version_regex = "^\\s*\\* version \\*\\s*$";
	const char *include_regex = "^\\s*\\* include \\*\\s*$";
	const char *language_regex = "^\\s*\\* language \\*\\s*$";
	const char *alias_regex = "^\\s*\\* alias \\*\\s*$";
	char line[MAXLINE], *status;
	int dummy1;
	int code;

	// Skip blank lines and comments 
	while (true) {
		// Skip blank lines 
		while ((status = fgets(line, MAXLINE, fp))) {
			++(*currLine);
			if (!lineEmpty(line))
				break;
		}

		// Check for error or EOF 
		if (!status)
			return TF_EOF;

		// We've got a non-blank line -- is it a comment block? 
		if (!searchLine(line, commenTF_regex))
			break;

		// Skip the comment (non-blank lines) 
		while ((status = fgets(line, MAXLINE, fp))) {
			++(*currLine);
			if (lineEmpty(line))
				break;
		}

		if (!status)
			return TF_EOF;
	}

	// Now we know it's a meaningful block 
	dummy1 = searchLine(line, include_regex);
	if (dummy1 || searchLine(line, alias_regex)) {
		// INCLUDE or ALIAS block 
		int incLen, incPos, i, incLines;

		// fprintf(stderr, "Starting include/alias at line %i\n", *currLine); 
		if (dummy1)
			code = TF_INCLUDE;
		else {
			code = TF_ALIAS;
			// Need to read the header line for an alias 
			status = fgets(line, MAXLINE, fp);
			++(*currLine);
			if (!status)
				return TF_ERROR_EOF;
			if (lineEmpty(line)) {
				fprintf(stderr, "nedit: Warning: empty '* alias *' "
				                "block in calltips file.\n");
				return TF_ERROR;
			}
			rstrip(header, line);
		}
		incPos = ftell(fp);
		*blkLine = *currLine + 1; // Line of first actual filename/alias 
		if (incPos < 0)
			return TF_ERROR;
		// Figure out how long the block is 
		while ((status = fgets(line, MAXLINE, fp)) || feof(fp)) {
			++(*currLine);
			if (feof(fp) || lineEmpty(line))
				break;
		}
		incLen = ftell(fp) - incPos;
		incLines = *currLine - *blkLine;
		// Correct currLine for the empty line it read at the end 
		--(*currLine);
		if (incLines == 0) {
			fprintf(stderr, "nedit: Warning: empty '* include *' or"
			                " '* alias *' block in calltips file.\n");
			return TF_ERROR;
		}
		// Make space for the filenames/alias sources 
		*body = new char[incLen + 1];
		*body[0] = '\0';
		if (fseek(fp, incPos, SEEK_SET) != 0) {
			delete [] *body;
			return TF_ERROR;
		}
		// Read all the lines in the block 
		// qDebug("Copying lines");
		for (i = 0; i < incLines; i++) {
			status = fgets(line, MAXLINE, fp);
			if (!status) {
				delete [] *body;
				return TF_ERROR_EOF;
			}
			rstrip(line, line);
			if (i) {
				strcat(*body, ":");
			}
			strcat(*body, line);
		}
		// qDebug("Finished include/alias at line %i", *currLine);
	}

	else if (searchLine(line, language_regex)) {
		// LANGUAGE block 
		status = fgets(line, MAXLINE, fp);
		++(*currLine);
		if (!status)
			return TF_ERROR_EOF;
		if (lineEmpty(line)) {
			qWarning("nedit: Warning: empty '* language *' block in calltips file.");
			return TF_ERROR;
		}
		*blkLine = *currLine;
		rstrip(header, line);
		code = TF_LANGUAGE;
	}

	else if (searchLine(line, version_regex)) {
		// VERSION block 
		status = fgets(line, MAXLINE, fp);
		++(*currLine);
		if (!status)
			return TF_ERROR_EOF;
		if (lineEmpty(line)) {
			qWarning("nedit: Warning: empty '* version *' block in calltips file.");
			return TF_ERROR;
		}
		*blkLine = *currLine;
		rstrip(header, line);
		code = TF_VERSION;
	}

	else {
		// Calltip block 
		/*  The first line is the key, the rest is the tip.
		    Strip trailing whitespace. */
		rstrip(header, line);

		status = fgets(line, MAXLINE, fp);
		++(*currLine);
		if (!status)
			return TF_ERROR_EOF;
		if (lineEmpty(line)) {
			fprintf(stderr, "nedit: Warning: empty calltip block:\n"
			                "   \"%s\"\n",
			        header);
			return TF_ERROR;
		}
		*blkLine = *currLine;
		*body = strdup(line);
		code = TF_BLOCK;
	}

	// Skip the rest of the block 
	dummy1 = *currLine;
	while (fgets(line, MAXLINE, fp)) {
		++(*currLine);
		if (lineEmpty(line))
			break;
	}

	// Warn about any unneeded extra lines (which are ignored). 
	if (dummy1 + 1 < *currLine && code != TF_BLOCK) {
		qWarning("nedit: Warning: extra lines in language or version block ignored.");
	}

	return code;
}

// A struct for describing a calltip alias 
struct tf_alias {    
	std::string dest;
	char *sources;
};

// Deallocate a linked-list of aliases 
static void free_alias_list(QList<tf_alias> *aliases) {

    for(tf_alias &alias : *aliases) {
        delete [] alias.sources;
	}
}

/*
** Load a calltips file and insert all of the entries into the global tips
** database.  Each tip is essentially stored as its filename and the line
** at which it appears--the exact same way ctags indexes source-code.  That's
** why calltips and tags share so much code.
*/
static int loadTipsFile(const QString &tipsFile, int index, int recLevel) {
	FILE *fp = nullptr;
	char header[MAXLINE];
	
	char *tipIncFile;
    int nTipsAdded = 0;
    int langMode = PLAIN_LANGUAGE_MODE;
    int oldLangMode;
	int currLine = 0;

    QList<tf_alias> aliases;

	if (recLevel > MAX_TAG_INCLUDE_RECURSION_LEVEL) {
		qWarning("nedit: Warning: Reached recursion limit before loading calltips file:\n\t%s", tipsFile.toLatin1().data());
		return 0;
	}

	// find the tips file 
    // Allow ~ in Unix filenames
    QString tipPath = ExpandTildeEx(tipsFile);
    if(tipPath.isNull()) {
        return 0;
    }

    QString resolvedTipsFile = ResolvePathEx(tipPath);
    if(resolvedTipsFile.isNull()) {
        return 0;
    }

    // Get the path to the tips file
    ParseFilenameEx(resolvedTipsFile, nullptr, &tipPath);

	// Open the file 
    if ((fp = fopen(resolvedTipsFile.toLatin1().data(), "r")) == nullptr) {
		return 0;
    }

    Q_FOREVER {
		int blkLine = 0;
		char *body = nullptr;
        int code = nextTFBlock(fp, header, &body, &blkLine, &currLine);

		if (code == TF_ERROR_EOF) {
			qWarning("nedit: Warning: unexpected EOF in calltips file.");
			break;
		}

		if (code == TF_EOF) {
			break;
		}

		switch (code) {
		case TF_BLOCK:
			/* Add the calltip to the global hash table.
			    For the moment I'm just using line numbers because I don't
			    want to have to deal with adding escape characters for
			    regex metacharacters that might appear in the string */
            nTipsAdded += addTag(header, resolvedTipsFile.toLatin1().data(), langMode, "", blkLine, tipPath.toLatin1().data(), index);
			delete [] body;
			break;
		case TF_INCLUDE:
			/* nextTFBlock returns a colon-separated list of tips files
			    in body */
			for (tipIncFile = strtok(body, ":"); tipIncFile; tipIncFile = strtok(nullptr, ":")) {
				/* qDebug("nedit: DEBUG: including tips file '%s'", tipIncFile); */
                nTipsAdded += loadTipsFile(QString::fromLatin1(tipIncFile), index, recLevel + 1);
			}
			delete [] body;
			break;
		case TF_LANGUAGE:
			/* Switch to the new language mode if it's valid, else ignore
			    it. */
			oldLangMode = langMode;
            langMode = FindLanguageMode(QString::fromLatin1(header));
			if (langMode == PLAIN_LANGUAGE_MODE && strcmp(header, "Plain")) {
				fprintf(stderr, "nedit: Error reading calltips file:\n\t%s\n"
				                "Unknown language mode: \"%s\"\n",
                        tipsFile.toLatin1().data(), header);
				langMode = oldLangMode;
			}
			break;
		case TF_ERROR:
			fprintf(stderr, "nedit: Warning: Recoverable error while "
			                "reading calltips file:\n   \"%s\"\n",
			        resolvedTipsFile.toLatin1().data());
			break;
		case TF_ALIAS:
			// Allocate a new alias struct 
            aliases.push_front(tf_alias{ header, body });
			break;
		default:
			; // Ignore TF_VERSION for now 
		}
	}

	// NOTE(eteran): fix resource leak
	fclose(fp);

    // Now resolve any aliases
    for(const tf_alias &tmp_alias : aliases) {
        QList<Tag> tags = getTag(tmp_alias.dest.c_str(), TIP);
        if (tags.isEmpty()) {
			fprintf(stderr, "nedit: Can't find destination of alias \"%s\"\n"
			                "  in calltips file:\n   \"%s\"\n",
                    tmp_alias.dest.c_str(), resolvedTipsFile.toLatin1().data());
		} else {

            const Tag *t = &tags[0];
            for (char *src = strtok(tmp_alias.sources, ":"); src; src = strtok(nullptr, ":")) {
                addTag(src, resolvedTipsFile.toLatin1().data(), t->language, "", t->posInf, tipPath.toLatin1().data(), index);
            }
		}
	}

    free_alias_list(&aliases);
	return nTipsAdded;
}
