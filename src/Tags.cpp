
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
#include "Util/algorithm.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTextStream>

#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>

#ifdef Q_OS_UNIX
#include <sys/param.h>
#endif

#include <gsl/gsl_util>

namespace Tags {

namespace {

int loadTagsFile(const QString &tagSpec, int index, int recLevel);
QList<Tag> getUniqueTags(QList<Tag> &tags);

struct CalltipAlias {
	QString dest;
	QString sources;
};

// Tag File Type
enum TFT : uint8_t {
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

/**
 * @brief Check if a line is empty or contains only whitespace characters.
 *
 * @param line The line to check.
 * @return true if the line is empty or contains only whitespace characters, false otherwise.
 */
bool lineEmpty(const QString &line) {

	for (const QChar ch : line) {
		if (ch == QLatin1Char('\n')) {
			break;
		}

		if (ch != QLatin1Char(' ') && ch != QLatin1Char('\t')) {
			return false;
		}
	}
	return true;
}

/**
 * @brief Splits a tag specification string into a list of file paths.
 *
 * @param tagSpec The tag specification string containing file paths separated by a specific character.
 * @return A QStringList containing the individual file paths.
 */
QStringList tagSpecToFileList(const QString &tagSpec) {
#ifdef Q_OS_WIN
	auto sep = QLatin1Char(';');
#else
	auto sep = QLatin1Char(':');
#endif
	return tagSpec.split(sep);
}

/**
 * @brief Remove trailing whitespace and newline characters from a string.
 *
 * @param s The string to process.
 * @return The processed string with trailing whitespace and newline characters removed.
 */
QString rstrip(QString s) {
	static const QRegularExpression re(QLatin1String("\\s*\\n"));
	return s.replace(re, QString());
}

/**
 * @brief Get unique tags from a list of tags.
 *
 * @param tags The list of tags to process.
 * @return A list of unique tags.
 */
QList<Tag> getTagFromTable(QMultiHash<QString, Tag> &table, const QString &name) {
	auto tags = table.values(name);
	tags      = getUniqueTags(tags);
	return tags;
}

/**
 * @brief Move the position ahead by n lines in the given string.
 * This function advances the position by n lines, where line
 * separators (for now) are '\n'.
 *
 * @param str The string to process.
 * @param pos The current position in the string, which will be updated.
 * @param n The number of lines to move ahead.
 * @return If the end of string is reached before n lines, return the number of lines advanced,
 * else normally return -1.
 */
int64_t moveAheadNLines(std::string_view str, int64_t &pos, int64_t n) {

	const int64_t i = n;
	while (static_cast<size_t>(pos) != str.size() && n > 0) {
		if (str[static_cast<size_t>(pos)] == '\n') {
			--n;
		}
		++pos;
	}

	if (n == 0) {
		return -1;
	}

	return i - n;
}

/**
 * @brief Get the hash table based on the search mode.
 *
 * @param mode The search mode to determine which hash table to return.
 * @return A pointer to the appropriate hash table.
 */
QMultiHash<QString, Tag> *hashTableByType(SearchMode mode) {
	if (mode == SearchMode::TIP) {
		return &LoadedTips;
	}

	return &LoadedTags;
}

/**
 * @brief Get the list of files based on the search mode.
 *
 * @param mode The search mode to determine which file list to return.
 * @return A pointer to the appropriate file list.
 */
std::deque<File> *tagListByType(SearchMode mode) {
	if (mode == SearchMode::TAG) {
		return &TagsFileList;
	}

	return &TipsFileList;
}

/**
 * @brief Delete a tag from the hash table based on its index.
 *
 * @param index The index of the tag to delete.
 * @return true if at least one tag was deleted, false otherwise.
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

/**
 * @brief Scans a line from a ctags tags file and adds the tag to the hash table.
 *
 * @param line The line to scan from the ctags file.
 * @param tagPath The path to the tags file.
 * @param index The index of the tag in the tags file.
 * @return The number of tag specifications added, or 0 if the line is not valid.
 */
int scanCTagsLine(const QString &line, const QString &tagPath, int index) {

	static const auto regex = QRegularExpression(QLatin1String(R"(^([^\t]+)\t([^\t]+)\t([^\n]+)$)"));

	const QRegularExpressionMatch match = regex.match(line);
	if (!match.hasMatch()) {
		return 0;
	}

	if (match.lastCapturedIndex() != 3) {
		return 0;
	}

	const QString name   = match.captured(1);
	const QString file   = match.captured(2);
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

		const int posTagREEnd = searchString.lastIndexOf(QLatin1Char(';'));

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

/**
 * @brief Scans a line from an etags tags file and adds the tag to the hash table.
 *
 * @param line The line to scan from the etags file.
 * @param tagPath The path to the tags file.
 * @param index The index of the tag in the tags file.
 * @param file The destination file for the tag definition, which may be modified.
 * @param recLevel The current recursion level for tags file inclusion.
 * @return The number of tag specifications added, or 0 if the line is not valid.
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
		const QString searchString = line.left(posDEL);
		const QString name         = line.mid(posDEL + 1, (posSOH - posDEL) - 1);
		const int pos              = line.mid(posCOM + 1).toInt();

		// No ability to set language mode for the moment
		return addTag(name, file, PLAIN_LANGUAGE_MODE, searchString, pos, tagPath, index);
	}

	if (!file.isEmpty() && posDEL != -1 && (posCOM > posDEL)) {
		// old etags style, part  name<soh>  is missing here!
		const QString searchString = line.left(posDEL);

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

		const QString name = searchString.mid(pos + 1, len - pos);
		pos                = line.mid(posCOM + 1).toInt();

		return addTag(name, file, PLAIN_LANGUAGE_MODE, searchString, pos, tagPath, index);
	}

	// check for destination file spec
	if (!line.isEmpty() && posCOM != -1) {

		file = line.left(posCOM);

		// check if that's an include file ...
		if (line.mid(posCOM + 1, 7) == QLatin1String("include")) {

			if (!QFileInfo(file).isAbsolute()) {
				const QString incPath = NormalizePathname(tr("%1%2").arg(tagPath, file));
				return loadTagsFile(incPath, index, recLevel + 1);
			}

			return loadTagsFile(file, index, recLevel + 1);
		}
	}

	return 0;
}

/**
 * @brief Loads a tags file into the hash table.
 *
 * @param tagSpec The specification of the tags file to load.
 * @param index The index of the tags file in the list of loaded tags files.
 * @param recLevel The current recursion level for tags file inclusion.
 * @return The number of tag specifications added, or 0 if the file could not be loaded.
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
	const QFileInfo fi(tagSpec);
	const QString resolvedTagsFile = fi.canonicalFilePath();
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

		const QString line = stream.readLine();

		/* the first character in the file decides if the file is treat as
		   etags or ctags file.
		 */
		if (tagFileType == TFT_CHECK) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
			if (line.startsWith(QChar::FormFeed)) { // <np>
#else
			if (line.startsWith(QChar(0x000c))) { // <np>
#endif
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

/**
 * @brief Get the next block from a tips file.
 * A block is a "\n\n+"" delimited set of lines in a calltips file.
 *
 * @param stream The QTextStream to read from the tips file.
 * @param header The header of the block, which depends on the block type.
 * @param body The body of the block, which depends on the block type.
 * @param blkLine The line number of the first line of the block after the "* xxxx *" line.
 * @param currLine The current line number in the file, which is updated as lines are read.
 * @return A CalltipToken representing the type of block found, or TF_EOF if the end of file is reached.
 */
CalltipToken nextTFBlock(QTextStream &stream, QString &header, QString &body, int *blkLine, int *currLine) {

	// These are the different kinds of tokens
	static const auto comment_regex  = QRegularExpression(QLatin1String(R"(^\s*\* comment \*\s*$)"));
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
		if (!searchLine(line, comment_regex)) {
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
	const bool dummy1 = searchLine(line, include_regex);
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

			const QString currentLine = rstrip((line));

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
		if (eof) {
			return TF_ERROR_EOF;
		}

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

/**
 * @brief Load a calltips file and insert all of the entries into the global tips database.
 * Each tip is essentially stored as its filename and the line at which it appears.
 * The exact same way ctags indexes source-code. That's why calltips and tags share so much code.
 *
 * @param tipsFile The path to the calltips file to load.
 * @param index The index of the calltips file in the list of loaded tips files.
 * @param recLevel The current recursion level for calltips file inclusion.
 * @return The number of tips added from the file, or 0 if the file could not be loaded.
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
	const QString tipFullPath = expandTilde(tipsFile);
	if (tipFullPath.isNull()) {
		return 0;
	}

	const QFileInfo fi(tipFullPath);
	const QString resolvedTipsFile = fi.canonicalFilePath();
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
				regex meta-characters that might appear in the string */
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

			const QStringList segments = alias.sources.split(QLatin1Char(':'));
			for (const QString &src : segments) {
				addTag(src, resolvedTipsFile, first_tag.language, QString(), first_tag.posInf, tipPathInfo.pathname, index);
			}
		}
	}

	return nTipsAdded;
}

/**
 * @brief Match a tag against a list of tags recursively.
 *
 * @param tags The list of tags to match against.
 * @param tag The tag to match.
 * @return true if a match is found, false otherwise.
 */
bool matchTagRecord(QList<Tag> &tags, Tag &tag) {
	QString newFile;
	if (QFileInfo(tag.file).isAbsolute()) {
		newFile = tag.file;
	} else {
		newFile = tr("%1%2").arg(tag.path, tag.file);
	}
	newFile = NormalizePathname(newFile);

	for (const Tag &t : tags) {

		if (tag.language != t.language) {
			continue;
		}

		if (tag.searchString != t.searchString) {
			continue;
		}

		if (tag.posInf != t.posInf) {
			continue;
		}

		if (QFileInfo(tag.file).isAbsolute() && (newFile != tag.file)) {
			continue;
		}

		if (QFileInfo(t.file).isAbsolute()) {

			auto tmpFile = tr("%1%2").arg(t.path, t.file);

			tmpFile = NormalizePathname(tmpFile);

			if (newFile != tmpFile) {
				continue;
			}
		}
		return true;
	}

	return false;
}

/**
 * @brief Get unique tags from a list of tags.
 * This function filters out duplicate tags based on their properties.
 *
 * @param tags The list of tags to filter.
 * @return A list of unique tags.
 */
QList<Tag> getUniqueTags(QList<Tag> &tags) {
	QList<Tag> ntags;

	if (tags.size() <= 1) {
		return tags;
	}

	for (Tag t1 : tags) {
		if (!matchTagRecord(ntags, t1)) {
			ntags.append(t1);
		}
	}

	return ntags;
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

// TODO(eteran): we really should avoid global state like this
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

/**
 * @brief Add a tag specification to the hash table.
 *
 * @param name The name of the tag.
 * @param file The file where the tag is defined.
 * @param lang The language mode of the tag.
 * @param search The search string for the tag, which may be empty.
 * @param posInf The position information for the tag, which may be -1 if not applicable.
 * @param path The path to the tag file.
 * @param index The index of the tag in the tags file.
 * @return 1 if the tag was added successfully, 0 otherwise.
 *
 * @note 2020-11-29: To help speed up the loading of large tag files,
 * we no longer check if the tag already exists in the hash table.
 * Instead, we delegate the checking and removal of repeated tags to
 * the getTag() function, which will return a list of unique tags.
 *
 * @note This function returns an int and not a bool because it is used in a context
 * where the return value is used to indicate the number of tags added.
 */
int addTag(const QString &name, const QString &file, size_t lang,
		   const QString &search, int64_t posInf, const QString &path, int index) {

	QMultiHash<QString, Tag> *const table = hashTableByType(searchMode);

	const Tag t = {name, file, search, path, lang, posInf, index};

	table->insert(name, t);
	return 1;
}

/**
 * @brief Add relative tags files to the tag list.
 * Rescan tagSpec for relative tag file specs
 * (not starting with [/~]) and extend the tag files list if in
 * windowPath a tags file matching the relative spec has been found.
 *
 * @param tagSpec The specification of the tags files to add, which may contain relative paths.
 * @param windowPath The path of the window where the tags files are located.
 * @param mode The search mode to use for the tags files (TAG or TIP).
 * @return true if at least one tags file was added, false otherwise.
 */
bool addRelTagsFile(const QString &tagSpec, const QString &windowPath, SearchMode mode) {

	bool added = false;

	searchMode                 = mode;
	std::deque<File> *FileList = tagListByType(searchMode);

	if (tagSpec.isEmpty()) {
		return false;
	}

	const QStringList filenames = tagSpecToFileList(tagSpec);
	for (const QString &filename : filenames) {
		if (QFileInfo(filename).isAbsolute() || filename.startsWith(QLatin1Char('~'))) {
			continue;
		}

		QString pathName;
		if (!windowPath.isEmpty()) {
			pathName = QStringLiteral("%1/%2").arg(windowPath, filename);
		} else {
			pathName = QStringLiteral("%1/%2").arg(QDir::currentPath(), filename);
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
		const QFileInfo fileInfo(pathName);
		const QDateTime timestamp = fileInfo.lastModified();
		if (timestamp.isNull()) {
			continue;
		}

		const File tag = {
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

/**
 * @brief Add tags files to the tag list.
 * This function sets up the list of tag files to manage from a file specification.
 * It can list multiple tags files, specified by separating them with colons.
 *
 * @param tagSpec The specification of the tags files to add, which may contain relative paths.
 * @param mode The search mode to use for the tags files (TAG or TIP).
 * @return true if all files were found in the FileList or loaded successfully, false otherwise.
 */
bool addTagsFile(const QString &tagSpec, SearchMode mode) {

	searchMode                 = mode;
	std::deque<File> *FileList = tagListByType(searchMode);

	if (tagSpec.isEmpty()) {
		return false;
	}

	const QStringList filenames = tagSpecToFileList(tagSpec);
	for (const QString &filename : filenames) {

		QString pathName;
		if (!QFileInfo(filename).isAbsolute()) {
			pathName = QStringLiteral("%1/%2").arg(QDir::currentPath(), filename);
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
			continue;
		}

		const QFileInfo fileInfo(pathName);
		const QDateTime timestamp = fileInfo.lastModified();

		if (timestamp.isNull()) {
			// Problem reading this tags file. Return false
			return false;
		}

		const File tag = {
			pathName,
			timestamp,
			false,
			++tagFileIndex,
			1};

		FileList->push_front(tag);
	}

	MainWindow::updateMenuItems();
	return true;
}

/**
 * @brief Delete tags files from the tag list.
 *
 * @param tagSpec The specification of the tags files to delete, which may contain relative paths.
 * @param mode The search mode to use for the tags files (TAG or TIP).
 * @param force_unload If true, force the deletion of calltips files even if their refcount is nonzero.
 * @return true if all files were found in the FileList and unloaded, false
 */
bool deleteTagsFile(const QString &tagSpec, SearchMode mode, bool force_unload) {

	if (tagSpec.isEmpty()) {
		return false;
	}

	searchMode                       = mode;
	std::deque<File> *const FileList = tagListByType(searchMode);

	const QStringList filenames = tagSpecToFileList(tagSpec);
	for (const QString &filename : filenames) {

		QString pathName;
		if (!QFileInfo(filename).isAbsolute()) {
			pathName = QStringLiteral("%1/%2").arg(QDir::currentPath(), filename);
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
			return false;
		}
	}

	return true;
}

/**
 * @brief Look up a tag in the list of tags files.
 *
 * @param FileList The list of tags files to search in.
 * @param name The name of the tag to look up.
 * @param mode The search mode to use for the tag lookup (TAG or TIP).
 * @return A list of tags that match the given name.
 */
QList<Tag> lookupTagFromList(std::deque<File> *FileList, const QString &name, SearchMode mode) {

	/*
	** Go through the list of all tags Files:
	**   - load them (if not already loaded)
	**   - check for update of the tags file and reload it in that case
	**   - save the modification date of the tags file
	**
	** Do this only as long as name != nullptr, not for successive calls
	** to find multiple tags specs.
	**
	*/
	if (!name.isNull()) {
		for (File &tf : *FileList) {

			int load_status;

			if (tf.loaded) {

				const QFileInfo fileInfo(tf.filename);
				const QDateTime timestamp = fileInfo.lastModified();

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

				const QFileInfo fileInfo(tf.filename);
				const QDateTime timestamp = fileInfo.lastModified();

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

/**
 * @brief lookup the file and path of the definition
 * and the proper search string.
 *
 * @param name The name of the tag to look up.
 * @param mode The search mode to use for the tag lookup (TAG or TIP).
 * @return A list of tags that match the given name.
 */
QList<Tag> lookupTag(const QString &name, SearchMode mode) {

	searchMode = mode;
	if (searchMode == SearchMode::TIP) {
		return lookupTagFromList(&TipsFileList, name, mode);
	}

	return lookupTagFromList(&TagsFileList, name, mode);
}

/*
**
**
**
**
*/

/**
 * @brief Perform a regex search in a buffer using a search string.
 * ctags search expressions are literal strings with a search direction flag,
 * line starting "^" and ending "$" delimiters. This routine translates them
 * into NEdit compatible regular expressions and does the search. Etags
 * search expressions are plain literals strings.
 *
 * @param buffer The buffer to search in.
 * @param searchString The search string to use, which may contain regex-like syntax.
 * @param startPos The starting position for the search, which may be -1 for etags mode.
 * @param endPos A pointer to store the end position of the found match.
 * @return true if a match is found, false otherwise.
 */
bool fakeRegExSearch(std::string_view buffer, const QString &searchString, int64_t *startPos, int64_t *endPos) {

	if (searchString.isEmpty()) {
		return false;
	}

	int64_t searchStartPos;
	Direction dir;
	bool ctagsMode;

	const std::string_view fileString = buffer;

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
		searchStartPos = ssize(fileString);
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
				   - literal CRs generated by standard ctags for DOS-ified sources
				*/
				++inPtr;
			} else if (::strchr("()-[]<>{}.|^*+?&\\", inPtr[0].toLatin1()) || (inPtr[0] == QLatin1Char('$') && (inPtr[1] != QChar() || !ctagsMode))) {
				/* Escape RE Meta Characters to match them literally.
				 * Don't escape $ if it's the last character of the search expr
				 * in ctags mode; always escape $ in etags mode.
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
	}

	// startPos, endPos left untouched by SearchString if search failed.
	QApplication::beep();
	return false;
}

/**
 * @brief Show a matching calltip for a tag.
 * This reads from either a source code file (if searchMode == TIP_FROM_TAG)
 * or a calltips file (if searchMode == TIP).
 * @param parent The parent widget for the calltip dialog.
 * @param area The text area where the calltip will be displayed.
 * @param id The index of the tag in the tag files list.
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

			const bool found = Search::SearchString(
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
		const int64_t tipLen = endPos - startPos;
		const auto message   = QString::fromLatin1(&fileString[static_cast<size_t>(startPos)], gsl::narrow<int>(tipLen));

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
 * @brief Search a line for a regular expression match.
 *
 * @param line The line to search in.
 * @param regex The regular expression to search for.
 * @return true if a match is found, false otherwise.
 */
bool searchLine(const QString &line, const QRegularExpression &re) {
	return re.match(line).hasMatch();
}

/**
 * @brief Get a tag from the loaded tags or tips.
 *
 * @param name The name of the tag to look up.
 * @param mode The search mode to use for the tag lookup (TAG or TIP).
 * @return A list of tags that match the given name.
 */
QList<Tag> getTag(const QString &name, SearchMode mode) {

	if (mode == SearchMode::TIP) {
		return getTagFromTable(LoadedTips, name);
	}

	return getTagFromTable(LoadedTags, name);
}

/**
 * @brief Show a calltip in the text area.
 *
 * @param area The text area where the calltip will be displayed.
 * @param text The text to display in the calltip.
 * @return true if the calltip was shown successfully, otherwise false.
 */
int tagsShowCalltip(TextArea *area, const QString &text) {
	if (!text.isNull()) {
		return area->TextDShowCalltip(text, globAnchored, globPos, globHAlign, globVAlign, globAlignMode);
	}

	return 0;
}

}
