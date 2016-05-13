
#include "DialogMoveDocument.h"
#include "Document.h"

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogMoveDocument::DialogMoveDocument(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogMoveDocument::~DialogMoveDocument() {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogMoveDocument::on_buttonMove_clicked() {
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogMoveDocument::addItem(Document *document) {

	QString name = tr("%1%2").arg(document->filenameSet_ ? document->path_ : tr("")).arg(document->filename_);

	documents_.push_back(document);
	ui.listDocuments->addItem(name);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogMoveDocument::resetSelection() {
	ui.listDocuments->setCurrentRow(0);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogMoveDocument::setLabel(const QString &label) {
	ui.label->setText(tr("Move %1 into window of").arg(label));
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogMoveDocument::setMultipleDocuments(bool value) {
	ui.checkMoveAll->setVisible(value);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
int DialogMoveDocument::selectionIndex() const {
	return ui.listDocuments->currentRow();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
bool DialogMoveDocument::moveAllSelected() const {
	return ui.checkMoveAll->isChecked();
}
