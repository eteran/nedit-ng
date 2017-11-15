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
#include "preferences.h"
#include "search.h"
#include "selection.h"
#include "util/fileUtils.h"
#include "util/utils.h"
#include "gsl/gsl_util"

#include <QApplication>
#include <QMessageBox>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>

#include <iostream>
#include <fstream>
#include <cctype>
#include <cstdio>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <sstream>

#include <sys/param.h>

namespace {

constexpr int MAXLINE						  = 2048;
constexpr int MAXDUPTAGS					  = 100;
constexpr int MAX_TAG_INCLUDE_RECURSION_LEVEL = 5;

/* Take this many lines when making a tip from a tag.
   (should probably be a language-dependent option, but...) */
constexpr int TIP_DEFAULT_LINES = 4;

}

struct Tag;

/*
**  contents of                   tag->searchString   | tag->posInf
**    ctags, line num specified:  ""                  | line num
**    ctags, search expr specfd:  ctags search expr   | -1
**    etags  (emacs tags)         etags search string | search start pos
*/

static int loadTagsFile(const QString &tagSpec, int index, int recLevel);
static int fakeRegExSearchEx(view::string_view buffer, const char *searchString, int *startPos, int *endPos);
static int addTag(const QString &name, const QString &file, int lang, const QString &search, int posInf, const QString &path, int index);
static bool delTag(int index);
static QList<Tag> getTag(const QString &name, TagSearchMode search_type);
static void createSelectMenuEx(DocumentWidget *document, TextArea *area, const QStringList &args);
static QList<Tag> LookupTag(const QString &name, TagSearchMode search_type);

static bool searchLine(const std::string &line, const std::string &regex);
static QString rstrip(QString s);
static int nextTFBlock(std::istream &is, QString &header, QString &body, int *blkLine, int *currLine);
static int loadTipsFile(const QString &tipsFile, int index, int recLevel);
static QMultiHash<QString, Tag> *hashTableByType(TagSearchMode type);
static QList<TagFile> *tagListByType(TagSearchMode type);

struct Tag {
    QString name;
    QString file;
    QString searchString;
    QString path;
    int language;
	int posInf;
    int index;
};

static QMultiHash<QString, Tag> Tags;

// list of loaded tags files 
QList<TagFile> TagsFileList;

// Hash table of calltip tags 
static QMultiHash<QString, Tag> Tips;
QList<TagFile> TipsFileList;

/* These are all transient global variables -- they don't hold any state
    between tag/tip lookups */
TagSearchMode searchMode = TagSearchMode::TAG;
QString tagName;

static QString tagFiles[MAXDUPTAGS];
static QString tagSearch[MAXDUPTAGS];
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

static QMultiHash<QString, Tag> *hashTableByType(TagSearchMode type) {
    if (type == TagSearchMode::TIP) {
        return &Tips;
    } else {
        return &Tags;
    }
}

static QList<TagFile> *tagListByType(TagSearchMode type) {
    if (type == TagSearchMode::TAG) {
        return &TagsFileList;
    } else {
        return &TipsFileList;
    }
}

static QList<Tag> getTagFromTable(QMultiHash<QString, Tag> *table, const QString &name) {
    return table->values(name);
}

