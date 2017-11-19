
#include "DialogDrawingStyles.h"
#include "DocumentWidget.h"
#include "FontType.h"
#include "HighlightStyle.h"
#include "MainWindow.h"
#include "preferences.h"
#include "SignalBlocker.h"
#include "X11Colors.h"
#include "highlightData.h"
#include <QMessageBox>
#include <QRegExp>
#include <QRegExpValidator>
#include <QtDebug>

//------------------------------------------------------------------------------
// Name: DialogDrawingStyles
//------------------------------------------------------------------------------
DialogDrawingStyles::DialogDrawingStyles(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f), previous_(nullptr) {
	ui.setupUi(this);

	// Copy the list of highlight style information to one that the user can freely edit
    Q_FOREACH(const HighlightStyle &style, HighlightStyles) {
        auto ptr  = new HighlightStyle(style);
		auto item = new QListWidgetItem(ptr->name);
		item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr));
		ui.listItems->addItem(item);
	}

	if(ui.listItems->count() != 0) {
		ui.listItems->setCurrentRow(0);
	}
	
	// Valid characters are letters, numbers, _, -, +, $, #, and internal whitespace.
	auto validator = new QRegExpValidator(QRegExp(QLatin1String("[\\sA-Za-z0-9_+$#-]+")), this);
	
	ui.editName->setValidator(validator);
}

//------------------------------------------------------------------------------
// Name: ~DialogDrawingStyles
//------------------------------------------------------------------------------
DialogDrawingStyles::~DialogDrawingStyles() noexcept {

	for(int i = 0; i < ui.listItems->count(); ++i) {
	    delete itemFromIndex(i);
	}
}

//------------------------------------------------------------------------------
// Name: itemFromIndex
//------------------------------------------------------------------------------
HighlightStyle *DialogDrawingStyles::itemFromIndex(int i) const {
	if(i < ui.listItems->count()) {
	    QListWidgetItem* item = ui.listItems->item(i);
		auto ptr = reinterpret_cast<HighlightStyle *>(item->data(Qt::UserRole).toULongLong());
		return ptr;
	}
	
	return nullptr;
}

//------------------------------------------------------------------------------
// Name: setStyleByName
//------------------------------------------------------------------------------
void DialogDrawingStyles::setStyleByName(const QString &name) {

	QList<QListWidgetItem *> items = ui.listItems->findItems(name, Qt::MatchFixedString);
	if(items.size() != 1) {
		return;
	}
	
	ui.listItems->setCurrentItem(items[0]);
}

//------------------------------------------------------------------------------
// Name: on_buttonNew_clicked
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_buttonNew_clicked() {

	if(!updateCurrentItem()) {
		return;
	}

	auto ptr  = new HighlightStyle;
	ptr->name = tr("New Item");

	auto item = new QListWidgetItem(ptr->name);
	item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr));
	ui.listItems->addItem(item);
	ui.listItems->setCurrentItem(item);
}

//------------------------------------------------------------------------------
// Name: on_buttonCopy_clicked
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_buttonCopy_clicked() {

	if(!updateCurrentItem()) {
		return;
	}

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	auto ptr = reinterpret_cast<HighlightStyle *>(selection->data(Qt::UserRole).toULongLong());
	auto newPtr = new HighlightStyle(*ptr);
	auto newItem = new QListWidgetItem(newPtr->name);
	newItem->setData(Qt::UserRole, reinterpret_cast<qulonglong>(newPtr));

	const int i = ui.listItems->row(selection);
	ui.listItems->insertItem(i + 1, newItem);
	ui.listItems->setCurrentItem(newItem);
}

//------------------------------------------------------------------------------
// Name: on_buttonDelete_clicked
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_buttonDelete_clicked() {

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return;
	}
	
	// prevent usage of this item going forward
	previous_ = nullptr;

	QListWidgetItem *const selection = selections[0];
	auto ptr = reinterpret_cast<HighlightStyle *>(selection->data(Qt::UserRole).toULongLong());

	delete ptr;
	delete selection;
	
	
	// force an update of the display
	on_listItems_itemSelectionChanged();
}

//------------------------------------------------------------------------------
// Name: on_buttonUp_clicked
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_buttonUp_clicked() {

    QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];

	const int i = ui.listItems->row(selection);
	if(i != 0) {
		QListWidgetItem *item = ui.listItems->takeItem(i);
		ui.listItems->insertItem(i - 1, item);
		ui.listItems->scrollToItem(item);
		item->setSelected(true);
	}
}

//------------------------------------------------------------------------------
// Name: on_buttonDown_clicked
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_buttonDown_clicked() {

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];

	const int i = ui.listItems->row(selection);
	if(i != ui.listItems->count() - 1) {
		QListWidgetItem *item = ui.listItems->takeItem(i);
		ui.listItems->insertItem(i + 1, item);
		ui.listItems->scrollToItem(item);
		item->setSelected(true);
	}
}

