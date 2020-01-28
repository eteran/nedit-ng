
#include "DialogPreferences.h"
#include "Preferences.h"
#include "Font.h"
#include "Settings.h"
#include "X11Colors.h"
#include "MainWindow.h"
#include "DocumentWidget.h"
#include "DialogSmartIndent.h"
#include <QPainter>
#include <QColorDialog>
#include <QMessageBox>

namespace {

constexpr int CategoryIconSize = 48;

/**
 * @brief toString
 * @param color
 * @return
 */
QString toString(const QColor &color) {
	return QString(QLatin1String("#%1")).arg((color.rgb() & 0x00ffffff), 6, 16, QLatin1Char('0'));
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
 * @brief DialogPreferences::DialogPreferences
 * @param parent
 * @param f
 */
DialogPreferences::DialogPreferences(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	ui.setupUi(this);

	// Setup navigation
	ui.listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
	ui.listWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	ui.listWidget->setIconSize(QSize(CategoryIconSize, CategoryIconSize));
	ui.listWidget->setMovement(QListView::Static);

	//auto filter = new QSortFilterProxyModel(this);
	//ui.listWidget->setModel(filter);

	connect(ui.listWidget, &QListWidget::currentRowChanged, this, [this](int index) {
		ui.pageTitle->setText(ui.listWidget->item(index)->text());
		ui.pageContainer->setCurrentIndex(index);
	});

	ui.listWidget->setCurrentRow(0);

	// setup the button box
	connect(ui.buttonBox, &QDialogButtonBox::rejected, this, &DialogPreferences::close);
	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &DialogPreferences::buttonBoxClicked);

	setupFonts();
	setupColors();
	setupDisplay();
	setupBackup();
}

/**
 * @brief DialogPreferences::applyFonts
 */
void DialogPreferences::applyFonts() {
	QFont font = ui.comboFont->currentFont();
	font.setPointSize(ui.comboFontSize->currentData().toInt());
	QString fontName = font.toString();

	Preferences::SetPrefFont(fontName);

	for(DocumentWidget *document : DocumentWidget::allDocuments()) {
		document->action_Set_Fonts(fontName);
	}
}

/**
 * @brief DialogPreferences::applyColors
 */
void DialogPreferences::applyColors() {
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

/**
 * @brief DialogPreferences::applyGeneral
 */
void DialogPreferences::applyDisplay() {

	Preferences::SetPrefStatsLine(ui.checkShowStatistics->isChecked());
	Preferences::SetPrefISearchLine(ui.checkShowIncrementalSearch->isChecked());
	Preferences::SetPrefLineNums(ui.checkShowLineNumbers->isChecked());
	Preferences::SetPrefBacklightChars(ui.checkApplyBacklighting->isChecked());

	std::vector<MainWindow *> windows = MainWindow::allWindows();
	for(MainWindow *win : windows) {
		win->action_Statistics_Line_toggled(ui.checkShowStatistics->isChecked());
		win->action_Incremental_Search_Line_toggled(ui.checkShowIncrementalSearch->isChecked());
		win->action_Show_Line_Numbers_toggled(ui.checkShowLineNumbers->isChecked());
		win->action_Apply_Backlighting_toggled(ui.checkApplyBacklighting->isChecked());
	}
}

/**
 * @brief DialogPreferences::applyBackup
 */
void DialogPreferences::applyBackup() {

	Preferences::SetPrefSaveOldVersion(ui.checkMakeBackupCopy->isChecked());
	Preferences::SetPrefAutoSave(ui.checkIncrementalBackup->isChecked());

	std::vector<MainWindow *> windows = MainWindow::allWindows();
	for(MainWindow *win : windows) {
		win->action_Make_Backup_Copy_toggled(ui.checkMakeBackupCopy->isChecked());
		win->action_Incremental_Backup_toggled(ui.checkIncrementalBackup->isChecked());
	}

}

/**
 * @brief DialogPreferences::setupFonts
 */
void DialogPreferences::setupFonts() {
	const QFont font = Font::fromString(Preferences::GetPrefFontName());

	for (int size : QFontDatabase::standardSizes()) {
		ui.comboFontSize->addItem(tr("%1").arg(size), size);
	}

	ui.comboFont->setCurrentFont(font);

	const int n = ui.comboFontSize->findData(font.pointSize());
	if (n != -1) {
		ui.comboFontSize->setCurrentIndex(n);
	}
}

void DialogPreferences::setupColors() {
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
}

/**
 * @brief DialogPreferences::setupGeneral
 */
void DialogPreferences::setupDisplay() {
	ui.checkShowStatistics->setChecked(Preferences::GetPrefStatsLine());
	ui.checkShowIncrementalSearch->setChecked(Preferences::GetPrefISearchLine());
	ui.checkShowLineNumbers->setChecked(Preferences::GetPrefLineNums());
	ui.checkApplyBacklighting->setChecked(Preferences::GetPrefBacklightChars());
}

/**
 * @brief DialogPreferences::setupBackup
 */
void DialogPreferences::setupBackup() {
	ui.checkMakeBackupCopy->setChecked(Preferences::GetPrefSaveOldVersion());
	ui.checkIncrementalBackup->setChecked(Preferences::GetPrefAutoSave());
}

/**
 * @brief DialogColors::chooseColor
 * @param edit
 */
QColor DialogPreferences::chooseColor(QPushButton *button, const QColor &currentColor) {

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
 * @brief DialogPreferences::buttonBoxClicked
 * @param button
 */
void DialogPreferences::buttonBoxClicked(QAbstractButton *button) {

	switch (ui.buttonBox->standardButton(button)) {
	case QDialogButtonBox::Apply:
		applySettings();
		break;
	case QDialogButtonBox::Ok:
		acceptDialog();
		break;
	default:
		break;
	}
}

/**
 * @brief DialogPreferences::applySettings
 */
void DialogPreferences::applySettings() {
	applyFonts();
	applyColors();
	applyDisplay();
	applyBackup();
}

/**
 * @brief DialogPreferences::acceptDialog
 */
void DialogPreferences::acceptDialog() {
	applySettings();
}

