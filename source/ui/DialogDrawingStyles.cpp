
#include <QtDebug>
#include "DialogDrawingStyles.h"
#include "HighlightStyle.h"
#include "highlightData.h"
#include "preferences.h"
#include "Document.h"

//------------------------------------------------------------------------------
// Name: DialogDrawingStyles
//------------------------------------------------------------------------------
DialogDrawingStyles::DialogDrawingStyles(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
	
	// Copy the list of highlight style information to one that the user can freely edit
	for (HighlightStyle *style: HighlightStyles) {
		styles_.push_back(new HighlightStyle(*style));
	}

	ui.listStyles->addItem(tr("New"));
	for(HighlightStyle *style : styles_) {
		ui.listStyles->addItem(QString::fromStdString(style->name));
	}
	ui.listStyles->setCurrentRow(0);
}

//------------------------------------------------------------------------------
// Name: ~DialogDrawingStyles
//------------------------------------------------------------------------------
DialogDrawingStyles::~DialogDrawingStyles() {
	qDeleteAll(styles_);
}

//------------------------------------------------------------------------------
// Name: setStyleByName
//------------------------------------------------------------------------------
void DialogDrawingStyles::setStyleByName(const QString &name) {

	qDebug() << "HERE: " << name;

	QList<QListWidgetItem *> items = ui.listStyles->findItems(name, Qt::MatchFixedString);
	if(items.size() != 1) {
		return;
	}
	
	items[0]->setSelected(true);
}

//------------------------------------------------------------------------------
// Name: on_buttonCopy_clicked
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_buttonCopy_clicked() {
}

//------------------------------------------------------------------------------
// Name: on_buttonDelete_clicked
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_buttonDelete_clicked() {

}

//------------------------------------------------------------------------------
// Name: on_buttonUp_clicked
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_buttonUp_clicked() {
	QList<QListWidgetItem *> selections = ui.listStyles->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];

	const int i = ui.listStyles->row(selection) - 1;
	if(i != 0) {
		styles_.move(i, i - 1);
		
		// offset by 1 because of "New" at the top
		QListWidgetItem *item = ui.listStyles->takeItem(i + 1);
		ui.listStyles->insertItem(i, item);
		ui.listStyles->scrollToItem(item);
		item->setSelected(true);
	}
}

//------------------------------------------------------------------------------
// Name: on_buttonDown_clicked
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_buttonDown_clicked() {
	QList<QListWidgetItem *> selections = ui.listStyles->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];

	const int i = ui.listStyles->row(selection) - 1;
	if(i != styles_.size() - 1) {
		styles_.move(i, i + 1);
		
		// offset by 1 because of "New" at the top
		QListWidgetItem *item = ui.listStyles->takeItem(i + 1);
		ui.listStyles->insertItem(i + 2, item);
		ui.listStyles->scrollToItem(item);
		item->setSelected(true);
	}
}

//------------------------------------------------------------------------------
// Name: on_listStyles_itemSelectionChanged
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_listStyles_itemSelectionChanged() {

	QList<QListWidgetItem *> selections = ui.listStyles->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	styleName_ = selection->text();

	if(styleName_ == tr("New")) {
		ui.buttonUp    ->setEnabled(false);
		ui.buttonDown  ->setEnabled(false);
		ui.buttonDelete->setEnabled(false);
		ui.buttonCopy  ->setEnabled(false);
	} else {
		const int i = ui.listStyles->row(selection) - 1;

		HighlightStyle *style = styles_[i];
		
		ui.editName->setText(QString::fromStdString(style->name));
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
			ui.buttonDown  ->setEnabled(true);
			ui.buttonDelete->setEnabled(true);
			ui.buttonCopy  ->setEnabled(true);
		} else if(i == styles_.size() - 1) {
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
	}
}

//------------------------------------------------------------------------------
// Name: on_editName_textChanged
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_editName_textChanged(const QString &text) {
	QList<QListWidgetItem *> selections = ui.listStyles->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	const int i = ui.listStyles->row(selection) - 1;
	styles_[i]->name = text.toStdString();
}

//------------------------------------------------------------------------------
// Name: on_editColorFG_textChanged
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_editColorFG_textChanged(const QString &text) {
	QList<QListWidgetItem *> selections = ui.listStyles->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	const int i = ui.listStyles->row(selection) - 1;
	styles_[i]->color = text;
}

//------------------------------------------------------------------------------
// Name: on_editColorBG_textChanged
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_editColorBG_textChanged(const QString &text) {
	QList<QListWidgetItem *> selections = ui.listStyles->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	const int i = ui.listStyles->row(selection) - 1;
	styles_[i]->bgColor = text;
}

//------------------------------------------------------------------------------
// Name: on_radioPlain_toggled
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_radioPlain_toggled(bool checked) {
	if(checked) {
		QList<QListWidgetItem *> selections = ui.listStyles->selectedItems();
		if(selections.size() != 1) {
			return;
		}

		QListWidgetItem *const selection = selections[0];
		const int i = ui.listStyles->row(selection) - 1;	
		styles_[i]->font = PLAIN_FONT;
	}
}

//------------------------------------------------------------------------------
// Name: on_radioBold_toggled
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_radioBold_toggled(bool checked) {
	if(checked) {
		QList<QListWidgetItem *> selections = ui.listStyles->selectedItems();
		if(selections.size() != 1) {
			return;
		}

		QListWidgetItem *const selection = selections[0];
		const int i = ui.listStyles->row(selection) - 1;
		styles_[i]->font = BOLD_FONT;
	}
}

//------------------------------------------------------------------------------
// Name: on_radioItalic_toggled
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_radioItalic_toggled(bool checked) {
	if(checked) {
		QList<QListWidgetItem *> selections = ui.listStyles->selectedItems();
		if(selections.size() != 1) {
			return;
		}

		QListWidgetItem *const selection = selections[0];
		const int i = ui.listStyles->row(selection) - 1;
		styles_[i]->font = ITALIC_FONT;
	}
}

//------------------------------------------------------------------------------
// Name: on_radioBoldItalic_toggled
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_radioBoldItalic_toggled(bool checked) {
	if(checked) {
		QList<QListWidgetItem *> selections = ui.listStyles->selectedItems();
		if(selections.size() != 1) {
			return;
		}

		QListWidgetItem *const selection = selections[0];
		const int i = ui.listStyles->row(selection) - 1;

		HighlightStyle *style = styles_[i];	

		style->font = BOLD_ITALIC_FONT;
	}
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


/*
** Apply the changes made in the highlight styles dialog to the stored
** highlight style information in HighlightStyles
*/
bool DialogDrawingStyles::updateHSList() {

#if 0
	// Replace the old highlight styles list with the new one from the dialog 
	for (int i = 0; i < NHighlightStyles; i++) {
		delete HighlightStyles[i];
	}

	for (int i = 0; i < styles_.size(); ++i) {
		HighlightStyles[i] = new HighlightStyle(*styles_[i]);
	}
	NHighlightStyles = styles_.size();

	// If a syntax highlighting dialog is up, update its menu 
	updateHighlightStyleMenu();

	// Redisplay highlighted windows which use changed style(s) 
	for(Document *window: WindowList) {
		UpdateHighlightStyles(window);
	}

	// Note that preferences have been changed 
	MarkPrefsChanged();
#endif
	return true;
}
