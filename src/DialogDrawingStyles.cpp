
#include "DialogDrawingStyles.h"
#include "DialogSyntaxPatterns.h"
#include "DocumentWidget.h"
#include "Highlight.h"
#include "HighlightStyleModel.h"
#include "Preferences.h"
#include "X11Colors.h"

#include <QMessageBox>
#include <QColorDialog>
#include <QRegularExpressionValidator>

/**
 * @brief DialogDrawingStyles::DialogDrawingStyles
 * @param parent
 * @param f
 */
DialogDrawingStyles::DialogDrawingStyles(DialogSyntaxPatterns *dialogSyntaxPatterns, std::vector<HighlightStyle> &highlightStyles, QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) , highlightStyles_(highlightStyles), dialogSyntaxPatterns_(dialogSyntaxPatterns) {
	ui.setupUi(this);
	connectSlots();

	ui.buttonNew   ->setIcon(QIcon::fromTheme(QLatin1String("document-new"), QIcon(QLatin1String(":/document-new.svg"))));
	ui.buttonDelete->setIcon(QIcon::fromTheme(QLatin1String("edit-delete"),  QIcon(QLatin1String(":/edit-delete.svg"))));
	ui.buttonCopy  ->setIcon(QIcon::fromTheme(QLatin1String("edit-copy"),    QIcon(QLatin1String(":/edit-copy.svg"))));
	ui.buttonUp    ->setIcon(QIcon::fromTheme(QLatin1String("go-up"),        QIcon(QLatin1String(":/go-up.svg"))));
	ui.buttonDown  ->setIcon(QIcon::fromTheme(QLatin1String("go-down"),      QIcon(QLatin1String(":/go-down.svg"))));

	model_ = new HighlightStyleModel(this);
	ui.listItems->setModel(model_);

	// Copy the list of highlight style information to one that the user can freely edit
	for(const HighlightStyle &style : highlightStyles) {
		model_->addItem(style);
	}

	connect(ui.listItems->selectionModel(), &QItemSelectionModel::currentChanged, this, &DialogDrawingStyles::currentChanged, Qt::QueuedConnection);
	connect(this, &DialogDrawingStyles::restore, ui.listItems, &QListView::setCurrentIndex, Qt::QueuedConnection);

	// default to selecting the first item
	if(model_->rowCount() != 0) {
		QModelIndex index = model_->index(0, 0);
		ui.listItems->setCurrentIndex(index);
	}

	// Valid characters are letters, numbers, _, -, +, $, #, and internal whitespace.
	static const QRegularExpression rx(QLatin1String("[\\sA-Za-z0-9_+$#-]+"));
	auto validator = new QRegularExpressionValidator(rx, this);
	ui.editName->setValidator(validator);
}

/**
 * @brief DialogDrawingStyles::connectSlots
 */
void DialogDrawingStyles::connectSlots() {
	connect(ui.buttonNew, &QPushButton::clicked, this, &DialogDrawingStyles::buttonNew_clicked);
	connect(ui.buttonCopy, &QPushButton::clicked, this, &DialogDrawingStyles::buttonCopy_clicked);
	connect(ui.buttonDelete, &QPushButton::clicked, this, &DialogDrawingStyles::buttonDelete_clicked);
	connect(ui.buttonUp, &QPushButton::clicked, this, &DialogDrawingStyles::buttonUp_clicked);
	connect(ui.buttonDown, &QPushButton::clicked, this, &DialogDrawingStyles::buttonDown_clicked);
	connect(ui.buttonForeground, &QPushButton::clicked, this, &DialogDrawingStyles::buttonForeground_clicked);
	connect(ui.buttonBackground, &QPushButton::clicked, this, &DialogDrawingStyles::buttonBackground_clicked);
	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &DialogDrawingStyles::buttonBox_accepted);
	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &DialogDrawingStyles::buttonBox_clicked);
}


/**
 * @brief DialogDrawingStyles::setStyleByName
 * @param name
 */
void DialogDrawingStyles::setStyleByName(const QString &name) {

	for(int i = 0; i < model_->rowCount(); ++i) {
		QModelIndex index = model_->index(i, 0);
		auto ptr = model_->itemFromIndex(index);
		if(ptr->name == name) {
			ui.listItems->setCurrentIndex(index);
			break;
		}
	}

	ui.listItems->scrollTo(ui.listItems->currentIndex());
	updateButtonStates();
}

