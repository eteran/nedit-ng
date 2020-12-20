
#include "DialogColors.h"
#include "DocumentWidget.h"
#include "Preferences.h"
#include "Settings.h"
#include "X11Colors.h"

#include <QColorDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QTimer>

namespace {

/**
 * @brief toString
 * @param color
 * @return
 */
QString toString(const QColor &color) {
	return QStringLiteral("#%1").arg((color.rgb() & 0x00ffffff), 6, 16, QLatin1Char('0'));
}

/**
 * @brief toIcon
 * @param color
 * @return
 */
QIcon toIcon(const QColor &color) {
	QPixmap pixmap(16, 16);
	QPainter painter(&pixmap);

	painter.fillRect(1, 1, 30, 30, color);

	painter.setPen(Qt::black);
	painter.drawRect(0, 0, 32, 32);

	return QIcon(pixmap);
}

}

/**
 * @brief DialogColors::DialogColors
 * @param parent
 * @param f
 */
DialogColors::DialogColors(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {
	ui.setupUi(this);

	QTimer::singleShot(0, this, [this]() {
		resize(0, 0);
	});

	textFG_        = X11Colors::fromString(Preferences::GetPrefColorName(TEXT_FG_COLOR));
	textBG_        = X11Colors::fromString(Preferences::GetPrefColorName(TEXT_BG_COLOR));
	selectionFG_   = X11Colors::fromString(Preferences::GetPrefColorName(SELECT_FG_COLOR));
	selectionBG_   = X11Colors::fromString(Preferences::GetPrefColorName(SELECT_BG_COLOR));
	matchFG_       = X11Colors::fromString(Preferences::GetPrefColorName(HILITE_FG_COLOR));
	matchBG_       = X11Colors::fromString(Preferences::GetPrefColorName(HILITE_BG_COLOR));
	lineNumbersFG_ = X11Colors::fromString(Preferences::GetPrefColorName(LINENO_FG_COLOR));
	lineNumbersBG_ = X11Colors::fromString(Preferences::GetPrefColorName(LINENO_BG_COLOR));
	cursorFG_      = X11Colors::fromString(Preferences::GetPrefColorName(CURSOR_FG_COLOR));

	ui.pushButtonFG->setText(Preferences::GetPrefColorName(TEXT_FG_COLOR));
	ui.pushButtonBG->setText(Preferences::GetPrefColorName(TEXT_BG_COLOR));
	ui.pushButtonSelectionFG->setText(Preferences::GetPrefColorName(SELECT_FG_COLOR));
	ui.pushButtonSelectionBG->setText(Preferences::GetPrefColorName(SELECT_BG_COLOR));
	ui.pushButtonMatchFG->setText(Preferences::GetPrefColorName(HILITE_FG_COLOR));
	ui.pushButtonMatchBG->setText(Preferences::GetPrefColorName(HILITE_BG_COLOR));
	ui.pushButtonLineNumbersFG->setText(Preferences::GetPrefColorName(LINENO_FG_COLOR));
	ui.pushButtonLineNumbersBG->setText(Preferences::GetPrefColorName(LINENO_BG_COLOR));
	ui.pushButtonCursor->setText(Preferences::GetPrefColorName(CURSOR_FG_COLOR));

	ui.pushButtonFG->setIcon(toIcon(textFG_));
	ui.pushButtonBG->setIcon(toIcon(textBG_));
	ui.pushButtonSelectionFG->setIcon(toIcon(selectionFG_));
	ui.pushButtonSelectionBG->setIcon(toIcon(selectionBG_));
	ui.pushButtonMatchFG->setIcon(toIcon(matchFG_));
	ui.pushButtonMatchBG->setIcon(toIcon(matchBG_));
	ui.pushButtonLineNumbersFG->setIcon(toIcon(lineNumbersFG_));
	ui.pushButtonLineNumbersBG->setIcon(toIcon(lineNumbersBG_));
	ui.pushButtonCursor->setIcon(toIcon(cursorFG_));

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
 * @brief DialogColors::chooseColor
 * @param edit
 */
QColor DialogColors::chooseColor(QPushButton *button, const QColor &currentColor) {

	QColor color = QColorDialog::getColor(currentColor, this);
	if (color.isValid()) {
		QPixmap pixmap(16, 16);
		pixmap.fill(color);
		button->setIcon(QIcon(pixmap));
		button->setText(toString(color));
	}

	return color;
}

/**
 * @brief DialogColors::updateColors
 *
 * Update the colors in the window or in the preferences
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

	Preferences::SetPrefColorName(TEXT_FG_COLOR, toString(textFG_));
	Preferences::SetPrefColorName(TEXT_BG_COLOR, toString(textBG_));
	Preferences::SetPrefColorName(SELECT_FG_COLOR, toString(selectionFG_));
	Preferences::SetPrefColorName(SELECT_BG_COLOR, toString(selectionBG_));
	Preferences::SetPrefColorName(HILITE_FG_COLOR, toString(matchFG_));
	Preferences::SetPrefColorName(HILITE_BG_COLOR, toString(matchBG_));
	Preferences::SetPrefColorName(LINENO_FG_COLOR, toString(lineNumbersFG_));
	Preferences::SetPrefColorName(LINENO_BG_COLOR, toString(lineNumbersBG_));
	Preferences::SetPrefColorName(CURSOR_FG_COLOR, toString(cursorFG_));
}
