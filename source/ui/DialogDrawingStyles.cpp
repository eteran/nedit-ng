
#include "DialogDrawingStyles.h"
#include "HighlightStyle.h"
#include "highlightData.h"


//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogDrawingStyles::DialogDrawingStyles(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
	
	
	// Copy the list of highlight style information to one that the user can freely edit
	for (int i = 0; i < NHighlightStyles; i++) {
		styles_.push_back(new HighlightStyle(*HighlightStyles[i]));
	}
	
	ui.listStyles->addItem(tr("New"));
	for(HighlightStyle *style : styles_) {
		ui.listStyles->addItem(style->name);
	}
	ui.listStyles->setCurrentRow(0);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogDrawingStyles::~DialogDrawingStyles() {
	qDeleteAll(styles_);
}

//------------------------------------------------------------------------------
// Name: on_buttonCopy_clicked
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_buttonCopy_clicked() {
}

//------------------------------------------------------------------------------
// Name: 
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
// Name: 
//------------------------------------------------------------------------------
void DialogDrawingStyles::on_listStyles_itemSelectionChanged() {

	QList<QListWidgetItem *> selections = ui.listStyles->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	QString styleName = selection->text();

	if(styleName == tr("New")) {
		ui.buttonUp    ->setEnabled(false);
		ui.buttonDown  ->setEnabled(false);
		ui.buttonDelete->setEnabled(false);
		ui.buttonCopy  ->setEnabled(false);
	} else {
		const int i = ui.listStyles->row(selection) - 1;

		HighlightStyle *style = styles_[i];
		
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