/**
 * @brief DialogDrawingStyles::buttonNew_clicked
 */
void DialogDrawingStyles::buttonNew_clicked() {

	if(!updateCurrentItem()) {
		return;
	}

	HighlightStyle style;
	// some sensible defaults...
	style.name  = tr("New Item");
	style.color = tr("black");
	model_->addItem(style);

	QModelIndex index = model_->index(model_->rowCount() - 1, 0);
	ui.listItems->setCurrentIndex(index);

	ui.listItems->scrollTo(ui.listItems->currentIndex());
	updateButtonStates();
}

/**
 * @brief DialogDrawingStyles::buttonCopy_clicked
 */
void DialogDrawingStyles::buttonCopy_clicked() {

	if(!updateCurrentItem()) {
		return;
	}

	QModelIndex index = ui.listItems->currentIndex();
	if(index.isValid()) {
		auto ptr = model_->itemFromIndex(index);
		model_->addItem(*ptr);

		QModelIndex newIndex = model_->index(model_->rowCount() - 1, 0);
		ui.listItems->setCurrentIndex(newIndex);
	}

	ui.listItems->scrollTo(ui.listItems->currentIndex());
	updateButtonStates();
}

/**
 * @brief DialogDrawingStyles::buttonDelete_clicked
 */
void DialogDrawingStyles::buttonDelete_clicked() {

	QModelIndex index = ui.listItems->currentIndex();
	if(index.isValid()) {
		deleted_ = index;
		model_->deleteItem(index);
	}

	ui.listItems->scrollTo(ui.listItems->currentIndex());
	updateButtonStates();
}

/**
 * @brief DialogDrawingStyles::buttonUp_clicked
 */
void DialogDrawingStyles::buttonUp_clicked() {
	QModelIndex index = ui.listItems->currentIndex();
	if(index.isValid()) {
		model_->moveItemUp(index);
	}

	ui.listItems->scrollTo(ui.listItems->currentIndex());
	updateButtonStates();
}

/**
 * @brief DialogDrawingStyles::buttonDown_clicked
 */
void DialogDrawingStyles::buttonDown_clicked() {
	QModelIndex index = ui.listItems->currentIndex();
	if(index.isValid()) {
		model_->moveItemDown(index);
	}

	ui.listItems->scrollTo(ui.listItems->currentIndex());
	updateButtonStates();
}

/**
 * @brief DialogDrawingStyles::updateButtonStates
 */
void DialogDrawingStyles::updateButtonStates() {
	QModelIndex index = ui.listItems->currentIndex();
	updateButtonStates(index);
}

/**
 * @brief DialogDrawingStyles::updateButtonStates
 */
void DialogDrawingStyles::updateButtonStates(const QModelIndex &current) {
	if(current.isValid()) {
		if(current.row() == 0) {
			ui.buttonUp    ->setEnabled(false);
			ui.buttonDown  ->setEnabled(model_->rowCount() > 1);
			ui.buttonDelete->setEnabled(true);
			ui.buttonCopy  ->setEnabled(true);
		} else if(current.row() == model_->rowCount() - 1) {
			ui.buttonUp    ->setEnabled(true);
			ui.buttonDown  ->setEnabled(false);
			ui.buttonDelete->setEnabled(true);
			ui.buttonCopy  ->setEnabled(true);
		} else {
			ui.buttonUp    ->setEnabled(true);
			ui.buttonDown  ->setEnabled(true);
			ui.buttonDelete->setEnabled(true);
			ui.buttonCopy  ->setEnabled(true);
		}
	} else {
		ui.buttonUp    ->setEnabled(false);
		ui.buttonDown  ->setEnabled(false);
		ui.buttonDelete->setEnabled(false);
		ui.buttonCopy  ->setEnabled(false);
	}
}

/**
 * @brief PreferenceList::currentChanged
 * @param current
 * @param previous
 */
