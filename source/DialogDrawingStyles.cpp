
#include "DialogDrawingStyles.h"
#include "DocumentWidget.h"
#include "FontType.h"
#include "highlight.h"
#include "HighlightStyleModel.h"
#include "MainWindow.h"
#include "preferences.h"
#include "X11Colors.h"

#include <QMessageBox>

/**
 * @brief DialogDrawingStyles::DialogDrawingStyles
 * @param parent
 * @param f
 */
DialogDrawingStyles::DialogDrawingStyles(std::vector<HighlightStyle> &highlightStyles, QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) , highlightStyles_(highlightStyles) {
	ui.setupUi(this);

    model_ = new HighlightStyleModel(this);
    ui.listItems->setModel(model_);

	// Copy the list of highlight style information to one that the user can freely edit
    for(const HighlightStyle &style : highlightStyles) {
        model_->addItem(style);
	}

    connect(ui.listItems->selectionModel(), &QItemSelectionModel::currentChanged, this, &DialogDrawingStyles::currentChanged, Qt::QueuedConnection);
    connect(this, &DialogDrawingStyles::restore, this, &DialogDrawingStyles::restoreSlot, Qt::QueuedConnection);

    // default to selecting the first item
    if(model_->rowCount() != 0) {
        QModelIndex index = model_->index(0, 0);
        ui.listItems->setCurrentIndex(index);
    }

	// Valid characters are letters, numbers, _, -, +, $, #, and internal whitespace.
	auto validator = new QRegExpValidator(QRegExp(QLatin1String("[\\sA-Za-z0-9_+$#-]+")), this);
	ui.editName->setValidator(validator);
}

/**
 * @brief DialogDrawingStyles::restoreSlot
 * @param index
 */
