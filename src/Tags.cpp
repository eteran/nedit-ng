
#include "Tags.h"
#include "DocumentWidget.h"
#include "LanguageMode.h"
#include "MainWindow.h"
#include "Preferences.h"
#include "Search.h"
#include "TextArea.h"
#include "TextBuffer.h"
#include "Util/FileSystem.h"
#include "Util/Input.h"
#include "Util/User.h"

#include <gsl/gsl_util>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTextStream>

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <unordered_map>

#ifdef Q_OS_UNIX
#include <sys/param.h>
#endif

namespace Tags {

namespace {

int loadTagsFile(const QString &tagSpec, int index, int recLevel);

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

constexpr int MAX_LINE                        = 2048;
constexpr int MAX_TAG_INCLUDE_RECURSION_LEVEL = 5;

/* Take this many lines when making a tip from a tag.
   (should probably be a language-dependent option, but...) */
constexpr int TIP_DEFAULT_LINES = 4;

// used  in AddRelTagsFile and AddTagsFile
int16_t tagFileIndex = 0;

QMultiHash<QString, Tag> LoadedTags;
QMultiHash<QString, Tag> LoadedTips;

// Check if a line has non-ws characters
bool lineEmpty(const QString &line) {

	for (QChar ch : line) {
		if (ch == QLatin1Char('\n')) {
			break;
		}

		if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t')) {
			return false;
		}
	}
	return true;
}

QString rstrip(QString s) {
	static const QRegularExpression re(QLatin1String("\\s*\\n"));
	return s.replace(re, QString());
}

QList<Tag> getTagFromTable(QMultiHash<QString, Tag> &table, const QString &name) {
	return table.values(name);
}

/*
 * Given a string and a position, advance the position by n lines, where line
 * separators (for now) are \n.  If the end of string is reached before n
 * lines, return the number of lines advanced, else normally return -1.
 */
int64_t moveAheadNLines(view::string_view str, int64_t &pos, int64_t n) {

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

/**
 * @brief hashTableByType
 * @param mode
 * @return
 */
QMultiHash<QString, Tag> *hashTableByType(SearchMode mode) {
	if (mode == SearchMode::TIP) {
		return &LoadedTips;
	} else {
		return &LoadedTags;
	}
}

/**
 * @brief tagListByType
 * @param mode
 * @return
 */
std::deque<File> *tagListByType(SearchMode mode) {
	if (mode == SearchMode::TAG) {
		return &TagsFileList;
	} else {
		return &TipsFileList;
	}
}

/*  Delete a tag from the cache.
 *  Search is limited to valid matches of 'name','file', 'search', posInf, and 'index'.
 *  EX: delete all tags matching index 2 ==>
 *                      delTag(tagname,nullptr,-2,nullptr,-2,2);
 *  (posInf = -2 is an invalid match, posInf range: -1 .. +MAXINT,
	 lang = -2 is also an invalid match)
 */
bool delTag(int index) {
	int del = 0;

	QMultiHash<QString, Tag> *table = hashTableByType(searchMode);

	if (table->isEmpty()) {
		return false;
	}

	for (auto it = table->begin(); it != table->end();) {
		if (it->index == index) {
			it = table->erase(it);
			++del;
		} else {
			++it;
		}
	}

	return del > 0;
}

/*
** Scans one <line> from a ctags tags file (<index>) in tagPath.
** Return value: Number of tag specs added.
*/
int scanCTagsLine(const QString &line, const QString &tagPath, int index) {

	static const auto regex = QRegularExpression(QLatin1String(R"(^([^\t]+)\t([^\t]+)\t([^\n]+)$)"));

	QRegularExpressionMatch match = regex.match(line);
	if (!match.hasMatch()) {
		return 0;
	}

	if (match.lastCapturedIndex() != 3) {
		return 0;
	}

	QString name         = match.captured(1);
	QString file         = match.captured(2);
	QString searchString = match.captured(3);

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

		if (posTagREEnd == -1 ||
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
		if (searchString.startsWith(searchString.right(1))) {
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
int scanETagsLine(const QString &line, const QString &tagPath, int index, QString &file, int recLevel) {

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
		QString searchString = line.left(posDEL);
		QString name         = line.mid(posDEL + 1, (posSOH - posDEL) - 1);
		int pos              = line.midRef(posCOM + 1).toInt();

		// No ability to set language mode for the moment
		return addTag(name, file, PLAIN_LANGUAGE_MODE, searchString, pos, tagPath, index);
	}

	if (!file.isEmpty() && posDEL != -1 && (posCOM > posDEL)) {
		// old etags style, part  name<soh>  is missing here!
		QString searchString = line.left(posDEL);

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
		pos          = line.midRef(posCOM + 1).toInt();

		return addTag(name, file, PLAIN_LANGUAGE_MODE, searchString, pos, tagPath, index);
	}

	// check for destination file spec
	if (!line.isEmpty() && posCOM != -1) {

		file = line.left(posCOM);

		// check if that's an include file ...
		if (line.midRef(posCOM + 1, 7) == QLatin1String("include")) {

			if (!QFileInfo(file).isAbsolute()) {

				QString incPath = NormalizePathname(tr("%1%2").arg(tagPath, file));
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
int loadTagsFile(const QString &tagSpec, int index, int recLevel) {

	int nTagsAdded  = 0;
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
	if (!f.open(QIODevice::ReadOnly)) {
		return 0;
	}

	const PathInfo tagPathInfo = parseFilename(resolvedTagsFile);

	QString filename;

	QTextStream stream(&f);

	while (!stream.atEnd()) {
		/* This might take a while if you have a huge tags file (like I do)..
		   keep the windows up to date and post a busy cursor so the user
		   doesn't think we died. */
		MainWindow::allDocumentsBusy(tr("Loading tags file..."));

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
			nTagsAdded += scanCTagsLine(line, tagPathInfo.pathname, index);
		} else {
			nTagsAdded += scanETagsLine(line, tagPathInfo.pathname, index, filename, recLevel);
		}
	}

	MainWindow::allDocumentsUnbusy();
	return nTagsAdded;
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
CalltipToken nextTFBlock(QTextStream &stream, QString &header, QString &body, int *blkLine, int *currLine) {

	// These are the different kinds of tokens
	static const auto commenTF_regex = QRegularExpression(QLatin1String(R"(^\s*\* comment \*\s*$)"));
	static const auto version_regex  = QRegularExpression(QLatin1String(R"(^\s*\* version \*\s*$)"));
	static const auto include_regex  = QRegularExpression(QLatin1String(R"(^\s*\* include \*\s*$)"));
	static const auto language_regex = QRegularExpression(QLatin1String(R"(^\s*\* language \*\s*$)"));
	static const auto alias_regex    = QRegularExpression(QLatin1String(R"(^\s*\* alias \*\s*$)"));

	QString line;
	CalltipToken code;

	// Skip blank lines and comments
	Q_FOREVER {

		// Skip blank lines
		while (stream.readLineInto(&line)) {
			++(*currLine);
			if (!lineEmpty(line)) {
				break;
			}
		}

		// Check for error or EOF
		if (stream.status() != QTextStream::Ok) {
			return TF_EOF;
		}

		// We've got a non-blank line -- is it a comment block?
		if (!searchLine(line, commenTF_regex)) {
			break;
		}

		// Skip the comment (non-blank lines)
		while (stream.readLineInto(&line)) {
			++(*currLine);
			if (lineEmpty(line)) {
				break;
			}
		}

		if (stream.status() != QTextStream::Ok) {
			return TF_EOF;
		}
	}

	// Now we know it's a meaningful block
	bool dummy1 = searchLine(line, include_regex);
	if (dummy1 || searchLine(line, alias_regex)) {
		// INCLUDE or ALIAS block

		qint64 incPos;
		int i;
		int incLines;

		// qDebug("NEdit: Starting include/alias at line %i", *currLine);
		if (dummy1) {
			code = TF_INCLUDE;
		} else {
			code = TF_ALIAS;
			// Need to read the header line for an alias

			const bool eof = !stream.readLineInto(&line);
			++(*currLine);
			if (eof) {
				return TF_ERROR_EOF;
			}

			if (lineEmpty(line)) {
				qWarning("NEdit: Warning: empty '* alias *' block in calltips file.");
				return TF_ERROR;
			}
			header = rstrip(line);
		}

		incPos   = stream.pos();
		*blkLine = *currLine + 1; // Line of first actual filename/alias

		if (incPos < 0) {
			return TF_ERROR;
		}

		// Figure out how long the block is
		while (stream.readLineInto(&line) || stream.atEnd()) {
			++(*currLine);
			if (stream.atEnd() || lineEmpty(line)) {
				break;
			}
		}

		incLines = *currLine - *blkLine;
		// Correct currLine for the empty line it read at the end
		--(*currLine);
		if (incLines == 0) {
			qWarning("NEdit: Warning: empty '* include *' or '* alias *' block in calltips file.");
			return TF_ERROR;
		}

		// Make space for the filenames/alias sources
		if (!stream.seek(incPos)) {
			return TF_ERROR;
		}

		// Read all the lines in the block
		for (i = 0; i < incLines; i++) {
			if (!stream.readLineInto(&line)) {
				return TF_ERROR_EOF;
			}

			QString currentLine = rstrip((line));

			if (i != 0) {
				body.push_back(QLatin1Char(':'));
			}

			body.append((currentLine));
		}
		// qDebug("Finished include/alias at line %i", *currLine);
	} else if (searchLine(line, language_regex)) {
		// LANGUAGE block
		const bool eof = !stream.readLineInto(&line);
		++(*currLine);

		if (eof) {
			return TF_ERROR_EOF;
		}

		if (lineEmpty(line)) {
			qWarning("NEdit: Warning: empty '* language *' block in calltips file.");
			return TF_ERROR;
		}
		*blkLine = *currLine;
		header   = rstrip(line);
		code     = TF_LANGUAGE;
	} else if (searchLine(line, version_regex)) {
		// VERSION block
		const bool eof = !stream.readLineInto(&line);
		++(*currLine);
		if (eof)
			return TF_ERROR_EOF;
		if (lineEmpty(line)) {
			qWarning("NEdit: Warning: empty '* version *' block in calltips file.");
			return TF_ERROR;
		}
		*blkLine = *currLine;
		header   = rstrip(line);
		code     = TF_VERSION;
	} else {
		// Calltip block
		/*  The first line is the key, the rest is the tip.
			Strip trailing whitespace. */
		header = rstrip(line);

		const bool eof = !stream.readLineInto(&line);
		++(*currLine);
		if (eof) {
			return TF_ERROR_EOF;
		}
		if (lineEmpty(line)) {
			qWarning("NEdit: Warning: empty calltip block:\n   \"%s\"", qPrintable(header));
			return TF_ERROR;
		}
		*blkLine = *currLine;
		body     = line;
		code     = TF_BLOCK;
	}

	// Skip the rest of the block
	const int dummy2 = *currLine;
	while (stream.readLineInto(&line)) {
		++(*currLine);
		if (lineEmpty(line)) {
			break;
		}
	}

	// Warn about any unneeded extra lines (which are ignored).
	if (dummy2 + 1 < *currLine && code != TF_BLOCK) {
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
int loadTipsFile(const QString &tipsFile, int index, int recLevel) {

	int currLine    = 0;
	int nTipsAdded  = 0;
	size_t langMode = PLAIN_LANGUAGE_MODE;
	std::vector<CalltipAlias> aliases;

	if (recLevel > MAX_TAG_INCLUDE_RECURSION_LEVEL) {
		qWarning("NEdit: Warning: Reached recursion limit before loading calltips file:\n\t%s",
				 qPrintable(tipsFile));
		return 0;
	}

	// find the tips file
	// Allow ~ in Unix filenames
	const QString tipFullPath = ExpandTilde(tipsFile);
	if (tipFullPath.isNull()) {
		return 0;
	}

	QFileInfo fi(tipFullPath);
	QString resolvedTipsFile = fi.canonicalFilePath();
	if (resolvedTipsFile.isEmpty()) {
		return 0;
	}

	// Get the path to the tips file
	const PathInfo tipPathInfo = parseFilename(resolvedTipsFile);

	QFile file(resolvedTipsFile);
	if (!file.open(QIODevice::ReadOnly)) {
		return 0;
	}

	QTextStream stream(&file);

	Q_FOREVER {
		int blkLine = 0;
		QString header;
		QString body;

		const CalltipToken code = nextTFBlock(stream, header, body, &blkLine, &currLine);

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
			nTipsAdded += addTag(header, resolvedTipsFile, langMode, QString(), blkLine, tipPathInfo.pathname, index);
			break;
		case TF_INCLUDE: {
			// nextTFBlock returns a colon-separated list of tips files in body
			const QStringList segments = body.split(QLatin1Char(':'));

			for (const QString &tipIncFile : segments) {
				nTipsAdded += loadTipsFile(tipIncFile, index, recLevel + 1);
			}
			break;
		}
		case TF_LANGUAGE: {
			// Switch to the new language mode if it's valid, else ignore it.
			const size_t oldLangMode = std::exchange(langMode, Preferences::FindLanguageMode(header));

			if (langMode == PLAIN_LANGUAGE_MODE && header != QLatin1String("Plain")) {

				qWarning("NEdit: Error reading calltips file:\n\t%s\nUnknown language mode: \"%s\"",
						 qPrintable(tipsFile),
						 qPrintable(header));

				langMode = oldLangMode;
			}
			break;
		}
		case TF_ERROR:
			qWarning("NEdit: Warning: Recoverable error while reading calltips file:\n   \"%s\"",
					 qPrintable(resolvedTipsFile));
			break;
		case TF_ALIAS:
			aliases.push_back({header, body});
			break;
		default:
			break; // Ignore TF_VERSION for now
		}
	}

	// Now resolve any aliases
	for (const CalltipAlias &alias : aliases) {

		QList<Tag> tags = getTag(alias.dest, SearchMode::TIP);

		if (tags.isEmpty()) {
			qWarning("NEdit: Can't find destination of alias \"%s\"\n"
					 "in calltips file:\n   \"%s\"\n",
					 qPrintable(alias.dest),
					 qPrintable(resolvedTipsFile));
		} else {

			const Tag &first_tag = tags[0];

			QStringList segments = alias.sources.split(QLatin1Char(':'));
			for (const QString &src : segments) {
				addTag(src, resolvedTipsFile, first_tag.language, QString(), first_tag.posInf, tipPathInfo.pathname, index);
			}
		}
	}

	return nTipsAdded;
}

}

/*
**  contents of                   tag->searchString   | tag->posInf
**    ctags, line num specified:  ""                  | line num
**    ctags, search expr specfd:  ctags search expr   | -1
**    etags  (emacs tags)         etags search string | search start pos
*/

// list of loaded tags/tips files
std::deque<File> TagsFileList;
std::deque<File> TipsFileList;

/* These are all transient global variables -- they don't hold any state
	between tag/tip lookups */
SearchMode searchMode = SearchMode::TAG;
QString tagName;

QString tagFiles[MaxDupTags];
QString tagSearch[MaxDupTags];
int64_t tagPosInf[MaxDupTags];

bool globAnchored;
CallTipPosition globPos;
TipHAlignMode globHAlign;
TipVAlignMode globVAlign;
TipAlignMode globAlignMode;

/* Add a tag specification to the hash table
**   Return Value:  false ... tag already existing, spec not added
**                  true  ... tag spec is new, added.
**   (We don't return boolean as the return value is used as counter increment!)
**
*/
int addTag(const QString &name, const QString &file, size_t lang, const QString &search, int64_t posInf, const QString &path, int index) {

	QMultiHash<QString, Tag> *const table = hashTableByType(searchMode);

	QString newFile;
	if (QFileInfo(file).isAbsolute()) {
		newFile = file;
	} else {
		newFile = tr("%1%2").arg(path, file);
	}

	newFile = NormalizePathname(newFile);

	QList<Tag> tags = table->values(name);
	for (const Tag &t : tags) {

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

			tmpFile = NormalizePathname(tmpFile);

			if (newFile != tmpFile) {
				continue;
			}
		}
		return 0;
	}

	Tag t = {name, file, search, path, lang, posInf, index};

	table->insert(name, t);
	return 1;
}

/*
** AddRelTagsFile():  Rescan tagSpec for relative tag file specs
** (not starting with [/~]) and extend the tag files list if in
** windowPath a tags file matching the relative spec has been found.
*/
bool addRelTagsFile(const QString &tagSpec, const QString &windowPath, SearchMode mode) {

	bool added = false;

	searchMode                 = mode;
	std::deque<File> *FileList = tagListByType(searchMode);

	if (tagSpec.isEmpty()) {
		return false;
	}

	QStringList filenames = tagSpec.split(QDir::listSeparator());

	for (const QString &filename : filenames) {
		if (QFileInfo(filename).isAbsolute() || filename.startsWith(QLatin1Char('~'))) {
			continue;
		}

		QString pathName;
		if (!windowPath.isEmpty()) {
			pathName = tr("%1/%2").arg(windowPath, filename);
		} else {
			pathName = tr("%1/%2").arg(QDir::currentPath(), filename);
		}

		pathName = NormalizePathname(pathName);

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
bool addTagsFile(const QString &tagSpec, SearchMode mode) {

	bool added = true;

	searchMode                 = mode;
	std::deque<File> *FileList = tagListByType(searchMode);

	if (tagSpec.isEmpty()) {
		return false;
	}

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
	QStringList filenames = tagSpec.split(QDir::listSeparator());
#else
	QStringList filenames = tagSpec.split(QLatin1Char(':'));
#endif
	for (const QString &filename : filenames) {

		QString pathName;
		if (!QFileInfo(filename).isAbsolute()) {
			pathName = tr("%1/%2").arg(QDir::currentPath(), filename);
		} else {
			pathName = filename;
		}

		pathName = NormalizePathname(pathName);

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
			1};

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
bool deleteTagsFile(const QString &tagSpec, SearchMode mode, bool force_unload) {

	if (tagSpec.isEmpty()) {
		return false;
	}

	searchMode                       = mode;
	std::deque<File> *const FileList = tagListByType(searchMode);

	bool removed = true;
#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
	QStringList filenames = tagSpec.split(QDir::listSeparator());
#else
	QStringList filenames = tagSpec.split(QLatin1Char(':'));
#endif
	for (const QString &filename : filenames) {

		QString pathName;
		if (!QFileInfo(filename).isAbsolute()) {
			pathName = tr("%1/%2").arg(QDir::currentPath(), filename);
		} else {
			pathName = filename;
		}

		pathName = NormalizePathname(pathName);

		auto it = FileList->begin();
		while (it != FileList->end()) {
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

			it = FileList->erase(it);

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

QList<Tag> lookupTagFromList(std::deque<File> *FileList, const QString &name, SearchMode mode) {

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
		for (File &tf : *FileList) {

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
QList<Tag> lookupTag(const QString &name, SearchMode mode) {

	searchMode = mode;
	if (searchMode == SearchMode::TIP) {
		return lookupTagFromList(&TipsFileList, name, mode);
	} else {
		return lookupTagFromList(&TagsFileList, name, mode);
	}
}

/*
** ctags search expressions are literal strings with a search direction flag,
** line starting "^" and ending "$" delimiters. This routine translates them
** into NEdit compatible regular expressions and does the search.
** Etags search expressions are plain literals strings, which
*/
bool fakeRegExSearch(view::string_view buffer, const QString &searchString, int64_t *startPos, int64_t *endPos) {

	if (searchString.isEmpty()) {
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

	{
		Input inPtr(&searchString);

		if (ctagsMode) {
			// searchString[0] is / or ? --> search dir
			++inPtr;

			if (*inPtr == QLatin1Char('^')) {
				// If the first char is a caret then it's a RE line start delim
				*outPtr++ = *inPtr++;
			}
		}

		while (!inPtr.atEnd()) {

			if ((inPtr[0] == QLatin1Char('\\') && inPtr[1] == QLatin1Char('/')) || (inPtr[0] == QLatin1Char('\r') && inPtr[1] == QLatin1Char('$') && inPtr[2] != QChar())) {
				/* Remove:
			   - escapes (added by standard and exuberant ctags) from slashes
			   - literal CRs generated by standard ctags for DOSified sources
			*/
				++inPtr;
			} else if (::strchr("()-[]<>{}.|^*+?&\\", inPtr[0].toLatin1()) || (inPtr[0] == QLatin1Char('$') && (inPtr[1] != QChar() || !ctagsMode))) {
				/* Escape RE Meta Characters to match them literally.
			   Don't escape $ if it's the last charcter of the search expr
			   in ctags mode; always escape $ in etags mode.
			 */
				*outPtr++ = QLatin1Char('\\');
				*outPtr++ = *inPtr++;
			} else if (inPtr[0].isSpace()) { // col. multiple spaces
				*outPtr++ = QLatin1Char('\\');
				*outPtr++ = QLatin1Char('s');
				*outPtr++ = QLatin1Char('+');
				do {
					++inPtr;
				} while (inPtr[0].isSpace());
			} else { // simply copy all other characters
				*outPtr++ = *inPtr++;
			}
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
void showMatchingCalltip(QWidget *parent, TextArea *area, int id) {
	try {
		int64_t startPos = 0;
		int64_t endPos   = 0;

		// 1. Open the target file
		NormalizePathname(tagFiles[id]);

		std::ifstream file(tagFiles[id].toStdString());
		if (!file) {
			QMessageBox::critical(
				parent,
				tr("Error opening File"),
				tr("Error opening %1").arg(tagFiles[id]));
			return;
		}

		// 2. Read the target file
		std::string fileString(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});

		// 3. Search for the tagged location (set startPos)
		if (tagSearch[id].isEmpty()) {
			// It's a line number, just go for it
			if ((moveAheadNLines(fileString, startPos, tagPosInf[id] - 1)) >= 0) {
				QMessageBox::critical(
					parent,
					tr("Tags Error"),
					tr("%1\n not long enough for definition to be on line %2").arg(tagFiles[id]).arg(tagPosInf[id]));
				return;
			}
		} else {
			startPos = tagPosInf[id];
			if (!fakeRegExSearch(fileString, tagSearch[id], &startPos, &endPos)) {
				QMessageBox::critical(
					parent,
					tr("Tag not found"),
					tr("Definition for %1 not found in %2").arg(tagName, tagFiles[id]));
				return;
			}
		}

		if (searchMode == SearchMode::TIP) {

			// 4. Find the end of the calltip (delimited by an empty line)
			endPos = startPos;

			Search::Result searchResult;

			bool found = Search::SearchString(
				fileString,
				QLatin1String(R"(\n\s*\n)"),
				Direction::Forward,
				SearchType::Regex,
				WrapMode::NoWrap,
				startPos,
				&searchResult,
				QString());

			if (!found) {
				// Just take 4 lines
				moveAheadNLines(fileString, endPos, TIP_DEFAULT_LINES);
				--endPos; // Lose the last \n
			} else {
				endPos = searchResult.start;
			}

		} else { // Mode = TIP_FROM_TAG
			// 4. Copy TIP_DEFAULT_LINES lines of text to the calltip string
			endPos = startPos;
			moveAheadNLines(fileString, endPos, TIP_DEFAULT_LINES);

			// Make sure not to overrun the fileString with ". . ."
			if (static_cast<size_t>(endPos) <= (fileString.size() - 5)) {
				fileString.replace(static_cast<size_t>(endPos), 5, ". . .");
				endPos += 5;
			}
		}

		// 5. Copy the calltip to a string
		int64_t tipLen = endPos - startPos;
		auto message   = QString::fromLatin1(&fileString[static_cast<size_t>(startPos)], gsl::narrow<int>(tipLen));

		// 6. Display it
		tagsShowCalltip(area, message);
	} catch (const std::bad_alloc &) {
		QMessageBox::critical(
			parent,
			tr("Out of Memory"),
			tr("Can't allocate memory"));
	} catch (const std::ios_base::failure &ex) {
		qWarning("NEdit: Error while reading file. %s", ex.what());
	}
}

/**
 * @brief searchLine
 * @param line
 * @param regex
 * @return
 */
bool searchLine(const QString &line, const QRegularExpression &re) {
	QRegularExpressionMatch match = re.match(line);
	return match.hasMatch();
}

/**
 * @brief Retrieve a Tag structure from the hash table
 * @param name
 * @param search_type
 * @return
 */
QList<Tag> getTag(const QString &name, SearchMode mode) {

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
int tagsShowCalltip(TextArea *area, const QString &text) {
	if (!text.isNull()) {
		return area->TextDShowCalltip(text, globAnchored, globPos, globHAlign, globVAlign, globAlignMode);
	} else {
		return 0;
	}
}

}
