
#include "DialogColors.h"
#include "DocumentWidget.h"
#include "Preferences.h"
#include "Settings.h"
#include "X11Colors.h"

#include <QColorDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>

namespace {

/**
 * @brief Converts a QColor to a string representation.
 *
 * @param color The QColor to convert.
 * @return The color in hexadecimal format.
 */
QString ToString(const QColor &color) {
	return QStringLiteral("#%1").arg((color.rgb() & 0x00ffffff), 6, 16, QLatin1Char('0'));
}

/**
 * @brief Converts a QColor to a 32x32 QIcon containing just the color.
 *
 * @param color The QColor to convert.
 * @return A QIcon representing the color.
 */
QIcon ToIcon(const QColor &color) {
	QPixmap pixmap(32, 32);
	QPainter painter(&pixmap);

	painter.fillRect(1, 1, 30, 30, color);
	painter.setPen(Qt::black);
	painter.drawRect(0, 0, 32, 32);

	return QIcon(pixmap);
}

}

/**
 * @brief Constructor for DialogColors class
 *
 * @param parent The parent widget, defaults to nullptr
 * @param f The window flags, defaults to Qt::WindowFlags()
 */
DialogColors::DialogColors(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {
	ui.setupUi(this);

	Dialog::shrinkToFit(this);

	textFG_        = X11Colors::FromString(Preferences::GetPrefColorName(TEXT_FG_COLOR));
	textBG_        = X11Colors::FromString(Preferences::GetPrefColorName(TEXT_BG_COLOR));
	selectionFG_   = X11Colors::FromString(Preferences::GetPrefColorName(SELECT_FG_COLOR));
	selectionBG_   = X11Colors::FromString(Preferences::GetPrefColorName(SELECT_BG_COLOR));
	matchFG_       = X11Colors::FromString(Preferences::GetPrefColorName(HILITE_FG_COLOR));
	matchBG_       = X11Colors::FromString(Preferences::GetPrefColorName(HILITE_BG_COLOR));
	lineNumbersFG_ = X11Colors::FromString(Preferences::GetPrefColorName(LINENO_FG_COLOR));
	lineNumbersBG_ = X11Colors::FromString(Preferences::GetPrefColorName(LINENO_BG_COLOR));
	cursorFG_      = X11Colors::FromString(Preferences::GetPrefColorName(CURSOR_FG_COLOR));

	ui.pushButtonFG->setText(Preferences::GetPrefColorName(TEXT_FG_COLOR));
	ui.pushButtonBG->setText(Preferences::GetPrefColorName(TEXT_BG_COLOR));
	ui.pushButtonSelectionFG->setText(Preferences::GetPrefColorName(SELECT_FG_COLOR));
	ui.pushButtonSelectionBG->setText(Preferences::GetPrefColorName(SELECT_BG_COLOR));
	ui.pushButtonMatchFG->setText(Preferences::GetPrefColorName(HILITE_FG_COLOR));
	ui.pushButtonMatchBG->setText(Preferences::GetPrefColorName(HILITE_BG_COLOR));
	ui.pushButtonLineNumbersFG->setText(Preferences::GetPrefColorName(LINENO_FG_COLOR));
	ui.pushButtonLineNumbersBG->setText(Preferences::GetPrefColorName(LINENO_BG_COLOR));
	ui.pushButtonCursor->setText(Preferences::GetPrefColorName(CURSOR_FG_COLOR));

	ui.pushButtonFG->setIcon(ToIcon(textFG_));
	ui.pushButtonBG->setIcon(ToIcon(textBG_));
	ui.pushButtonSelectionFG->setIcon(ToIcon(selectionFG_));
	ui.pushButtonSelectionBG->setIcon(ToIcon(selectionBG_));
	ui.pushButtonMatchFG->setIcon(ToIcon(matchFG_));
	ui.pushButtonMatchBG->setIcon(ToIcon(matchBG_));
	ui.pushButtonLineNumbersFG->setIcon(ToIcon(lineNumbersFG_));
	ui.pushButtonLineNumbersBG->setIcon(ToIcon(lineNumbersBG_));
	ui.pushButtonCursor->setIcon(ToIcon(cursorFG_));

	connect(ui.pushButtonFG, &QPushButton::clicked, this, [this]() {
		textFG_ = chooseColor(ui.pushButtonFG, textFG_);
	});

	connect(ui.pushButtonBG, &QPushButton::clicked, this, [this]() {
		textBG_ = chooseColor(ui.pushButtonBG, textBG_);
	});

	connect(ui.pushButtonSelectionFG, &QPushButton::clicked, this, [this]() {
		selectionFG_ = chooseColor(ui.pushButtonSelectionFG, selectionFG_);
	});

	connect(ui.pushButtonSelectionBG, &QPushButton::clicked, this, [this]() {
		selectionBG_ = chooseColor(ui.pushButtonSelectionBG, selectionBG_);
	});

	connect(ui.pushButtonMatchFG, &QPushButton::clicked, this, [this]() {
		matchFG_ = chooseColor(ui.pushButtonMatchFG, matchFG_);
	});

	connect(ui.pushButtonMatchBG, &QPushButton::clicked, this, [this]() {
		matchBG_ = chooseColor(ui.pushButtonMatchBG, matchBG_);
	});

	connect(ui.pushButtonLineNumbersFG, &QPushButton::clicked, this, [this]() {
		lineNumbersFG_ = chooseColor(ui.pushButtonLineNumbersFG, lineNumbersFG_);
	});

	connect(ui.pushButtonLineNumbersBG, &QPushButton::clicked, this, [this]() {
		lineNumbersBG_ = chooseColor(ui.pushButtonLineNumbersBG, lineNumbersBG_);
	});

	connect(ui.pushButtonCursor, &QPushButton::clicked, this, [this]() {
		cursorFG_ = chooseColor(ui.pushButtonCursor, cursorFG_);
	});

	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, [this](QAbstractButton *button) {
		if (ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
			updateColors();
		}
	});

	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, [this]() {
		updateColors();
		accept();
	});
}