//      Retrieve a Tag structure from the hash table 
static QList<Tag> getTag(const QString &name, TagSearchMode search_type) {

    if (search_type == TagSearchMode::TIP) {
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
static int addTag(const QString &name, const QString &file, int lang, const QString &search, int posInf, const QString &path, int index) {

    QMultiHash<QString, Tag> *table;

    if (searchMode == TagSearchMode::TIP) {
        table = &Tips;
    } else {
        table = &Tags;
	}

    QString newFile;
    if (QFileInfo(file).isAbsolute()) {
        newFile = file;
	} else {
        newFile = QString(QLatin1String("%1%2")).arg(path, file);
	}

    newFile = NormalizePathnameEx(newFile);

    QList<Tag> tags = table->values(name);
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

        if (QFileInfo(t.file).isAbsolute() && (newFile != t.file)) {
			continue;
        }

        if (QFileInfo(t.file).isAbsolute()) {

            auto tmpFile = QString(QLatin1String("%1%2")).arg(t.path, t.file);

            tmpFile = NormalizePathnameEx(tmpFile);

            if(newFile != tmpFile) {
                continue;
            }
		}
        return 0;
    }

    Tag t = { name, file, search, path, lang, posInf, index };

    table->insert(name, t);
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
static int16_t tagFileIndex = 0;

/*
** AddRelTagsFile():  Rescan tagSpec for relative tag file specs
** (not starting with [/~]) and extend the tag files list if in
** windowPath a tags file matching the relative spec has been found.
*/
bool AddRelTagsFileEx(const QString &tagSpec, const QString &windowPath, TagSearchMode file_type) {

    bool added = false;


    searchMode = file_type;
    QList<TagFile> *FileList = tagListByType(searchMode);

    if(tagSpec.isEmpty()) {
        return false;
    }

    QStringList filenames = tagSpec.split(QLatin1Char(':'));

    for(const QString &filename : filenames) {
        if(QFileInfo(filename).isAbsolute() || filename.startsWith(QLatin1Char('~'))) {
            continue;
        }

        QString pathName;
        if(!windowPath.isEmpty()) {
            pathName = QString(QLatin1String("%1/%2")).arg(windowPath, filename);
        } else {
            pathName = QString(QLatin1String("%1/%2")).arg(GetCurrentDirEx(), filename);
        }

        pathName = NormalizePathnameEx(pathName);

        auto it = std::find_if(FileList->begin(), FileList->end(), [pathName](const TagFile &tag) {
            return tag.filename == pathName;
        });

        // if we found an entry with the same pathname, we're done..
        if (it != FileList->end()) {
            added = true;
            continue;
        }

        // or if the file isn't found...
        QFileInfo fileInfo(pathName);
        QDateTime timestamp = fileInfo.lastModified();
        if (timestamp.isNull()) {
            continue;
        }

        TagFile tag = {
            pathName,
            timestamp,
            false,
            ++tagFileIndex,
            1 // NOTE(eteran): added just so there aren't any uninitialized members
        };

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
bool AddTagsFileEx(const QString &tagSpec, TagSearchMode file_type) {

    bool added = true;

    searchMode = file_type;
    QList<TagFile> *FileList = tagListByType(searchMode);

    if(tagSpec.isEmpty()) {
        return false;
    }

    QStringList filenames = tagSpec.split(QLatin1Char(':'));

    for(const QString &filename : filenames) {

		QString pathName;
        if(!QFileInfo(filename).isAbsolute()) {
			pathName = QString(QLatin1String("%1/%2")).arg(GetCurrentDirEx(), filename);
        } else {
			pathName = filename;
        }

		pathName = NormalizePathnameEx(pathName);

        auto it = std::find_if(FileList->begin(), FileList->end(), [pathName](const TagFile &tag) {
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

        QFileInfo fileInfo(pathName);
        QDateTime timestamp = fileInfo.lastModified();

        if (timestamp.isNull()) {
            // Problem reading this tags file.  Return FALSE
            added = false;
            continue;
        }

        TagFile tag = {
            pathName,
            timestamp,
            false,
            ++tagFileIndex,
            1
        };

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
int DeleteTagsFileEx(const QString &tagSpec, TagSearchMode file_type, bool force_unload) {

    if(tagSpec.isEmpty()) {
        return false;
    }

    searchMode = file_type;
    QList<TagFile> *FileList = tagListByType(searchMode);

    bool removed = true;

    QStringList filenames = tagSpec.split(QLatin1Char(':'));

    for(const QString &filename : filenames) {

        QString pathName;
        if(!QFileInfo(filename).isAbsolute()) {
            pathName = QString(QLatin1String("%1/%2")).arg(GetCurrentDirEx(), filename);
        } else {
            pathName = filename;
        }

        pathName = NormalizePathnameEx(pathName);

        auto it = FileList->begin();
        while(it != FileList->end()) {
            TagFile &t = *it;

            if (t.filename != pathName) {
                ++it;
                continue;
            }

            // Don't unload tips files with nonzero refcounts unless forced
            if (searchMode == TagSearchMode::TIP && !force_unload && --t.refcount > 0) {
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
void updateMenuItems() {
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
static int scanCTagsLine(const QString &line, const QString &tagPath, int index) {

    QRegExp regex(QLatin1String("^([^\\t]+)\\t([^\\t]+)\\t([^\\n]+)\\n$"));
    if(!regex.exactMatch(line)) {
		return 0;
	}

	if(regex.captureCount() != 3) {
		return 0;
	}

    QString name         = regex.cap(1);
    QString file         = regex.cap(2);
    QString searchString = regex.cap(3);

	if (name.startsWith(QLatin1Char('!'))) {
		return 0;
	}

	int pos;

	/*
	** Guess the end of searchString:
	** Try to handle original ctags and exuberant ctags format:
	*/
    if (searchString.startsWith(QLatin1Char('/')) || searchString.startsWith(QLatin1Char('?'))) {

		pos = -1; // "search expr without pos info"

		/* Situations: /<ANY expr>/\0
		**             ?<ANY expr>?\0          --> original ctags
		**             /<ANY expr>/;"  <flags>
		**             ?<ANY expr>?;"  <flags> --> exuberant ctags
		*/

		int posTagREEnd = searchString.lastIndexOf(QLatin1Char(';'));

		if(posTagREEnd == -1 ||
		   searchString.mid(posTagREEnd, 2) != QLatin1String(";\"") ||
		   searchString.startsWith(searchString.right(1))) {
			//  -> original ctags format = exuberant ctags format 1
		} else {
			// looks like exuberant ctags format 2
			searchString = searchString.left(posTagREEnd);
		}

		/*
		** Hide the last delimiter:
		**   /<expression>/    becomes   /<expression>
		**   ?<expression>?    becomes   ?<expression>
		** This will save a little work in fakeRegExSearch.
		*/
		if(searchString.startsWith(searchString.right(1))) {
			searchString.chop(1);
		}
	} else {
		pos = searchString.toInt();
		searchString.clear();
	}
	// No ability to read language mode right now 
	return addTag(
                name,
                file,
	            PLAIN_LANGUAGE_MODE,
                searchString,
	            pos,
                tagPath,
	            index);
}

/*
 * Scans one <line> from an etags (emacs) tags file (<index>) in tagPath.
 * recLevel = current recursion level for tags file including
 * file = destination definition file. possibly modified. len=MAXPATHLEN!
 * Return value: Number of tag specs added.
 */
static int scanETagsLine(const QString &line, const QString &tagPath, int index, QString &file, int recLevel) {

	// check for destination file separator  
    if (line.startsWith(QLatin1Char('\014'))) { // <np>
        file = QString();
		return 0;
	}

    // check for standard definition line
    const int posDEL = line.lastIndexOf(QLatin1Char('\177'));
    const int posSOH = line.lastIndexOf(QLatin1Char('\001'));
    const int posCOM = line.lastIndexOf(QLatin1Char(','));

    if (!file.isEmpty() && posDEL != -1 && (posSOH > posDEL) && (posCOM > posSOH)) {
		// exuberant ctags -e style  
        QString searchString = line.mid(0, posDEL);
        QString name         = line.mid(posDEL + 1, (posSOH - posDEL) - 1);
        int pos              = line.midRef(posCOM + 1).toInt();

        // No ability to set language mode for the moment
        return addTag(name, file, PLAIN_LANGUAGE_MODE, searchString, pos, tagPath, index);
	}

    if (!file.isEmpty() && posDEL != -1 && (posCOM > posDEL)) {
        // old etags style, part  name<soh>  is missing here!
        QString searchString = line.mid(0, posDEL);

		// guess name: take the last alnum (plus _) part of searchString 
        int len = posDEL;
		while (--len >= 0) {
            if (searchString[len].isLetterOrNumber() || (searchString[len] == QLatin1Char('_'))) {
                break;
            }
		}

        if (len < 0) {
            return 0;
        }

        int pos = len;

        while (pos >= 0 && (searchString[pos].isLetterOrNumber() || (searchString[pos] == QLatin1Char('_')))) {
			pos--;
        }

        QString name = searchString.mid(pos + 1, len - pos);
        pos = line.midRef(posCOM + 1).toInt();

        return addTag(name, file, PLAIN_LANGUAGE_MODE, searchString, pos, tagPath, index);
	}

	// check for destination file spec 
    if (!line.isEmpty() && posCOM != -1) {

        file = line.mid(0, posCOM);

        // check if that's an include file ...
        if(line.midRef(posCOM + 1, 7) == QLatin1String("include")) {

            if (!QFileInfo(file).isAbsolute()) {

                if ((tagPath.size() + file.size()) >= MAXPATHLEN) {
                    qWarning("NEdit: tags.c: MAXPATHLEN overflow");
                    file = QString(); // invalidate
					return 0;
                }

                auto incPath = QString(QLatin1String("%1%2")).arg(tagPath, file);
                incPath = CompressPathnameEx(incPath);
                return loadTagsFile(incPath, index, recLevel + 1);
			} else {
                return loadTagsFile(file, index, recLevel + 1);
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
static int loadTagsFile(const QString &tagSpec, int index, int recLevel) {

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
    QString resolvedTagsFile = ResolvePathEx(tagSpec);
    if (resolvedTagsFile.isNull()) {
		return 0;
	}

	QFile f(resolvedTagsFile);
	if(!f.open(QIODevice::ReadOnly)) {
		return 0;
	}

	ParseFilenameEx(resolvedTagsFile, nullptr, &tagPath);

	/* This might take a while if you have a huge tags file (like I do)..
	   keep the windows up to date and post a busy cursor so the user
	   doesn't think we died. */
	MainWindow::AllWindowsBusyEx(QLatin1String("Loading tags file..."));

    QString filename;

    QTextStream stream(&f);

    while (!stream.atEnd()) {
        QString line = stream.readLine();

		/* the first character in the file decides if the file is treat as
		   etags or ctags file.
		 */
		if (tagFileType == TFT_CHECK) {
			if (line.startsWith(0x0c)) { // <np>
				tagFileType = TFT_ETAGS;
			} else {
				tagFileType = TFT_CTAGS;
			}
		}

		if (tagFileType == TFT_CTAGS) {
            nTagsAdded += scanCTagsLine(line, tagPath, index);
        } else {            
            nTagsAdded += scanETagsLine(line, tagPath, index, filename, recLevel);
		}
	}

	MainWindow::AllWindowsUnbusyEx();
	return nTagsAdded;
}

static QList<Tag> LookupTagFromList(QList<TagFile> *FileList, const QString &name, TagSearchMode search_type) {

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
    if (!name.isNull()) {
        for(TagFile &tf : *FileList) {

            int load_status;
		
            if (tf.loaded) {

                QFileInfo fileInfo(tf.filename);
                QDateTime timestamp = fileInfo.lastModified();

                if (timestamp.isNull()) {
                    qWarning("NEdit: Error getting status for tag file %s", qPrintable(tf.filename));
                } else {
                    if (tf.date == timestamp) {
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

                QFileInfo fileInfo(tf.filename);
                QDateTime timestamp = fileInfo.lastModified();

                if (timestamp.isNull()) {
                    if (!tf.loaded) {
                        // if tf->loaded == true we already have seen the error msg
                        qWarning("NEdit: Error getting status for tag file %s", qPrintable(tf.filename));
                    }
                } else {
                    tf.date = timestamp;
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
static QList<Tag> LookupTag(const QString &name, TagSearchMode search_type) {

	searchMode = search_type;
    if (searchMode == TagSearchMode::TIP) {
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
int ShowTipStringEx(DocumentWidget *document, const QString &text, bool anchored, int pos, bool lookup, TagSearchMode search_type, int hAlign, int vAlign, int alignMode) {
    if (search_type == TagSearchMode::TAG) {
        return 0;
    }

    // So we don't have to carry all of the calltip alignment info around
    globAnchored  = anchored;
    globPos       = pos;
    globHAlign    = hAlign;
    globVAlign    = vAlign;
    globAlignMode = alignMode;

    // If this isn't a lookup request, just display it.
    if (!lookup) {
        return tagsShowCalltipEx(MainWindow::fromDocument(document)->lastFocus_, text);
    } else {
        return document->findDef(MainWindow::fromDocument(document)->lastFocus_, text, search_type);
    }
}

/*
** ctags search expressions are literal strings with a search direction flag,
** line starting "^" and ending "$" delimiters. This routine translates them
** into NEdit compatible regular expressions and does the search.
** Etags search expressions are plain literals strings, which
*/
static int fakeRegExSearchEx(view::string_view buffer, const char *searchString, int *startPos, int *endPos) {

    int searchStartPos;
    Direction dir;
    bool ctagsMode;
	char searchSubs[3 * MAXLINE + 3];
	const char *inPtr;

    view::string_view fileString = buffer;

	// determine search direction and start position 
	if (*startPos != -1) { // etags mode! 
        dir = Direction::FORWARD;
		searchStartPos = *startPos;
        ctagsMode = false;
    } else if (searchString[0] == '/') {
        dir = Direction::FORWARD;
		searchStartPos = 0;
        ctagsMode = true;
    } else if (searchString[0] == '?') {
        dir = Direction::BACKWARD;
		// searchStartPos = window->buffer_->length; 
        searchStartPos = gsl::narrow<int>(fileString.size());
        ctagsMode = true;
	} else {
		qWarning("NEdit: Error parsing tag file search string");
        return false;
	}

	// Build the search regex. 
    char *outPtr = searchSubs;
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
        } else if (safe_ctype<isspace>(*inPtr)) { // col. multiple spaces
			*outPtr++ = '\\';
			*outPtr++ = 's';
			*outPtr++ = '+';
			do {
				inPtr++;
            } while (safe_ctype<isspace>(*inPtr));
		} else { // simply copy all other characters 
			*outPtr++ = *inPtr++;
		}
	}

    *outPtr = '\0'; // Terminate searchSubs

    bool found = SearchString(fileString, QString::fromLatin1(searchSubs), dir, SearchType::Regex, WrapMode::NoWrap, searchStartPos, startPos, endPos, nullptr, nullptr, QString());

	if (!found && !ctagsMode) {
		/* position of the target definition could have been drifted before
		   startPos, if nothing has been found by now try searching backward
		   again from startPos.
		*/
        found = SearchString(fileString, QString::fromLatin1(searchSubs), Direction::BACKWARD, SearchType::Regex, WrapMode::NoWrap, searchStartPos, startPos, endPos, nullptr, nullptr, QString());
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
int findAllMatchesEx(DocumentWidget *document, TextArea *area, const QString &string) {

    QString filename;
    QString pathname;

    int i;
    int pathMatch = 0;
    int samePath = 0;    
    int nMatches = 0;

    // verify that the string is reasonable as a tag
    if(string.isEmpty()) {
        QApplication::beep();
        return -1;
    }

    tagName = string;

    QList<Tag> tags = LookupTag(string, searchMode);

    // First look up all of the matching tags
    for(const Tag &tag : tags) {

        QString fileToSearch = tag.file;
        QString searchString = tag.searchString;
        QString tagPath      = tag.path;
        int langMode         = tag.language;
        int startPos         = tag.posInf;

        /*
        ** Skip this tag if it has a language mode that doesn't match the
        ** current language mode, but don't skip anything if the window is in
        ** PLAIN_LANGUAGE_MODE.
        */
        if (document->languageMode_ != PLAIN_LANGUAGE_MODE && GetPrefSmartTags() && langMode != PLAIN_LANGUAGE_MODE && langMode != document->languageMode_) {
            continue;
        }

        if (QFileInfo(fileToSearch).isAbsolute()) {
            tagFiles[nMatches] = fileToSearch;
        } else {
            tagFiles[nMatches] = QString(QLatin1String("%1%2")).arg(tagPath, fileToSearch);
        }

        tagSearch[nMatches] = searchString;
        tagPosInf[nMatches] = startPos;

        ParseFilenameEx(tagFiles[nMatches], &filename, &pathname);

        // Is this match in the current file?  If so, use it!
        if (GetPrefSmartTags() && document->filename_ == filename && document->path_ == pathname) {
            if (nMatches) {
                tagFiles[0]  = tagFiles[nMatches];
                tagSearch[0] = tagSearch[nMatches];
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
        tagFiles[0] = tagFiles[pathMatch];
        tagSearch[0] = tagSearch[pathMatch];
        tagPosInf[0] = tagPosInf[pathMatch];
        nMatches = 1;
    }

    //  If all of the tag entries are the same file, just use the first.
    if (GetPrefSmartTags()) {
        for (i = 1; i < nMatches; i++) {
            if(tagFiles[i] != tagFiles[i - 1]) {
                break;
            }
        }

        if (i == nMatches) {
            nMatches = 1;
        }
    }

    if (nMatches > 1) {
		QStringList dupTagsList;

        for (i = 0; i < nMatches; i++) {

            QString temp;

            ParseFilenameEx(tagFiles[i], &filename, &pathname);
            if ((i < nMatches - 1 && (tagFiles[i] == tagFiles[i + 1])) || (i > 0 && (tagFiles[i] == tagFiles[i - 1]))) {

                if (!tagSearch[i].isEmpty() && (tagPosInf[i] != -1)) {
                    // etags
                    temp = QString::asprintf("%2d. %s%s %8i %s", i + 1, qPrintable(pathname), qPrintable(filename), tagPosInf[i], qPrintable(tagSearch[i]));
                } else if (!tagSearch[i].isEmpty()) {
                    // ctags search expr
                    temp = QString::asprintf("%2d. %s%s          %s", i + 1, qPrintable(pathname), qPrintable(filename), qPrintable(tagSearch[i]));
                } else {
                    // line number only
                    temp = QString::asprintf("%2d. %s%s %8i", i + 1, qPrintable(pathname), qPrintable(filename), tagPosInf[i]);
                }
            } else {
                temp = QString::asprintf("%2d. %s%s", i + 1, qPrintable(pathname), qPrintable(filename));
            }

            dupTagsList.push_back(temp);
        }

        createSelectMenuEx(document, area, dupTagsList);
        return 1;
    }

    /*
    **  No need for a dialog list, there is only one tag matching --
    **  Go directly to the tag
    */
    if (searchMode == TagSearchMode::TAG) {
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
static int moveAheadNLinesEx(view::string_view str, int *pos, int n) {

	int i = n;
	while (str.begin() + *pos != str.end() && n > 0) {
		if (str[*pos] == '\n') {
			--n;
		}
		++(*pos);
	}

	if (n == 0) {
		return -1;
	} else {
		return i - n;
	}
}

/*
** Show the calltip specified by tagFiles[i], tagSearch[i], tagPosInf[i]
** This reads from either a source code file (if searchMode == TIP_FROM_TAG)
** or a calltips file (if searchMode == TIP).
*/
void showMatchingCalltipEx(TextArea *area, int i) {
    try {
        int startPos = 0;
        int endPos = 0;

        // 1. Open the target file
        NormalizePathnameEx(tagFiles[i]);

        std::ifstream file(tagFiles[i].toStdString());
		if(!file) {
            QMessageBox::critical(
                        nullptr /*parent*/,
                        QLatin1String("Error opening File"),
                        QString(QLatin1String("Error opening %1")).arg(tagFiles[i]));
			return;
		}

		// 2. Read the target file
		std::string fileString(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});

		// 3. Search for the tagged location (set startPos)
        if (tagSearch[i].isEmpty()) {
            // It's a line number, just go for it
			if ((moveAheadNLinesEx(fileString, &startPos, tagPosInf[i] - 1)) >= 0) {
                QMessageBox::critical(
                            nullptr /*parent*/,
                            QLatin1String("Tags Error"),
                            QString(QLatin1String("%1\n not long enough for definition to be on line %2")).arg(tagFiles[i]).arg(tagPosInf[i]));
                return;
            }
        } else {
            startPos = tagPosInf[i];
            if (!fakeRegExSearchEx(fileString, tagSearch[i].toLatin1().data(), &startPos, &endPos)) {
                QMessageBox::critical(
                            nullptr /*parent*/,
                            QLatin1String("Tag not found"),
                            QString(QLatin1String("Definition for %1\nnot found in %2")).arg(tagName, tagFiles[i]));
                return;
            }
        }

        if (searchMode == TagSearchMode::TIP) {
			int dummy;

            // 4. Find the end of the calltip (delimited by an empty line)
            endPos = startPos;
            bool found = SearchString(fileString.c_str(), QLatin1String("\\n\\s*\\n"), Direction::FORWARD, SearchType::Regex, WrapMode::NoWrap, startPos, &endPos, &dummy, nullptr, nullptr, QString());
            if (!found) {
                // Just take 4 lines
				moveAheadNLinesEx(fileString, &endPos, TIP_DEFAULT_LINES);
                --endPos; // Lose the last \n
            }

        } else { // Mode = TIP_FROM_TAG
            // 4. Copy TIP_DEFAULT_LINES lines of text to the calltip string
            endPos = startPos;
			moveAheadNLinesEx(fileString, &endPos, TIP_DEFAULT_LINES);

            // Make sure not to overrun the fileString with ". . ."
			if (static_cast<size_t>(endPos) <= (fileString.size() - 5)) {
				fileString.replace(endPos, 5, ". . .");
                endPos += 5;
            }
        }

        // 5. Copy the calltip to a string
		int tipLen = endPos - startPos;
        auto message = QString::fromLatin1(&fileString[startPos], tipLen);

        // 6. Display it
        tagsShowCalltipEx(area, message);
    } catch(const std::bad_alloc &) {
        QMessageBox::critical(
                    nullptr /*parent*/,
                    QLatin1String("Out of Memory"),
                    QLatin1String("Can't allocate memory"));
    }
}

/*  Open a new (or existing) editor window to the location specified in
    tagFiles[i], tagSearch[i], tagPosInf[i] */
void editTaggedLocationEx(TextArea *area, int i) {

    int endPos;
    int rows;
    QString filename;
    QString pathname;

    auto document = DocumentWidget::fromArea(area);

    ParseFilenameEx(tagFiles[i], &filename, &pathname);

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

	DocumentWidget *windowToSearch = MainWindow::FindWindowWithFile(filename, pathname);
    if(!windowToSearch) {
        QMessageBox::warning(
                    document,
                    QLatin1String("File not found"),
                    QString(QLatin1String("File %1 not found")).arg(tagFiles[i]));
        return;
    }

	int startPos = tagPosInf[i];

    if (tagSearch[i].isEmpty()) {
        // if the search string is empty, select the numbered line
        SelectNumberedLineEx(document, area, startPos);
        return;
    }

    // search for the tags file search string in the newly opened file
    if (!fakeRegExSearchEx(windowToSearch->buffer_->BufAsStringEx(), tagSearch[i].toLatin1().data(), &startPos, &endPos)) {
        QMessageBox::warning(
                    document,
                    QLatin1String("Tag Error"),
                    QString(QLatin1String("Definition for %1\nnot found in %2")).arg(tagName, tagFiles[i]));
        return;
    }

    // select the matched string
    windowToSearch->buffer_->BufSelect(startPos, endPos);
    windowToSearch->RaiseFocusDocumentWindow(true);

    /* Position it nicely in the window,
       about 1/4 of the way down from the top */
	int lineNum = windowToSearch->buffer_->BufCountLines(0, startPos);

    rows = area->getRows();

    area->TextDSetScroll(lineNum - rows / 4, 0);
    area->TextSetCursorPos(endPos);
}

//      Create a Menu for user to select from the collided tags 
static void createSelectMenuEx(DocumentWidget *document, TextArea *area, const QStringList &args) {

    auto dialog = new DialogDuplicateTags(document, area, document);
	dialog->setTag(tagName);
    for(int i = 0; i < args.size(); ++i) {
		dialog->addListItem(args[i], i);
    }
    dialog->show();
}

/********************************************************************
 *           Functions for loading Calltips files                   *
 ********************************************************************/

enum tftoken_types {
    TF_EOF,
    TF_BLOCK,
    TF_VERSION,
    TF_INCLUDE,
    TF_LANGUAGE,
    TF_ALIAS,
    TF_ERROR,
    TF_ERROR_EOF
};

// A wrapper for SearchString
static bool searchLine(const std::string &line, const std::string &regex) {
    int dummy1;
    int dummy2;
    return SearchString(
                line.c_str(),
                QString::fromStdString(regex),
                Direction::FORWARD,
                SearchType::Regex,
                WrapMode::NoWrap,
                0,
                &dummy1,
                &dummy2,
                nullptr,
                nullptr,
                QString());
}

// Check if a line has non-ws characters 
static bool lineEmpty(const view::string_view line) {

    for(char ch : line) {
        if(ch == '\n') {
            break;
        }

        if (ch != ' ' && ch != '\t') {
            return false;
        }
    }
    return true;
}

static QString rstrip(QString s) {
    return s.replace(QRegularExpression(QLatin1String("\\s*\\n")), QString());
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
static int nextTFBlock(std::istream &is, QString &header, QString &body, int *blkLine, int *currLine) {

    // These are the different kinds of tokens
    const char *commenTF_regex = "^\\s*\\* comment \\*\\s*$";
    const char *version_regex  = "^\\s*\\* version \\*\\s*$";
    const char *include_regex  = "^\\s*\\* include \\*\\s*$";
    const char *language_regex = "^\\s*\\* language \\*\\s*$";
    const char *alias_regex    = "^\\s*\\* alias \\*\\s*$";
    std::string line;
    int dummy1;
    int code;

	// Skip blank lines and comments 
    Q_FOREVER {

        // Skip blank lines
        while(std::getline(is, line)) {
            ++(*currLine);
			if (!lineEmpty(line))
				break;
		}

		// Check for error or EOF 
        if (!is)
            return TF_EOF;

		// We've got a non-blank line -- is it a comment block? 
        if (!searchLine(line, commenTF_regex))
            break;

        // Skip the comment (non-blank lines)
        while (std::getline(is, line)) {
            ++(*currLine);
            if (lineEmpty(line))
                break;
        }

        if (!is) {
            return TF_EOF;
        }
	}

	// Now we know it's a meaningful block 
    dummy1 = searchLine(line, include_regex);
    if (dummy1 || searchLine(line, alias_regex)) {
		// INCLUDE or ALIAS block 

        long incPos;
        int i;
        int incLines;

        // qDebug("NEdit: Starting include/alias at line %i", *currLine);
        if (dummy1) {
			code = TF_INCLUDE;
        } else {
			code = TF_ALIAS;
            // Need to read the header line for an alias

            std::getline(is, line);
            ++(*currLine);
            if (!is)
                return TF_ERROR_EOF;
            if (lineEmpty(line)) {
                qWarning("NEdit: Warning: empty '* alias *' block in calltips file.");
				return TF_ERROR;
            }
            header = rstrip(QString::fromStdString(line));
		}

        incPos = is.tellg();
        *blkLine = *currLine + 1; // Line of first actual filename/alias
        if (incPos < 0)
            return TF_ERROR;

        // Figure out how long the block is
        while (std::getline(is, line) || is.eof()) {
            ++(*currLine);
            if (is.eof() || lineEmpty(line))
                break;
		}

        incLines = *currLine - *blkLine;
		// Correct currLine for the empty line it read at the end 
        --(*currLine);
		if (incLines == 0) {
            qWarning("NEdit: Warning: empty '* include *' or '* alias *' block in calltips file.");
			return TF_ERROR;
        }

        // Make space for the filenames/alias sources
        if (is.seekg(incPos, std::ios::beg)) {
			return TF_ERROR;
        }

		// Read all the lines in the block 
		// qDebug("Copying lines");
        for (i = 0; i < incLines; i++) {
            if(!std::getline(is, line)) {
				return TF_ERROR_EOF;
            }

            QString currentLine = rstrip(QString::fromStdString((line)));

            if (i != 0) {
                body.push_back(QLatin1Char(':'));
			}

            body.append((currentLine));
		}
		// qDebug("Finished include/alias at line %i", *currLine);
    } else if (searchLine(line, language_regex)) {
        // LANGUAGE block
        std::getline(is, line);
        ++(*currLine);

        if (!is) {
            return TF_ERROR_EOF;
        }

		if (lineEmpty(line)) {
            qWarning("NEdit: Warning: empty '* language *' block in calltips file.");
			return TF_ERROR;
        }
        *blkLine = *currLine;
        header = rstrip(QString::fromStdString(line));
		code = TF_LANGUAGE;
    } else if (searchLine(line, version_regex)) {
		// VERSION block 
        std::getline(is, line);
        ++(*currLine);
        if (!is)
			return TF_ERROR_EOF;
		if (lineEmpty(line)) {
            qWarning("NEdit: Warning: empty '* version *' block in calltips file.");
			return TF_ERROR;
		}
        *blkLine = *currLine;
        header = rstrip(QString::fromStdString(line));
		code = TF_VERSION;
    } else {
		// Calltip block 
		/*  The first line is the key, the rest is the tip.
		    Strip trailing whitespace. */
        header = rstrip(QString::fromStdString(line));

        std::getline(is, line);
        ++(*currLine);
        if (!is)
            return TF_ERROR_EOF;
		if (lineEmpty(line)) {
            qWarning("NEdit: Warning: empty calltip block:\n   \"%s\"", qPrintable(header));
			return TF_ERROR;
		}
        *blkLine = *currLine;
        body = QString::fromStdString(line);
		code = TF_BLOCK;
	}

	// Skip the rest of the block 
    dummy1 = *currLine;
    while (std::getline(is, line)) {
        ++(*currLine);
		if (lineEmpty(line))
			break;
	}

	// Warn about any unneeded extra lines (which are ignored). 
    if (dummy1 + 1 < *currLine && code != TF_BLOCK) {
        qWarning("NEdit: Warning: extra lines in language or version block ignored.");
	}

	return code;
}

// A struct for describing a calltip alias 
struct tf_alias {    
    QString dest;
    QString sources;
};

/*
** Load a calltips file and insert all of the entries into the global tips
** database.  Each tip is essentially stored as its filename and the line
** at which it appears--the exact same way ctags indexes source-code.  That's
** why calltips and tags share so much code.
*/
static int loadTipsFile(const QString &tipsFile, int index, int recLevel) {

    QString header;

    int nTipsAdded = 0;
    int langMode = PLAIN_LANGUAGE_MODE;
    int oldLangMode;
    int currLine = 0;

    QList<tf_alias> aliases;

	if (recLevel > MAX_TAG_INCLUDE_RECURSION_LEVEL) {
        qWarning("NEdit: Warning: Reached recursion limit before loading calltips file:\n\t%s",
                 qPrintable(tipsFile));
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
    std::ifstream is(resolvedTipsFile.toStdString());
    if(!is) {
        return 0;
    }

    Q_FOREVER {
        int blkLine = 0;
        QString body;
        int code = nextTFBlock(is, header, body, &blkLine, &currLine);

        if (code == TF_ERROR_EOF) {
            qWarning("NEdit: Warning: unexpected EOF in calltips file.");
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
            nTipsAdded += addTag(header, resolvedTipsFile, langMode, QString(), blkLine, tipPath, index);
			break;
		case TF_INCLUDE:
        {
            // nextTFBlock returns a colon-separated list of tips files in body
            auto ss = body;
            QStringList segments = ss.split(QLatin1Char(':'));
            for(const QString &tipIncFile : segments) {
                //qDebug("NEdit: including tips file '%s'", qPrintable(tipIncFile));
                nTipsAdded += loadTipsFile(tipIncFile, index, recLevel + 1);
            }
            break;
        }
		case TF_LANGUAGE:
            // Switch to the new language mode if it's valid, else ignore it.
			oldLangMode = langMode;
            langMode = FindLanguageMode(header);
            if (langMode == PLAIN_LANGUAGE_MODE && header != QLatin1String("Plain")) {

                qWarning("NEdit: Error reading calltips file:\n\t%s\nUnknown language mode: \"%s\"",
                         qPrintable(tipsFile),
                         qPrintable(header));

				langMode = oldLangMode;
			}
			break;
		case TF_ERROR:
            qWarning("NEdit: Warning: Recoverable error while reading calltips file:\n   \"%s\"",
                     qPrintable(resolvedTipsFile));
			break;
		case TF_ALIAS:
			// Allocate a new alias struct 
            aliases.push_front(tf_alias{ header, body });
			break;
		default:
			; // Ignore TF_VERSION for now 
		}
	}

    // Now resolve any aliases
    for(const tf_alias &tmp_alias : aliases) {

        QList<Tag> tags = getTag(tmp_alias.dest, TagSearchMode::TIP);

        if (tags.isEmpty()) {
            qWarning("NEdit: Can't find destination of alias \"%s\"\n"
                     "in calltips file:\n   \"%s\"\n",
                     qPrintable(tmp_alias.dest),
                     qPrintable(resolvedTipsFile));
		} else {

            const Tag &first_tag = tags[0];

            QStringList segments = tmp_alias.sources.split(QLatin1Char(':'));
            for(const QString &src : segments) {
                addTag(src, resolvedTipsFile, first_tag.language, QString(), first_tag.posInf, tipPath, index);
            }
		}
	}

	return nTipsAdded;
}
