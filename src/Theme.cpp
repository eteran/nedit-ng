
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

const auto DefaultTextFG      = QLatin1String("#221f1e");
const auto DefaultTextBG      = QLatin1String("#d6d2d0");
const auto DefaultSelFG       = QLatin1String("#ffffff");
const auto DefaultSelBG       = QLatin1String("#43ace8");
const auto DefaultHighlightFG = QLatin1String("white"); /* These are colors for flashing */
const auto DefaultHighlightBG = QLatin1String("red");   /* matching parens. */
const auto DefaultLineNumFG   = QLatin1String("black");
const auto DefaultLineNumBG   = QLatin1String("#d6d2d0");
const auto DefaultCursorFG    = QLatin1String("black");

}

/**
 * @brief Load the theme from the specified file or the default one if not found.
 */
void Load() {
	const QString filename = Settings::ThemeFile();

	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly)) {
		file.setFileName(QLatin1String(":/DefaultStyles.xml"));
		if (!file.open(QIODevice::ReadOnly)) {
			qFatal("NEdit: failed to open theme file!");
		}
	}

	QDomDocument xml;
	if (xml.setContent(&file)) {
		const QDomElement root = xml.firstChildElement(QLatin1String("theme"));

		// load basic color Settings::..
		const QDomElement text      = root.firstChildElement(QLatin1String("text"));
		const QDomElement selection = root.firstChildElement(QLatin1String("selection"));
		const QDomElement highlight = root.firstChildElement(QLatin1String("highlight"));
		const QDomElement cursor    = root.firstChildElement(QLatin1String("cursor"));
		const QDomElement lineno    = root.firstChildElement(QLatin1String("line-numbers"));

		Settings::colors[ColorTypes::TEXT_BG_COLOR]   = text.attribute(QLatin1String("background"), DefaultTextBG);
		Settings::colors[ColorTypes::TEXT_FG_COLOR]   = text.attribute(QLatin1String("foreground"), DefaultTextFG);
		Settings::colors[ColorTypes::SELECT_BG_COLOR] = selection.attribute(QLatin1String("background"), DefaultSelBG);
		Settings::colors[ColorTypes::SELECT_FG_COLOR] = selection.attribute(QLatin1String("foreground"), DefaultSelFG);
		Settings::colors[ColorTypes::HILITE_BG_COLOR] = highlight.attribute(QLatin1String("background"), DefaultHighlightBG);
		Settings::colors[ColorTypes::HILITE_FG_COLOR] = highlight.attribute(QLatin1String("foreground"), DefaultHighlightFG);
		Settings::colors[ColorTypes::LINENO_FG_COLOR] = lineno.attribute(QLatin1String("foreground"), DefaultLineNumFG);
		Settings::colors[ColorTypes::LINENO_BG_COLOR] = lineno.attribute(QLatin1String("background"), DefaultLineNumBG);
		Settings::colors[ColorTypes::CURSOR_FG_COLOR] = cursor.attribute(QLatin1String("foreground"), DefaultCursorFG);

		// load styles for syntax highlighting...
		QDomElement style = root.firstChildElement(QLatin1String("style"));
		for (; !style.isNull(); style = style.nextSiblingElement(QLatin1String("style"))) {

			HighlightStyle hs;
			hs.name            = style.attribute(QLatin1String("name"));
			hs.color           = style.attribute(QLatin1String("foreground"), QLatin1String("black"));
			hs.bgColor         = style.attribute(QLatin1String("background"), QString());
			const QString font = style.attribute(QLatin1String("font"), QLatin1String("Plain"));

			if (hs.name.isEmpty()) {
				qWarning("NEdit: style name required");
				continue;
			}

			if (hs.color.isEmpty()) {
				qWarning("NEdit: color name required in: %s", qPrintable(hs.name));
				continue;
			}

			// map the font to it's associated value
			if (font == QLatin1String("Plain")) {
				hs.font = Font::Plain;
			} else if (font == QLatin1String("Italic")) {
				hs.font = Font::Italic;
			} else if (font == QLatin1String("Bold")) {
				hs.font = Font::Bold;
			} else if (font == QLatin1String("Bold Italic")) {
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
		const QDomProcessingInstruction pi = xml.createProcessingInstruction(QLatin1String("xml"), QLatin1String(R"(version="1.0" encoding="UTF-8")"));

		xml.appendChild(pi);

		QDomElement root = xml.createElement(QLatin1String("theme"));
		root.setAttribute(QLatin1String("name"), QLatin1String("default"));
		xml.appendChild(root);

		// save basic color Settings::..
		{
			QDomElement text = xml.createElement(QLatin1String("text"));
			text.setAttribute(QLatin1String("foreground"), Settings::colors[ColorTypes::TEXT_FG_COLOR]);
			text.setAttribute(QLatin1String("background"), Settings::colors[ColorTypes::TEXT_BG_COLOR]);
			root.appendChild(text);
		}

		{
			QDomElement selection = xml.createElement(QLatin1String("selection"));
			selection.setAttribute(QLatin1String("foreground"), Settings::colors[ColorTypes::SELECT_FG_COLOR]);
			selection.setAttribute(QLatin1String("background"), Settings::colors[ColorTypes::SELECT_BG_COLOR]);
			root.appendChild(selection);
		}

		{
			QDomElement highlight = xml.createElement(QLatin1String("highlight"));
			highlight.setAttribute(QLatin1String("foreground"), Settings::colors[ColorTypes::HILITE_FG_COLOR]);
			highlight.setAttribute(QLatin1String("background"), Settings::colors[ColorTypes::HILITE_BG_COLOR]);
			root.appendChild(highlight);
		}

		{
			QDomElement cursor = xml.createElement(QLatin1String("cursor"));
			cursor.setAttribute(QLatin1String("foreground"), Settings::colors[ColorTypes::CURSOR_FG_COLOR]);
			root.appendChild(cursor);
		}

		{
			QDomElement lineno = xml.createElement(QLatin1String("line-numbers"));
			lineno.setAttribute(QLatin1String("foreground"), Settings::colors[ColorTypes::LINENO_FG_COLOR]);
			lineno.setAttribute(QLatin1String("background"), Settings::colors[ColorTypes::LINENO_BG_COLOR]);
			root.appendChild(lineno);
		}

		// save styles for syntax highlighting...
		for (const HighlightStyle &hs : Highlight::HighlightStyles) {
			QDomElement style = xml.createElement(QLatin1String("style"));
			style.setAttribute(QLatin1String("name"), hs.name);
			style.setAttribute(QLatin1String("foreground"), hs.color);
			if (!hs.bgColor.isEmpty()) {
				style.setAttribute(QLatin1String("background"), hs.bgColor);
			}

			// map the font to it's associated value
			switch (hs.font) {
			case Font::Plain:
				style.setAttribute(QLatin1String("font"), QLatin1String("Plain"));
				break;
			case Font::Italic:
				style.setAttribute(QLatin1String("font"), QLatin1String("Italic"));
				break;
			case Font::Bold:
				style.setAttribute(QLatin1String("font"), QLatin1String("Bold"));
				break;
			case Font::Italic | Font::Bold:
				style.setAttribute(QLatin1String("font"), QLatin1String("Bold Italic"));
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
