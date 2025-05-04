
#include "Theme.h"
#include "Highlight.h"
#include "HighlightStyle.h"
#include "Settings.h"
#include "Util/algorithm.h"

#include <QDomDocument>
#include <QDomElement>
#include <QFile>
#include <QSettings>
#include <QString>

namespace Theme {
namespace {

const auto DefaultTextFG      = QStringLiteral("#221f1e");
const auto DefaultTextBG      = QStringLiteral("#d6d2d0");
const auto DefaultSelFG       = QStringLiteral("#ffffff");
const auto DefaultSelBG       = QStringLiteral("#43ace8");
const auto DefaultHighlightFG = QStringLiteral("white"); /* These are colors for flashing */
const auto DefaultHighlightBG = QStringLiteral("red");   /* matching parens. */
const auto DefaultLineNumFG   = QStringLiteral("black");
const auto DefaultLineNumBG   = QStringLiteral("#d6d2d0");
const auto DefaultCursorFG    = QStringLiteral("black");

}

/**
 * @brief Load the theme from the specified file or the default one if not found.
 */
void Load() {
	const QString filename = Settings::ThemeFile();

	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly)) {
		file.setFileName(QStringLiteral(":/DefaultStyles.xml"));
		if (!file.open(QIODevice::ReadOnly)) {
			qFatal("NEdit: failed to open theme file!");
		}
	}

	QDomDocument xml;
	if (xml.setContent(&file)) {
		const QDomElement root = xml.firstChildElement(QStringLiteral("theme"));

		// load basic color Settings::..
		const QDomElement text      = root.firstChildElement(QStringLiteral("text"));
		const QDomElement selection = root.firstChildElement(QStringLiteral("selection"));
		const QDomElement highlight = root.firstChildElement(QStringLiteral("highlight"));
		const QDomElement cursor    = root.firstChildElement(QStringLiteral("cursor"));
		const QDomElement lineno    = root.firstChildElement(QStringLiteral("line-numbers"));

		Settings::colors[ColorTypes::TEXT_BG_COLOR]   = text.attribute(QStringLiteral("background"), DefaultTextBG);
		Settings::colors[ColorTypes::TEXT_FG_COLOR]   = text.attribute(QStringLiteral("foreground"), DefaultTextFG);
		Settings::colors[ColorTypes::SELECT_BG_COLOR] = selection.attribute(QStringLiteral("background"), DefaultSelBG);
		Settings::colors[ColorTypes::SELECT_FG_COLOR] = selection.attribute(QStringLiteral("foreground"), DefaultSelFG);
		Settings::colors[ColorTypes::HILITE_BG_COLOR] = highlight.attribute(QStringLiteral("background"), DefaultHighlightBG);
		Settings::colors[ColorTypes::HILITE_FG_COLOR] = highlight.attribute(QStringLiteral("foreground"), DefaultHighlightFG);
		Settings::colors[ColorTypes::LINENO_FG_COLOR] = lineno.attribute(QStringLiteral("foreground"), DefaultLineNumFG);
		Settings::colors[ColorTypes::LINENO_BG_COLOR] = lineno.attribute(QStringLiteral("background"), DefaultLineNumBG);
		Settings::colors[ColorTypes::CURSOR_FG_COLOR] = cursor.attribute(QStringLiteral("foreground"), DefaultCursorFG);

		// load styles for syntax highlighting...
		QDomElement style = root.firstChildElement(QStringLiteral("style"));
		for (; !style.isNull(); style = style.nextSiblingElement(QStringLiteral("style"))) {

			HighlightStyle hs;
			hs.name            = style.attribute(QStringLiteral("name"));
			hs.color           = style.attribute(QStringLiteral("foreground"), QStringLiteral("black"));
			hs.bgColor         = style.attribute(QStringLiteral("background"), QString());
			const QString font = style.attribute(QStringLiteral("font"), QStringLiteral("Plain"));

			if (hs.name.isEmpty()) {
				qWarning("NEdit: style name required");
				continue;
			}

			if (hs.color.isEmpty()) {
				qWarning("NEdit: color name required in: %s", qPrintable(hs.name));
				continue;
			}

			// map the font to it's associated value
			if (font == QStringLiteral("Plain")) {
				hs.font = Font::Plain;
			} else if (font == QStringLiteral("Italic")) {
				hs.font = Font::Italic;
			} else if (font == QStringLiteral("Bold")) {
				hs.font = Font::Bold;
			} else if (font == QStringLiteral("Bold Italic")) {
				hs.font = Font::Italic | Font::Bold;
			} else {
				qWarning("NEdit: unrecognized font type %s in %s", qPrintable(font), qPrintable(hs.name));
				continue;
			}

			// pattern set was read correctly, add/change it in the list
			Upsert(Highlight::HighlightStyles, hs, [&hs](const HighlightStyle &entry) {
				return entry.name == hs.name;
			});
		}
	}
}

