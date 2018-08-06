
#include "DialogWindowSize.h"
#include "preferences.h"

#include <QMessageBox>
#include <QIntValidator>

DialogWindowSize::DialogWindowSize(QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) {
	ui.setupUi(this);

	ui.editWidth->setValidator(new QIntValidator(0, INT_MAX, this));
	ui.editHeight->setValidator(new QIntValidator(0, INT_MAX, this));
}

void DialogWindowSize::on_buttonBox_accepted() {

	bool ok;
	const QString width  = ui.editWidth->text();
	const QString height = ui.editHeight->text();

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

	Preferences::SetPrefRows(rowValue);
	Preferences::SetPrefCols(colValue);
	accept();
}