//------------------------------------------------------------------------------
// Name: on_listItems_itemSelectionChanged
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_listItems_itemSelectionChanged() {

	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		previous_ = nullptr;
		return;
	}

	QListWidgetItem *const current = selections[0];

	if(previous_ != nullptr && current != nullptr && current != previous_) {
		// we want to try to save it (but not apply it yet)
		// and then move on
        if(!checkCurrent(Mode::Silent)) {

			QMessageBox messageBox(this);
			messageBox.setWindowTitle(tr("Discard Entry"));
			messageBox.setIcon(QMessageBox::Warning);
			messageBox.setText(tr("Discard incomplete entry for current highlight style?"));
			QPushButton *buttonKeep    = messageBox.addButton(tr("Keep"), QMessageBox::RejectRole);
			QPushButton *buttonDiscard = messageBox.addButton(tr("Discard"), QMessageBox::AcceptRole);
			Q_UNUSED(buttonDiscard);

			messageBox.exec();
			if (messageBox.clickedButton() == buttonKeep) {
			
				// again to cause messagebox to pop up
                checkCurrent(Mode::Verbose);
				
				// reselect the old item
                no_signals(ui.listItems)->setCurrentItem(previous_);
				return;
			}

			// if we get here, we are ditching changes
		} else {
			if(!updateCurrentItem(previous_)) {
				return;
			}
		}
	}

	if(current) {
		const int i = ui.listItems->row(current);

		auto style = reinterpret_cast<HighlightStyle *>(current->data(Qt::UserRole).toULongLong());

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


		if(i == 0) {
			ui.buttonUp    ->setEnabled(false);
			ui.buttonDown  ->setEnabled(ui.listItems->count() > 1);
			ui.buttonDelete->setEnabled(true);
			ui.buttonCopy  ->setEnabled(true);
		} else if(i == ui.listItems->count() - 1) {
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
		
		
		// don't allow deleteing the last "Plain" entry
		// since it's reserved
		if (style->name == tr("Plain")) {
			QList<QListWidgetItem *> plainItems = ui.listItems->findItems(tr("Plain"), Qt::MatchFixedString);
			if(plainItems.size() < 2) {
				ui.buttonDelete->setEnabled(false);
			}
		}
		
	}
	
	previous_ = current;
}

//------------------------------------------------------------------------------
// Name: on_buttonBox_accepted
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_buttonBox_accepted() {

	if (!updateHSList()) {
		return;
	}
	
	accept();
}


//------------------------------------------------------------------------------
// Name: on_buttonBox_clicked
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_buttonBox_clicked(QAbstractButton *button) {
	if(ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {
		updateHSList();
	}
}

//------------------------------------------------------------------------------
// Name: checkCurrent
//------------------------------------------------------------------------------
bool DialogDrawingStyles::checkCurrent(Mode mode) {
    if(auto ptr = readDialogFields(mode)) {
		return true;
	}
	return false;
}

//------------------------------------------------------------------------------
// Name: readDialogFields
//------------------------------------------------------------------------------
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
    if (color.isNull()) {
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
    if (!bgColor.isNull() && bgColor.isEmpty()) {
        hs->bgColor = QString();
    } else {
		hs->bgColor = bgColor;
	}

    // Verify that the background color (if present) is a valid X color spec 
    if (!hs->bgColor.isEmpty()) {
        rgb = X11Colors::fromString(hs->color);
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


/*
** Apply the changes made in the highlight styles dialog to the stored
** highlight style information in HighlightStyles
*/
bool DialogDrawingStyles::updateHSList() {

	// Test compile the macro
    auto current = readDialogFields(Mode::Verbose);
	if(!current) {
		return false;
	}

	// Get the current contents of the dialog fields
	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return false;
	}

	// update the currently selected item's associated data
	// and make sure it has the text updated as well
	QListWidgetItem *const selection = selections[0];
	auto ptr = reinterpret_cast<HighlightStyle *>(selection->data(Qt::UserRole).toULongLong());
	delete ptr;

    selection->setText(current->name);
    selection->setData(Qt::UserRole, reinterpret_cast<qulonglong>(current.release()));

	// Replace the old highlight styles list with the new one from the dialog 
	HighlightStyles.clear();
	
	for(int i = 0; i < ui.listItems->count(); ++i) {
		auto ptr = itemFromIndex(i);
        HighlightStyles.push_back(*ptr);
	}

	// If a syntax highlighting dialog is up, update its menu 
    MainWindow::updateHighlightStyleMenu();

	// Redisplay highlighted windows which use changed style(s) 
    Q_FOREACH(DocumentWidget *document, DocumentWidget::allDocuments()) {
        document->UpdateHighlightStylesEx();
	}

	// Note that preferences have been changed 
	MarkPrefsChanged();

	return true;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
bool DialogDrawingStyles::updateCurrentItem(QListWidgetItem *item) {
	// Get the current contents of the "patterns" dialog fields 
    auto ptr = readDialogFields(Mode::Verbose);
	if(!ptr) {
		return false;
	}
	
	// delete the current pattern in this slot
	auto old = reinterpret_cast<HighlightStyle *>(item->data(Qt::UserRole).toULongLong());
	delete old;
	
    item->setText(ptr->name);
    item->setData(Qt::UserRole, reinterpret_cast<qulonglong>(ptr.release()));
	return true;
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
bool DialogDrawingStyles::updateCurrentItem() {
	QList<QListWidgetItem *> selections = ui.listItems->selectedItems();
	if(selections.size() != 1) {
		return false;
	}

	QListWidgetItem *const selection = selections[0];
	return updateCurrentItem(selection);	
}