/**
 * @brief Save the current theme to the default theme file.
 */
void Save() {
	const QString filename = Settings::ThemeFile();

	QFile file(filename);
	if (file.open(QIODevice::WriteOnly)) {
		QDomDocument xml;
		const QDomProcessingInstruction pi = xml.createProcessingInstruction(QStringLiteral("xml"), QStringLiteral(R"(version="1.0" encoding="UTF-8")"));

		xml.appendChild(pi);

		QDomElement root = xml.createElement(QStringLiteral("theme"));
		root.setAttribute(QStringLiteral("name"), QStringLiteral("default"));
		xml.appendChild(root);

		// save basic color Settings::..
		{
			QDomElement text = xml.createElement(QStringLiteral("text"));
			text.setAttribute(QStringLiteral("foreground"), Settings::colors[ColorTypes::TEXT_FG_COLOR]);
			text.setAttribute(QStringLiteral("background"), Settings::colors[ColorTypes::TEXT_BG_COLOR]);
			root.appendChild(text);
		}

		{
			QDomElement selection = xml.createElement(QStringLiteral("selection"));
			selection.setAttribute(QStringLiteral("foreground"), Settings::colors[ColorTypes::SELECT_FG_COLOR]);
			selection.setAttribute(QStringLiteral("background"), Settings::colors[ColorTypes::SELECT_BG_COLOR]);
			root.appendChild(selection);
		}

		{
			QDomElement highlight = xml.createElement(QStringLiteral("highlight"));
			highlight.setAttribute(QStringLiteral("foreground"), Settings::colors[ColorTypes::HILITE_FG_COLOR]);
			highlight.setAttribute(QStringLiteral("background"), Settings::colors[ColorTypes::HILITE_BG_COLOR]);
			root.appendChild(highlight);
		}

		{
			QDomElement cursor = xml.createElement(QStringLiteral("cursor"));
			cursor.setAttribute(QStringLiteral("foreground"), Settings::colors[ColorTypes::CURSOR_FG_COLOR]);
			root.appendChild(cursor);
		}

		{
			QDomElement lineno = xml.createElement(QStringLiteral("line-numbers"));
			lineno.setAttribute(QStringLiteral("foreground"), Settings::colors[ColorTypes::LINENO_FG_COLOR]);
			lineno.setAttribute(QStringLiteral("background"), Settings::colors[ColorTypes::LINENO_BG_COLOR]);
			root.appendChild(lineno);
		}

		// save styles for syntax highlighting...
		for (const HighlightStyle &hs : Highlight::HighlightStyles) {
			QDomElement style = xml.createElement(QStringLiteral("style"));
			style.setAttribute(QStringLiteral("name"), hs.name);
			style.setAttribute(QStringLiteral("foreground"), hs.color);
			if (!hs.bgColor.isEmpty()) {
				style.setAttribute(QStringLiteral("background"), hs.bgColor);
			}

			// map the font to it's associated value
			switch (hs.font) {
			case Font::Plain:
				style.setAttribute(QStringLiteral("font"), QStringLiteral("Plain"));
				break;
			case Font::Italic:
				style.setAttribute(QStringLiteral("font"), QStringLiteral("Italic"));
				break;
			case Font::Bold:
				style.setAttribute(QStringLiteral("font"), QStringLiteral("Bold"));
				break;
			case Font::Italic | Font::Bold:
				style.setAttribute(QStringLiteral("font"), QStringLiteral("Bold Italic"));
				break;
			default:
				qFatal("NEdit: internal error saving theme file");
			}

			root.appendChild(style);
		}

		QTextStream stream(&file);
		stream << xml.toString();
	}
}

}