void DialogDrawingStyles::currentChanged(const QModelIndex &current, const QModelIndex &previous) {

	static bool canceled = false;

	if (canceled) {
		canceled = false;
		return;
	}

	// if we are actually switching items, check that the previous one was valid
	// so we can optionally cancel
	if(previous.isValid() && previous != deleted_ && !validateFields(Verbosity::Silent)) {
		QMessageBox messageBox(this);
		messageBox.setWindowTitle(tr("Discard Entry"));
		messageBox.setIcon(QMessageBox::Warning);
		messageBox.setText(tr("Discard incomplete entry for current highlight style?"));
		QPushButton *buttonKeep    = messageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
		QPushButton *buttonDiscard = messageBox.addButton(QMessageBox::Discard);
		Q_UNUSED(buttonDiscard)

		messageBox.exec();
		if (messageBox.clickedButton() == buttonKeep) {

			// again to cause messagebox to pop up
			validateFields(Verbosity::Verbose);

			// reselect the old item
			canceled = true;
			Q_EMIT restore(previous);
			return;
		}
	}

	// this is only safe if we aren't moving due to a delete operation
	if(previous.isValid() && previous != deleted_) {
		if(!updateCurrentItem(previous)) {
			// reselect the old item
			canceled = true;
			Q_EMIT restore(previous);
			return;
		}
	}

	// previous was OK, so let's update the contents of the dialog
	if(const auto style = model_->itemFromIndex(current)) {
		ui.editName->setText(style->name);
		ui.editColorFG->setText(style->color);
		ui.editColorBG->setText(style->bgColor);

		ui.checkBold->setChecked((style->font & Font::Bold) != 0);
		ui.checkItalic->setChecked((style->font & Font::Italic) != 0);

		// ensure that the appropriate buttons are enabled
		updateButtonStates(current);

		// don't allow deleteing the last "Plain" entry since it's reserved
		if (style->name == QLatin1String("Plain")) {
			// unless there is more than one "Plain"
			int count = countPlainEntries();
			if(count < 2) {
				ui.buttonDelete->setEnabled(false);
			}
		}
	}
}

/**
 * @brief DialogDrawingStyles::buttonBox_accepted
 */
void DialogDrawingStyles::buttonBox_accepted() {

	if (!applyDialogChanges()) {
		return;
	}

	accept();
}


/**
 * @brief DialogDrawingStyles::buttonBox_clicked
 * @param button
 */
void DialogDrawingStyles::buttonBox_clicked(QAbstractButton *button) {
	if(ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
		applyDialogChanges();
	}
}

/**
 * @brief DialogDrawingStyles::validateFields
 * @param mode
 * @return
 */
bool DialogDrawingStyles::validateFields(Verbosity verbosity) {
	if(readFields(verbosity)) {
		return true;
	}
	return false;
}

/**
 * @brief DialogDrawingStyles::readFields
 * @param mode
 * @return
 */
boost::optional<HighlightStyle> DialogDrawingStyles::readFields(Verbosity verbosity) {

	HighlightStyle hs;

	QString name = ui.editName->text().simplified();
	if (name.isNull()) {
		return boost::none;
	}

	hs.name = name;
	if (hs.name.isEmpty()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this,
								 tr("Highlight Style"),
								 tr("Please specify a name for the highlight style"));
		}

		return boost::none;
	}

	// read the color field
	QString color = ui.editColorFG->text().simplified();
	if (color.isEmpty()) {
		return boost::none;
	}

	hs.color = color;
	if (hs.color.isEmpty()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Style Color"), tr("Please specify a color for the highlight style"));
		}
		return boost::none;
	}

	// Verify that the color is a valid X color spec
	QColor rgb = X11Colors::fromString(hs.color);
	if(!rgb.isValid()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Invalid Color"), tr("Invalid X color specification: %1").arg(hs.color));
		}
		return boost::none;
	}

	// read the background color field - this may be empty
	QString bgColor = ui.editColorBG->text().simplified();
	if (bgColor.isEmpty()) {
		hs.bgColor = QString();
	} else {
		hs.bgColor = bgColor;
	}

	// Verify that the background color (if present) is a valid X color spec
	if (!hs.bgColor.isEmpty()) {
		rgb = X11Colors::fromString(hs.bgColor);
		if (!rgb.isValid()) {
			if (verbosity == Verbosity::Verbose) {
				QMessageBox::warning(this,
									 tr("Invalid Color"),
									 tr("Invalid X background color specification: %1").arg(hs.bgColor));
			}

			return boost::none;
		}
	}

	// read the font buttons
	hs.font = Font::Plain;
	if (ui.checkBold->isChecked()) {
		hs.font |= Font::Bold;
	}

	if (ui.checkItalic->isChecked()) {
		hs.font |= Font::Italic;
	}

	return hs;
}

