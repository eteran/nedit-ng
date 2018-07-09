
#include "Tags.h"
#include "DocumentWidget.h"
#include "MainWindow.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "preferences.h"
#include "Search.h"
#include "LanguageMode.h"
#include "Util/fileUtils.h"
#include "Util/utils.h"

#include <gsl/gsl_util>

#include <QApplication>
#include <QMessageBox>
#include <QTextStream>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <iostream>
#include <fstream>
#include <cctype>
#include <cstdio>
#include <memory>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <sstream>
#include <deque>

#include <sys/param.h>

namespace {

struct CalltipAlias {
    QString dest;
    QString sources;
};

// Tag File Type
enum TFT {
    TFT_CHECK,
    TFT_ETAGS,
    TFT_CTAGS
};

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

constexpr int MAX_LINE						  = 2048;
constexpr int MAX_TAG_INCLUDE_RECURSION_LEVEL = 5;

/* Take this many lines when making a tip from a tag.
   (should probably be a language-dependent option, but...) */
constexpr int TIP_DEFAULT_LINES = 4;

// used  in AddRelTagsFile and AddTagsFile
int16_t tagFileIndex = 0;

QMultiHash<QString, Tags::Tag> LoadedTags;
QMultiHash<QString, Tags::Tag> LoadedTips;

// Check if a line has non-ws characters
bool lineEmpty(view::string_view line) {

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

QString rstrip(QString s) {
    static const QRegularExpression re(QLatin1String("\\s*\\n"));
    return s.replace(re, QString());
}

QList<Tags::Tag> getTagFromTable(QMultiHash<QString, Tags::Tag> &table, const QString &name) {
    return table.values(name);
}

/*
 * Given a string and a position, advance the position by n lines, where line
 * separators (for now) are \n.  If the end of string is reached before n
 * lines, return the number of lines advanced, else normally return -1.
 */
int64_t moveAheadNLinesEx(view::string_view str, int64_t &pos, int64_t n) {

    int64_t i = n;
    while (static_cast<size_t>(pos) != str.size() && n > 0) {
        if (str[static_cast<size_t>(pos)] == '\n') {
            --n;
        }
        ++pos;
    }

    if (n == 0) {
        return -1;
    } else {
        return i - n;
    }
}

}

/*
**  contents of                   tag->searchString   | tag->posInf
**    ctags, line num specified:  ""                  | line num
**    ctags, search expr specfd:  ctags search expr   | -1
**    etags  (emacs tags)         etags search string | search start pos
*/


// list of loaded tags/tips files
std::deque<Tags::File> Tags::TagsFileList;
std::deque<Tags::File> Tags::TipsFileList;

/* These are all transient global variables -- they don't hold any state
    between tag/tip lookups */
Tags::SearchMode Tags::searchMode = Tags::SearchMode::TAG;
QString          Tags::tagName;


QString Tags::tagFiles[MAXDUPTAGS];
QString Tags::tagSearch[MAXDUPTAGS];
int64_t Tags::tagPosInf[MAXDUPTAGS];

bool            Tags::globAnchored;
CallTipPosition Tags::globPos;
TipHAlignMode   Tags::globHAlign;
TipVAlignMode   Tags::globVAlign;
TipAlignMode    Tags::globAlignMode;


/* Add a tag specification to the hash table
**   Return Value:  false ... tag already existing, spec not added
**                  true  ... tag spec is new, added.
**   (We don't return boolean as the return value is used as counter increment!)
**
*/
int Tags::addTag(const QString &name, const QString &file, size_t lang, const QString &search, int64_t posInf, const QString &path, int index) {

    QMultiHash<QString, Tag> *const table = hashTableByType(searchMode);

    QString newFile;
    if (QFileInfo(file).isAbsolute()) {
        newFile = file;
	} else {
        newFile = tr("%1%2").arg(path, file);
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

            auto tmpFile = tr("%1%2").arg(t.path, t.file);

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

/*
** AddRelTagsFile():  Rescan tagSpec for relative tag file specs
** (not starting with [/~]) and extend the tag files list if in
** windowPath a tags file matching the relative spec has been found.
*/
bool Tags::AddRelTagsFileEx(const QString &tagSpec, const QString &windowPath, SearchMode mode) {

    bool added = false;

    searchMode = mode;
    std::deque<File> *FileList = tagListByType(searchMode);

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
            pathName = tr("%1/%2").arg(windowPath, filename);
        } else {
            pathName = tr("%1/%2").arg(GetCurrentDirEx(), filename);
        }

        pathName = NormalizePathnameEx(pathName);

        auto it = std::find_if(FileList->begin(), FileList->end(), [pathName](const File &tag) {
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

        File tag = {
            pathName,
            timestamp,
            false,
            ++tagFileIndex,
            1 // NOTE(eteran): added just so there aren't any uninitialized members
        };

        FileList->push_front(tag);
        added = true;
    }

    MainWindow::updateMenuItems();
    return added;
}

/*
** AddTagsFile():  Set up the the list of tag files to manage from a file spec.
** It can list multiple tags files, specified by separating them with colons.
** The config entry would look like this:
**      Nedit.tags: <tagfile1>:<tagfile2>
** Returns true if all files were found in the FileList or loaded successfully,
** false otherwise.
*/
bool Tags::AddTagsFileEx(const QString &tagSpec, SearchMode mode) {

    bool added = true;

    searchMode = mode;
    std::deque<File> *FileList = tagListByType(searchMode);

    if(tagSpec.isEmpty()) {
        return false;
    }

    QStringList filenames = tagSpec.split(QLatin1Char(':'));

    for(const QString &filename : filenames) {

		QString pathName;
        if(!QFileInfo(filename).isAbsolute()) {
            pathName = tr("%1/%2").arg(GetCurrentDirEx(), filename);
        } else {
			pathName = filename;
        }

		pathName = NormalizePathnameEx(pathName);

        auto it = std::find_if(FileList->begin(), FileList->end(), [pathName](const File &tag) {
			return tag.filename == pathName;
        });

        if (it != FileList->end()) {
            /* This file is already in the list.  It's easiest to just
             * refcount all tag/tip files even though we only actually care
             * about tip files. */

            ++(it->refcount);
            added = true;
            continue;
        }

        QFileInfo fileInfo(pathName);
        QDateTime timestamp = fileInfo.lastModified();

        if (timestamp.isNull()) {
            // Problem reading this tags file. Return false
            added = false;
            continue;
        }

        File tag = {
            pathName,
            timestamp,
            false,
            ++tagFileIndex,
            1
        };

        FileList->push_front(tag);
    }

    MainWindow::updateMenuItems();
    return added;
}

/* Un-manage a colon-delimited set of tags files
 * Return true if all files were found in the FileList and unloaded, false
 * if any file was not found in the FileList.
 * "mode" is either TAG or TIP
 * If "force_unload" is true, a calltips file will be deleted even if its
 * refcount is nonzero.
 */
bool Tags::DeleteTagsFileEx(const QString &tagSpec, SearchMode mode, bool force_unload) {

    if(tagSpec.isEmpty()) {
        return false;
    }

    searchMode = mode;
    std::deque<File> *const FileList = tagListByType(searchMode);

    bool removed = true;

    QStringList filenames = tagSpec.split(QLatin1Char(':'));

    for(const QString &filename : filenames) {

        QString pathName;
        if(!QFileInfo(filename).isAbsolute()) {
            pathName = tr("%1/%2").arg(GetCurrentDirEx(), filename);
        } else {
            pathName = filename;
        }

        pathName = NormalizePathnameEx(pathName);

        auto it = FileList->begin();
        while(it != FileList->end()) {
            File &t = *it;

            if (t.filename != pathName) {
                ++it;
                continue;
            }

            // Don't unload tips files with nonzero refcounts unless forced
            if (searchMode == SearchMode::TIP && !force_unload && --t.refcount > 0) {
                break;
            }

            if (t.loaded) {
                delTag(t.index);
            }

            FileList->erase(it);

            MainWindow::updateMenuItems();
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
** Scans one <line> from a ctags tags file (<index>) in tagPath.
** Return value: Number of tag specs added.
*/
int Tags::scanCTagsLine(const QString &line, const QString &tagPath, int index) {

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
int Tags::scanETagsLine(const QString &line, const QString &tagPath, int index, QString &file, int recLevel) {

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

                auto incPath = tr("%1%2").arg(tagPath, file);
                incPath = CompressPathnameEx(incPath);
                return loadTagsFile(incPath, index, recLevel + 1);
			} else {
                return loadTagsFile(file, index, recLevel + 1);
			}
		}
	}

	return 0;
}

/*
** Loads tagsFile into the hash table.
** Returns the number of added tag specifications.
*/
int Tags::loadTagsFile(const QString &tagSpec, int index, int recLevel) {

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
    QFileInfo fi(tagSpec);
    QString resolvedTagsFile = fi.canonicalFilePath();
    if (resolvedTagsFile.isEmpty()) {
		return 0;
	}

	QFile f(resolvedTagsFile);
	if(!f.open(QIODevice::ReadOnly)) {
		return 0;
	}

    // NOTE(eteran): no error checking...
	ParseFilenameEx(resolvedTagsFile, nullptr, &tagPath);

	/* This might take a while if you have a huge tags file (like I do)..
	   keep the windows up to date and post a busy cursor so the user
	   doesn't think we died. */
    MainWindow::AllWindowsBusyEx(tr("Loading tags file..."));

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

QList<Tags::Tag> Tags::LookupTagFromList(std::deque<File> *FileList, const QString &name, SearchMode mode) {

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
        for(File &tf : *FileList) {

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

    return getTag(name, mode);
}

/*
** Given a tag name, lookup the file and path of the definition
** and the proper search string.
*/
QList<Tags::Tag> Tags::LookupTag(const QString &name, SearchMode mode) {

    searchMode = mode;
    if (searchMode == SearchMode::TIP) {
        return LookupTagFromList(&TipsFileList, name, mode);
	} else {
        return LookupTagFromList(&TagsFileList, name, mode);
	}
}

/*
** ctags search expressions are literal strings with a search direction flag,
** line starting "^" and ending "$" delimiters. This routine translates them
** into NEdit compatible regular expressions and does the search.
** Etags search expressions are plain literals strings, which
*/
bool Tags::fakeRegExSearchEx(view::string_view buffer, const QString &searchString, int64_t *startPos, int64_t *endPos) {

    if(searchString.isEmpty()) {
        return false;
    }

    int64_t searchStartPos;
    Direction dir;
    bool ctagsMode;

    view::string_view fileString = buffer;

	// determine search direction and start position 
    if (*startPos != -1) { // etags mode!
        dir            = Direction::Forward;
        searchStartPos = *startPos;
        ctagsMode      = false;
    } else if (searchString.size() > 1 && searchString[0] == QLatin1Char('/')) {
        dir            = Direction::Forward;
        searchStartPos = 0;
        ctagsMode      = true;
    } else if (searchString.size() > 1 && searchString[0] == QLatin1Char('?')) {
        dir            = Direction::Backward;
        searchStartPos = static_cast<int64_t>(fileString.size());
        ctagsMode      = true;
	} else {
		qWarning("NEdit: Error parsing tag file search string");
        return false;
	}

	// Build the search regex. 
    QString searchSubs;
    searchSubs.reserve(3 * MAX_LINE + 3);
    auto outPtr = std::back_inserter(searchSubs);
    auto inPtr = searchString.begin();

    if (ctagsMode) {
        // searchString[0] is / or ? --> search dir
        ++inPtr;

        if (*inPtr == QLatin1Char('^')) {
            // If the first char is a caret then it's a RE line start delim
            *outPtr++ = *inPtr++;
        }
	}

    // TODO(eteran): this code is unsafe, and will blowup on makformed input!

    while (inPtr != searchString.end()) {
        if ((*inPtr == QLatin1Char('\\') && *std::next(inPtr) == QLatin1Char('/')) || (*inPtr == QLatin1Char('\r') && *std::next(inPtr) == QLatin1Char('$') && std::next(std::next(inPtr)) != searchString.end())) {
			/* Remove:
			   - escapes (added by standard and exuberant ctags) from slashes
			   - literal CRs generated by standard ctags for DOSified sources
			*/
			inPtr++;
        } else if (::strchr("()-[]<>{}.|^*+?&\\", inPtr->toLatin1()) || (*inPtr == QLatin1Char('$') && (std::next(inPtr) != searchString.end() || (!ctagsMode)))) {
			/* Escape RE Meta Characters to match them literally.
			   Don't escape $ if it's the last charcter of the search expr
			   in ctags mode; always escape $ in etags mode.
			 */
            *outPtr++ = QLatin1Char('\\');
            *outPtr++ = *inPtr++;
        } else if (inPtr->isSpace()) { // col. multiple spaces
            *outPtr++ = QLatin1Char('\\');
            *outPtr++ = QLatin1Char('s');
            *outPtr++ = QLatin1Char('+');
			do {
				inPtr++;
            } while (inPtr->isSpace());
		} else { // simply copy all other characters 
            *outPtr++ = *inPtr++;
		}
	}

    Search::Result searchResult;

    bool found = Search::SearchString(
                fileString,
                searchSubs,
                dir,
                SearchType::Regex,
                WrapMode::NoWrap,
                searchStartPos,
                &searchResult,
                QString());

	if (!found && !ctagsMode) {
		/* position of the target definition could have been drifted before
		   startPos, if nothing has been found by now try searching backward
		   again from startPos.
		*/
        found = Search::SearchString(
                    fileString,
                    searchSubs,
                    Direction::Backward,
                    SearchType::Regex,
                    WrapMode::NoWrap,
                    searchStartPos,
                    &searchResult,
                    QString());
	}

	// return the result 
	if (found) {
        *startPos = searchResult.start;
        *endPos   = searchResult.end;
        return true;
	} else {
		// startPos, endPos left untouched by SearchString if search failed. 
		QApplication::beep();
        return false;
	}
}


/*
** Show the calltip specified by tagFiles[i], tagSearch[i], tagPosInf[i]
** This reads from either a source code file (if searchMode == TIP_FROM_TAG)
** or a calltips file (if searchMode == TIP).
*/
void Tags::showMatchingCalltipEx(QWidget *parent, TextArea *area, size_t i) {
    try {
        int64_t startPos = 0;
        int64_t endPos   = 0;

        // 1. Open the target file
        NormalizePathnameEx(tagFiles[i]);

        std::ifstream file(tagFiles[i].toStdString());
        if(!file) {
            QMessageBox::critical(
                        parent,
                        tr("Error opening File"),
                        tr("Error opening %1").arg(tagFiles[i]));
            return;
        }

        // 2. Read the target file
        std::string fileString(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});

        // 3. Search for the tagged location (set startPos)
        if (tagSearch[i].isEmpty()) {
            // It's a line number, just go for it
            if ((moveAheadNLinesEx(fileString, startPos, tagPosInf[i] - 1)) >= 0) {
                QMessageBox::critical(
                            parent,
                            tr("Tags Error"),
                            tr("%1\n not long enough for definition to be on line %2").arg(tagFiles[i]).arg(tagPosInf[i]));
                return;
            }
        } else {
            startPos = tagPosInf[i];
            if (!fakeRegExSearchEx(fileString, tagSearch[i], &startPos, &endPos)) {
                QMessageBox::critical(
                            parent,
                            tr("Tag not found"),
                            tr("Definition for %1 not found in %2").arg(tagName, tagFiles[i]));
                return;
            }
        }

        if (searchMode == SearchMode::TIP) {

            // 4. Find the end of the calltip (delimited by an empty line)
            endPos = startPos;

            Search::Result searchResult;

            bool found = Search::SearchString(
                        fileString,
                        QLatin1String("\\n\\s*\\n"),
                        Direction::Forward,
                        SearchType::Regex,
                        WrapMode::NoWrap,
                        startPos,
                        &searchResult,
                        QString());

            if (!found) {
                // Just take 4 lines
                moveAheadNLinesEx(fileString, endPos, TIP_DEFAULT_LINES);
                --endPos; // Lose the last \n
            } else {
                endPos = searchResult.start;
            }

        } else { // Mode = TIP_FROM_TAG
            // 4. Copy TIP_DEFAULT_LINES lines of text to the calltip string
            endPos = startPos;
            moveAheadNLinesEx(fileString, endPos, TIP_DEFAULT_LINES);

            // Make sure not to overrun the fileString with ". . ."
            if (static_cast<size_t>(endPos) <= (fileString.size() - 5)) {
                fileString.replace(static_cast<size_t>(endPos), 5, ". . .");
                endPos += 5;
            }
        }

        // 5. Copy the calltip to a string
        int64_t tipLen = endPos - startPos;
        auto message = QString::fromLatin1(&fileString[static_cast<size_t>(startPos)], gsl::narrow<int>(tipLen));

        // 6. Display it
        tagsShowCalltipEx(area, message);
    } catch(const std::bad_alloc &) {
        QMessageBox::critical(
                    parent,
                    tr("Out of Memory"),
                    tr("Can't allocate memory"));
    }
}




/********************************************************************
 *           Functions for loading Calltips files                   *
 ********************************************************************/

/**
 * @brief A wrapper for SearchString
 * @param line
 * @param regex
 * @return
 */
bool Tags::searchLine(const std::string &line, const std::string &regex) {

    Search::Result searchResult;

    return Search::SearchString(
                line,
                QString::fromStdString(regex),
                Direction::Forward,
                SearchType::Regex,
                WrapMode::NoWrap,
                0,
                &searchResult,
                QString());
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
int Tags::nextTFBlock(std::istream &is, QString &header, QString &body, int *blkLine, int *currLine) {

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

/*
** Load a calltips file and insert all of the entries into the global tips
** database.  Each tip is essentially stored as its filename and the line
** at which it appears--the exact same way ctags indexes source-code.  That's
** why calltips and tags share so much code.
*/
int Tags::loadTipsFile(const QString &tipsFile, int index, int recLevel) {

    QString header;
    size_t oldLangMode;
    int currLine    = 0;
    int nTipsAdded  = 0;
    size_t langMode = PLAIN_LANGUAGE_MODE;
    std::deque<CalltipAlias> aliases;

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

    QFileInfo fi(tipPath);
    QString resolvedTipsFile = fi.canonicalFilePath();
    if(resolvedTipsFile.isEmpty()) {
        return 0;
    }

    // Get the path to the tips file
    // NOTE(eteran): no error checking...
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
            const QString &ss = body;
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
            langMode = Preferences::FindLanguageMode(header);
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
            aliases.push_front(CalltipAlias{ header, body });
			break;
		default:
			; // Ignore TF_VERSION for now 
		}
	}

    // Now resolve any aliases
    for(const CalltipAlias &tmp_alias : aliases) {

        QList<Tag> tags = getTag(tmp_alias.dest, SearchMode::TIP);

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

/*  Delete a tag from the cache.
 *  Search is limited to valid matches of 'name','file', 'search', posInf, and 'index'.
 *  EX: delete all tags matching index 2 ==>
 *                      delTag(tagname,nullptr,-2,nullptr,-2,2);
 *  (posInf = -2 is an invalid match, posInf range: -1 .. +MAXINT,
     lang = -2 is also an invalid match)
 */
bool Tags::delTag(int index) {
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

QMultiHash<QString, Tags::Tag> *Tags::hashTableByType(SearchMode mode) {
    if (mode == SearchMode::TIP) {
        return &LoadedTips;
    } else {
        return &LoadedTags;
    }
}

std::deque<Tags::File> *Tags::tagListByType(SearchMode mode) {
    if (mode == SearchMode::TAG) {
        return &TagsFileList;
    } else {
        return &TipsFileList;
    }
}

/**
 * @brief Retrieve a Tag structure from the hash table
 * @param name
 * @param search_type
 * @return
 */
QList<Tags::Tag> Tags::getTag(const QString &name, SearchMode mode) {

    if (mode == SearchMode::TIP) {
        return getTagFromTable(LoadedTips, name);
    } else {
        return getTagFromTable(LoadedTags, name);
    }
}

/**
 * @brief A wrapper for calling TextDShowCalltip
 * @param area
 * @param text
 * @return
 */
int Tags::tagsShowCalltipEx(TextArea *area, const QString &text) {
    if (!text.isNull()) {
        return area->TextDShowCalltip(text, globAnchored, globPos, globHAlign, globVAlign, globAlignMode);
    } else {
        return 0;
    }
}