void DialogDrawingStyles::restoreSlot(const QModelIndex &index) {
    ui.listItems->setCurrentIndex(index);
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
 * @brief DialogDrawingStyles::on_buttonNew_clicked
 */
void DialogDrawingStyles::on_buttonNew_clicked() {

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
 * @brief DialogDrawingStyles::on_buttonCopy_clicked
 */
void DialogDrawingStyles::on_buttonCopy_clicked() {

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
 * @brief DialogDrawingStyles::on_buttonDelete_clicked
 */
void DialogDrawingStyles::on_buttonDelete_clicked() {

    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        deleted_ = index;
        model_->deleteItem(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogDrawingStyles::on_buttonUp_clicked
 */
void DialogDrawingStyles::on_buttonUp_clicked() {
    QModelIndex index = ui.listItems->currentIndex();
    if(index.isValid()) {
        model_->moveItemUp(index);
    }

    ui.listItems->scrollTo(ui.listItems->currentIndex());
    updateButtonStates();
}

/**
 * @brief DialogDrawingStyles::on_buttonDown_clicked
 */
void DialogDrawingStyles::on_buttonDown_clicked() {
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
    if(previous.isValid() && previous != deleted_ && !checkCurrent(Mode::Silent)) {
        QMessageBox messageBox(this);
        messageBox.setWindowTitle(tr("Discard Entry"));
        messageBox.setIcon(QMessageBox::Warning);
        messageBox.setText(tr("Discard incomplete entry for current highlight style?"));
        QPushButton *buttonKeep    = messageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
        QPushButton *buttonDiscard = messageBox.addButton(QMessageBox::Discard);
        Q_UNUSED(buttonDiscard);

        messageBox.exec();
        if (messageBox.clickedButton() == buttonKeep) {

            // again to cause messagebox to pop up
            checkCurrent(Mode::Verbose);

            // reselect the old item
            canceled = true;
            Q_EMIT restore(previous);
            return;
        }
    }

    // NOTE(eteran): this is only safe if we aren't moving due to a delete operation
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

        switch(style->font) {
        case PLAIN_FONT:
            ui.radioPlain->setChecked(true);
            break;
        case BOLD_FONT:
            ui.radioBold->setChecked(true);
            break;
        case ITALIC_FONT:
            ui.radioItalic->setChecked(true);
            break;
        case BOLD_ITALIC_FONT:
            ui.radioBoldItalic->setChecked(true);
            break;
        }

        // ensure that the appropriate buttons are enabled
        updateButtonStates(current);

        // don't allow deleteing the last "Plain" entry since it's reserved
        if (style->name == tr("Plain")) {
            // unless there is more than one "Plain"
            int count = countPlainEntries();
            if(count < 2) {
                ui.buttonDelete->setEnabled(false);
            }
        }
    }
}

/**
 * @brief DialogDrawingStyles::on_buttonBox_accepted
 */
void DialogDrawingStyles::on_buttonBox_accepted() {

    if (!applyDialogChanges()) {
		return;
	}
	
	accept();
}


/**
 * @brief DialogDrawingStyles::on_buttonBox_clicked
 * @param button
 */
void DialogDrawingStyles::on_buttonBox_clicked(QAbstractButton *button) {
	if(ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
        applyDialogChanges();
	}
}

/**
 * @brief DialogDrawingStyles::checkCurrent
 * @param mode
 * @return
 */
bool DialogDrawingStyles::checkCurrent(Mode mode) {
    if(auto ptr = readDialogFields(mode)) {
		return true;
	}
	return false;
}

/**
 * @brief DialogDrawingStyles::readDialogFields
 * @param mode
 * @return
 */
std::unique_ptr<HighlightStyle> DialogDrawingStyles::readDialogFields(Mode mode) {

   	// Allocate a language mode structure to return 
    auto hs = std::make_unique<HighlightStyle>();

    // read the name field 
	QString name = ui.editName->text().simplified();
    if (name.isNull()) {
    	return nullptr;
    }

	hs->name = name;
    if (hs->name.isEmpty()) {
        if (mode == Mode::Verbose) {
            QMessageBox::warning(this, tr("Highlight Style"), tr("Please specify a name for the highlight style"));
        }

        return nullptr;
    }

    // read the color field 
    QString color = ui.editColorFG->text().simplified();
    if (color.isEmpty()) {
    	return nullptr;
    }

	hs->color = color;
    if (hs->color.isEmpty()) {
        if (mode == Mode::Verbose) {
            QMessageBox::warning(this, tr("Style Color"), tr("Please specify a color for the highlight style"));
        }
        return nullptr;
    }

    // Verify that the color is a valid X color spec
    QColor rgb = X11Colors::fromString(hs->color);
    if(!rgb.isValid()) {
        if (mode == Mode::Verbose) {
            QMessageBox::warning(this, tr("Invalid Color"), tr("Invalid X color specification: %1").arg(hs->color));
        }
        return nullptr;
    }

    // read the background color field - this may be empty
	QString bgColor = ui.editColorBG->text().simplified();
    if (bgColor.isEmpty()) {
        hs->bgColor = QString();
    } else {
		hs->bgColor = bgColor;
	}

    // Verify that the background color (if present) is a valid X color spec 
    if (!hs->bgColor.isEmpty()) {
        rgb = X11Colors::fromString(hs->bgColor);
        if (!rgb.isValid()) {
            if (mode == Mode::Verbose) {
                QMessageBox::warning(this, tr("Invalid Color"), tr("Invalid X background color specification: %1").arg(hs->bgColor));
            }

            return nullptr;;
        }
    }

    // read the font buttons 
    if (ui.radioBold->isChecked()) {
    	hs->font = BOLD_FONT;
    } else if (ui.radioItalic->isChecked()) {
    	hs->font = ITALIC_FONT;
    } else if (ui.radioBoldItalic->isChecked()) {
    	hs->font = BOLD_ITALIC_FONT;
    } else {
    	hs->font = PLAIN_FONT;
	}

    return hs;
}

/**
 * @brief DialogDrawingStyles::updateHSList
 * @return
 */
bool DialogDrawingStyles::applyDialogChanges() {

    if(model_->rowCount() != 0) {
        auto dialogFields = readDialogFields(Mode::Verbose);
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
        if(ptr->name == tr("Plain") && dialogFields->name != tr("Plain")) {
            int count = countPlainEntries();
            if(count < 2) {
                QMessageBox::information(this,
                                         tr("Highlight Style"),
                                         tr("There must be at least one Plain entry. Cannot rename this entry."));
                return false;
            }
        }
        *ptr = *dialogFields;
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
    MainWindow::updateHighlightStyleMenu();

	// Redisplay highlighted windows which use changed style(s) 
    for(DocumentWidget *document : DocumentWidget::allDocuments()) {
        document->UpdateHighlightStylesEx();
	}

	// Note that preferences have been changed 
	MarkPrefsChanged();
	return true;
}

/**
 * @brief DialogDrawingStyles::updateCurrentItem
 * @param item
 * @return
 */
bool DialogDrawingStyles::updateCurrentItem(const QModelIndex &index) {
	// Get the current contents of the "patterns" dialog fields 
    auto dialogFields = readDialogFields(Mode::Verbose);
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
    if(ptr->name == tr("Plain") && dialogFields->name != tr("Plain")) {
        int count = countPlainEntries();
        if(count < 2) {
            QMessageBox::information(this, tr("Highlight Style"), tr("There must be at least one Plain entry. Cannot rename this entry."));
            return false;
        }
    }


    *ptr = *dialogFields;
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
        if(style->name == tr("Plain")) {
            ++count;
        }
    }
    return count;
}