/**
 * @brief DialogDrawingStyles::updateHSList
 * @return
 */
bool DialogDrawingStyles::applyDialogChanges() {

	if(model_->rowCount() != 0) {
		auto dialogFields = readFields(Verbosity::Verbose);
		if(!dialogFields) {
			return false;
		}

		// Get the current selected item
		QModelIndex index = ui.listItems->currentIndex();
		if(!index.isValid()) {
			return false;
		}

		// update the currently selected item's associated data
		// and make sure it has the text updated as well
		auto ptr = model_->itemFromIndex(index);
		if(ptr->name == QLatin1String("Plain") && dialogFields->name != QLatin1String("Plain")) {
			int count = countPlainEntries();
			if(count < 2) {
				QMessageBox::information(this,
										 tr("Highlight Style"),
										 tr("There must be at least one Plain entry. Cannot rename this entry."));
				return false;
			}
		}
		model_->updateItem(index, *dialogFields);
	}

	// Replace the old highlight styles list with the new one from the dialog
	std::vector<HighlightStyle> newStyles;

	for(int i = 0; i < model_->rowCount(); ++i) {
		QModelIndex index = model_->index(i, 0);
		auto style = model_->itemFromIndex(index);
		newStyles.push_back(*style);
	}

	highlightStyles_ = newStyles;

	// If a syntax highlighting dialog is up, update its menu
	if(dialogSyntaxPatterns_) {
		dialogSyntaxPatterns_->updateHighlightStyleMenu();
	}

	// Redisplay highlighted windows which use changed style(s)
	for(DocumentWidget *document : DocumentWidget::allDocuments()) {
		document->updateHighlightStyles();
	}

	// Note that preferences have been changed
	Preferences::MarkPrefsChanged();
	return true;
}

/**
 * @brief DialogDrawingStyles::updateCurrentItem
 * @param item
 * @return
 */
bool DialogDrawingStyles::updateCurrentItem(const QModelIndex &index) {
	// Get the current contents of the "patterns" dialog fields
	auto dialogFields = readFields(Verbosity::Verbose);
	if(!dialogFields) {
		return false;
	}

	// Get the current contents of the dialog fields
	if(!index.isValid()) {
		return false;
	}

	// update the currently selected item's associated data
	// and make sure it has the text updated as well. Disallow renaming
	// the last "Plain" entry though
	auto ptr = model_->itemFromIndex(index);
	if(ptr->name == QLatin1String("Plain") && dialogFields->name != QLatin1String("Plain")) {
		int count = countPlainEntries();
		if(count < 2) {
			QMessageBox::information(this, tr("Highlight Style"), tr("There must be at least one Plain entry. Cannot rename this entry."));
			return false;
		}
	}

	model_->updateItem(index, *dialogFields);
	return true;
}

/**
 * @brief DialogDrawingStyles::updateCurrentItem
 * @return
 */
bool DialogDrawingStyles::updateCurrentItem() {
	QModelIndex index = ui.listItems->currentIndex();
	if(index.isValid()) {
		return updateCurrentItem(index);
	}

	return true;
}

/**
 * @brief DialogDrawingStyles::countPlainEntries
 * @return
 */
int DialogDrawingStyles::countPlainEntries() const {
	int count = 0;
	for(int i = 0; i < model_->rowCount(); ++i) {
		QModelIndex index = model_->index(i, 0);
		auto style = model_->itemFromIndex(index);
		if(style->name == QLatin1String("Plain")) {
			++count;
		}
	}
	return count;
}

/**
 * @brief DialogColors::chooseColor
 * @param edit
 */
void DialogDrawingStyles::chooseColor(QLineEdit *edit) {

	QString name = edit->text();

	QColor color = QColorDialog::getColor(X11Colors::fromString(name), this);
	if(color.isValid()) {
		edit->setText(tr("#%1").arg((color.rgb() & 0x00ffffff), 6, 16, QLatin1Char('0')));
	}
}

/**
 * @brief DialogDrawingStyles::buttonForeground_clicked
 */
void DialogDrawingStyles::buttonForeground_clicked() {
	chooseColor(ui.editColorFG);
}

/**
 * @brief DialogDrawingStyles::buttonBackground_clicked
 */
void DialogDrawingStyles::buttonBackground_clicked() {
	chooseColor(ui.editColorBG);
}