/**
 * @brief Opens a color dialog to choose a color for the given button.
 * Updates the button's icon and text with the chosen color.
 *
 * @param button The button to update with the chosen color.
 * @param currentColor The current color to show in the dialog.
 * @return The chosen color, or an invalid color if the user cancels the dialog.
 */
QColor DialogColors::chooseColor(QPushButton *button, const QColor &currentColor) {

	QColor color = QColorDialog::getColor(currentColor, this);
	if (color.isValid()) {
		button->setIcon(ToIcon(color));
		button->setText(ToString(color));
	}

	return color;
}

/**
 * @brief Update the colors in the window or in the preferences
 */
void DialogColors::updateColors() {

	for (DocumentWidget *document : DocumentWidget::allDocuments()) {
		document->setColors(
			textFG_,
			textBG_,
			selectionFG_,
			selectionBG_,
			matchFG_,
			matchBG_,
			lineNumbersFG_,
			lineNumbersBG_,
			cursorFG_);
	}

	Preferences::SetPrefColorName(TEXT_FG_COLOR, ToString(textFG_));
	Preferences::SetPrefColorName(TEXT_BG_COLOR, ToString(textBG_));
	Preferences::SetPrefColorName(SELECT_FG_COLOR, ToString(selectionFG_));
	Preferences::SetPrefColorName(SELECT_BG_COLOR, ToString(selectionBG_));
	Preferences::SetPrefColorName(HILITE_FG_COLOR, ToString(matchFG_));
	Preferences::SetPrefColorName(HILITE_BG_COLOR, ToString(matchBG_));
	Preferences::SetPrefColorName(LINENO_FG_COLOR, ToString(lineNumbersFG_));
	Preferences::SetPrefColorName(LINENO_BG_COLOR, ToString(lineNumbersBG_));
	Preferences::SetPrefColorName(CURSOR_FG_COLOR, ToString(cursorFG_));
}
