
#include "DialogDrawingStyles.h"
#include "CommonDialog.h"
#include "DialogSyntaxPatterns.h"
#include "DocumentWidget.h"
#include "Highlight.h"
#include "HighlightStyleModel.h"
#include "Preferences.h"
#include "X11Colors.h"

#include <QColorDialog>
#include <QMessageBox>
#include <QRegularExpressionValidator>

/**
 * @brief Constructor for DialogDrawingStyles class
 *
 * @param dialogSyntaxPatterns The DialogSyntaxPatterns instance
 * @param highlightStyles Reference to the vector of HighlightStyle objects
 *                        that will be modified by this dialog.
 * @param parent The parent widget, defaults to nullptr
 * @param f The window flags, defaults to Qt::WindowFlags()
 */
DialogDrawingStyles::DialogDrawingStyles(DialogSyntaxPatterns *dialogSyntaxPatterns, std::vector<HighlightStyle> &highlightStyles, QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f), highlightStyles_(highlightStyles), dialogSyntaxPatterns_(dialogSyntaxPatterns) {

	ui.setupUi(this);
	connectSlots();

	CommonDialog::SetButtonIcons(&ui);

	model_ = new HighlightStyleModel(this);
	ui.listItems->setModel(model_);

	// Copy the list of highlight style information to one that the user can freely edit
	for (const HighlightStyle &style : highlightStyles) {
		model_->addItem(style);
	}

	connect(ui.listItems->selectionModel(), &QItemSelectionModel::currentChanged, this, &DialogDrawingStyles::currentChanged, Qt::QueuedConnection);
	connect(this, &DialogDrawingStyles::restore, ui.listItems, &QListView::setCurrentIndex, Qt::QueuedConnection);

	// default to selecting the first item
	if (model_->rowCount() != 0) {
		const QModelIndex index = model_->index(0, 0);
		ui.listItems->setCurrentIndex(index);
	}

	// Valid characters are letters, numbers, _, -, +, $, #, and internal whitespace.
	static const QRegularExpression rx(QLatin1String("[\\sA-Za-z0-9_+$#-]+"));
	auto validator = new QRegularExpressionValidator(rx, this);
	ui.editName->setValidator(validator);
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
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
 * @brief Sets the current style by name in the dialog.
 *
 * @param name The name of the style to set as current.
 */
void DialogDrawingStyles::setStyleByName(const QString &name) {

	for (int i = 0; i < model_->rowCount(); ++i) {
		const QModelIndex index = model_->index(i, 0);
		auto ptr                = model_->itemFromIndex(index);
		if (ptr->name == name) {
			ui.listItems->setCurrentIndex(index);
			break;
		}
	}

	ui.listItems->scrollTo(ui.listItems->currentIndex());
	CommonDialog::UpdateButtonStates(&ui, model_);
}

/**
 * @brief Handler for the "New" button click event.
 */
void DialogDrawingStyles::buttonNew_clicked() {

	if (!updateCurrentItem()) {
		return;
	}

	CommonDialog::AddNewItem(&ui, model_, []() {
		HighlightStyle style;
		// some sensible defaults...
		style.name  = tr("New Item");
		style.color = tr("black");
		return style;
	});
}

/**
 * @brief Handler for the "Copy" button click event.
 */
void DialogDrawingStyles::buttonCopy_clicked() {

	if (!updateCurrentItem()) {
		return;
	}

	CommonDialog::CopyItem(&ui, model_);
}

/**
 * @brief Handler for the "Delete" button click event.
 */
void DialogDrawingStyles::buttonDelete_clicked() {
	CommonDialog::DeleteItem(&ui, model_, &deleted_);
}

/**
 * @brief Handler for the "Up" button click event.
 */
void DialogDrawingStyles::buttonUp_clicked() {
	CommonDialog::MoveItemUp(&ui, model_);
}

/**
 * @brief Handler for the "Down" button click event.
 */
void DialogDrawingStyles::buttonDown_clicked() {
	CommonDialog::MoveItemDown(&ui, model_);
}

/**
 * @brief Handler for the change of the current item in the list view.
 *
 * @param current The currently selected item index.
 * @param previous The previously selected item index.
 */
void DialogDrawingStyles::currentChanged(const QModelIndex &current, const QModelIndex &previous) {

	static bool canceled = false;
	bool skip_check      = false;

	if (canceled) {
		canceled = false;
		return;
	}

	// if we are actually switching items, check that the previous one was valid
	// so we can optionally cancel
	if (previous.isValid() && previous != deleted_ && !validateFields(Verbosity::Silent)) {
		QMessageBox messageBox(this);
		messageBox.setWindowTitle(tr("Discard Entry"));
		messageBox.setIcon(QMessageBox::Warning);
		messageBox.setText(tr("Discard incomplete entry for current highlight style?"));
		QPushButton *buttonKeep    = messageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
		QPushButton *buttonDiscard = messageBox.addButton(QMessageBox::Discard);

		messageBox.exec();
		if (messageBox.clickedButton() == buttonKeep) {

			// again to cause message box to pop up
			validateFields(Verbosity::Verbose);

			// reselect the old item
			canceled = true;
			Q_EMIT restore(previous);
			return;
		}

		if (messageBox.clickedButton() == buttonDiscard) {
			model_->deleteItem(previous);
			skip_check = true;
		}
	}

	// this is only safe if we aren't moving due to a delete operation
	if (previous.isValid() && previous != deleted_ && !skip_check) {
		if (!updateItem(previous)) {
			// reselect the old item
			canceled = true;
			Q_EMIT restore(previous);
			return;
		}
	}

	// previous was OK, so let's update the contents of the dialog
	if (const auto style = model_->itemFromIndex(current)) {
		ui.editName->setText(style->name);
		ui.editColorFG->setText(style->color);
		ui.editColorBG->setText(style->bgColor);

		ui.checkBold->setChecked((style->font & Font::Bold) != 0);
		ui.checkItalic->setChecked((style->font & Font::Italic) != 0);

		// ensure that the appropriate buttons are enabled
		CommonDialog::UpdateButtonStates(&ui, model_, current);

		// don't allow deleting the last "Plain" entry since it's reserved
		if (style->name == QLatin1String("Plain")) {
			// unless there is more than one "Plain"
			const int count = countPlainEntries();
			if (count < 2) {
				ui.buttonDelete->setEnabled(false);
			}
		}
	}
}

/**
 * @brief Handler for the "Accepted" button click event.
 */
void DialogDrawingStyles::buttonBox_accepted() {

	if (!applyDialogChanges()) {
		return;
	}

	accept();
}

/**
 * @brief Handler for the button box click event.
 * Handles the Apply button click to apply changes without closing the dialog.
 *
 * @param button The button that was clicked in the button box.
 */
void DialogDrawingStyles::buttonBox_clicked(QAbstractButton *button) {
	if (ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
		applyDialogChanges();
	}
}

/**
 * @brief Validates the fields in the dialog.
 *
 * @param verbosity The verbosity level for error messages.
 * @return `true` if the fields are valid, `false` otherwise.
 */
bool DialogDrawingStyles::validateFields(Verbosity verbosity) {
	if (readFields(verbosity)) {
		return true;
	}
	return false;
}

/**
 * @brief Reads the fields from the dialog and returns a HighlightStyle object.
 *
 * @param verbosity The verbosity level for error messages.
 * @return A filled out HighlightStyle object if successful, or an empty optional if validation fails.
 */
std::optional<HighlightStyle> DialogDrawingStyles::readFields(Verbosity verbosity) {

	HighlightStyle hs;

	const QString name = ui.editName->text().simplified();
	if (name.isNull()) {
		return {};
	}

	hs.name = name;
	if (hs.name.isEmpty()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this,
								 tr("Highlight Style"),
								 tr("Please specify a name for the highlight style"));
		}

		return {};
	}

	// read the color field
	const QString color = ui.editColorFG->text().simplified();
	if (color.isEmpty()) {
		return {};
	}

	hs.color = color;
	if (hs.color.isEmpty()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Style Color"), tr("Please specify a color for the highlight style"));
		}
		return {};
	}

	// Verify that the color is a valid X color spec
	QColor rgb = X11Colors::FromString(hs.color);
	if (!rgb.isValid()) {
		if (verbosity == Verbosity::Verbose) {
			QMessageBox::warning(this, tr("Invalid Color"), tr("Invalid X color specification: %1").arg(hs.color));
		}
		return {};
	}

	// read the background color field - this may be empty
	const QString bgColor = ui.editColorBG->text().simplified();
	if (bgColor.isEmpty()) {
		hs.bgColor = QString();
	} else {
		hs.bgColor = bgColor;
	}

	// Verify that the background color (if present) is a valid X color spec
	if (!hs.bgColor.isEmpty()) {
		rgb = X11Colors::FromString(hs.bgColor);
		if (!rgb.isValid()) {
			if (verbosity == Verbosity::Verbose) {
				QMessageBox::warning(this,
									 tr("Invalid Color"),
									 tr("Invalid X background color specification: %1").arg(hs.bgColor));
			}

			return {};
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
 * @brief Applies the changes made in the dialog to the highlight styles.
 *
 * @return `true` if the changes were successfully applied, `false` otherwise.
 */
bool DialogDrawingStyles::applyDialogChanges() {

	if (model_->rowCount() != 0) {
		auto dialogFields = readFields(Verbosity::Verbose);
		if (!dialogFields) {
			return false;
		}

		// Get the current selected item
		const QModelIndex index = ui.listItems->currentIndex();
		if (!index.isValid()) {
			return false;
		}

		// update the currently selected item's associated data
		// and make sure it has the text updated as well
		auto ptr = model_->itemFromIndex(index);
		if (ptr->name == QLatin1String("Plain") && dialogFields->name != QLatin1String("Plain")) {
			const int count = countPlainEntries();
			if (count < 2) {
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

	for (int i = 0; i < model_->rowCount(); ++i) {
		const QModelIndex index = model_->index(i, 0);
		auto style              = model_->itemFromIndex(index);
		newStyles.push_back(*style);
	}

	highlightStyles_ = newStyles;

	// If a syntax highlighting dialog is up, update its menu
	if (dialogSyntaxPatterns_) {
		dialogSyntaxPatterns_->updateHighlightStyleMenu();
	}

	// Redisplay highlighted windows which use changed style(s)
	for (DocumentWidget *document : DocumentWidget::allDocuments()) {
		document->updateHighlightStyles();
	}

	// Note that preferences have been changed
	Preferences::MarkPrefsChanged();
	return true;
}

/**
 * @brief Updates an item in the dialog with the current field values.
 *
 * @param index The item to update, specified by its index in the model.
 * @return `true` if the item was successfully updated, `false` otherwise.
 */
bool DialogDrawingStyles::updateItem(const QModelIndex &index) {
	// Get the current contents of the "patterns" dialog fields
	auto dialogFields = readFields(Verbosity::Verbose);
	if (!dialogFields) {
		return false;
	}

	// Get the current contents of the dialog fields
	if (!index.isValid()) {
		return false;
	}

	// update the item's associated data
	// and make sure it has the text updated as well. Disallow renaming
	// the last "Plain" entry though
	auto ptr = model_->itemFromIndex(index);
	if (ptr->name == QLatin1String("Plain") && dialogFields->name != QLatin1String("Plain")) {
		const int count = countPlainEntries();
		if (count < 2) {
			QMessageBox::information(this, tr("Highlight Style"), tr("There must be at least one Plain entry. Cannot rename this entry."));
			return false;
		}
	}

	model_->updateItem(index, *dialogFields);
	return true;
}

/**
 * @brief Updates the currently selected item in the dialog with the current field values.
 *
 * @return `true` if the item was successfully updated, `false` otherwise.
 */
bool DialogDrawingStyles::updateCurrentItem() {
	const QModelIndex index = ui.listItems->currentIndex();
	if (index.isValid()) {
		return updateItem(index);
	}

	return true;
}

/**
 * @brief Counts the number of "Plain" entries in the model.
 *
 * @return The count of "Plain" entries in the model.
 */
int DialogDrawingStyles::countPlainEntries() const {
	int count = 0;
	for (int i = 0; i < model_->rowCount(); ++i) {
		const QModelIndex index = model_->index(i, 0);
		auto style              = model_->itemFromIndex(index);
		if (style->name == QLatin1String("Plain")) {
			++count;
		}
	}
	return count;
}

/**
 * @brief Opens a color dialog to choose a color for the specified QLineEdit.
 *
 * @param edit The QLineEdit to update with the chosen color.
 */
void DialogDrawingStyles::chooseColor(QLineEdit *edit) {

	const QString name = edit->text();

	const QColor color = QColorDialog::getColor(X11Colors::FromString(name), this);
	if (color.isValid()) {
		edit->setText(QStringLiteral("#%1").arg((color.rgb() & 0x00ffffff), 6, 16, QLatin1Char('0')));
	}
}

/**
 * @brief Handler for the "Foreground" button click event.
 */
void DialogDrawingStyles::buttonForeground_clicked() {
	chooseColor(ui.editColorFG);
}

/**
 * @brief Handler for the "Background" button click event.
 */
void DialogDrawingStyles::buttonBackground_clicked() {
	chooseColor(ui.editColorBG);
}
