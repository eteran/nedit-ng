
#include "DialogWindowSize.h"
#include <QMessageBox>
#include "preferences.h"

DialogWindowSize::DialogWindowSize(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
}

DialogWindowSize::~DialogWindowSize() {
}

void DialogWindowSize::on_buttonBox_accepted() {

	bool ok;
	QString width  = ui.editWidth->text();
	QString height = ui.editHeight->text();

	if(width.isEmpty()) {
		QMessageBox::warning(this, tr("Warning"), tr("Please supply a value for number of rows"));
		return;
	}
	
	int rowValue = width.toInt(&ok);
	if(!ok) {
		QMessageBox::warning(this, tr("Warning"), tr("Can't read integer value \"%1\" in number of rows").arg(width));
		return;
	}


	if(height.isEmpty()) {
		QMessageBox::warning(this, tr("Warning"), tr("Please supply a value for number of columns"));
		return;
	}

	int colValue = height.toInt(&ok);
	if(!ok) {
		QMessageBox::warning(this, tr("Warning"), tr("Can't read integer value \"%1\" in number of columns").arg(height));
		return;
	}

	// set the corresponding preferences and dismiss the dialog 
	SetPrefRows(rowValue);
	SetPrefCols(colValue);
	
	accept();
}
