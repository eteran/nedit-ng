
#include "DialogWindowSize.h"
#include "Preferences.h"

#include <QIntValidator>
#include <QMessageBox>

/**
 * @brief Constructor for the DialogWindowSize class.
 *
 * @param parent The parent widget for this dialog, defaults to nullptr.
 * @param f The window flags for the dialog, defaults to Qt::WindowFlags().
 */
DialogWindowSize::DialogWindowSize(QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {

	ui.setupUi(this);
	connectSlots();

	Dialog::shrinkToFit(this);

	ui.editRows->setValidator(new QIntValidator(0, INT_MAX, this));
	ui.editColumns->setValidator(new QIntValidator(0, INT_MAX, this));
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogWindowSize::connectSlots() {
	connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &DialogWindowSize::buttonBox_accepted);
}
/**
 * @brief Handles the acceptance of the dialog when the "OK" button is clicked.
 */
void DialogWindowSize::buttonBox_accepted() {

	bool ok;
	const QString rows  = ui.editRows->text();
	const QString columns = ui.editColumns->text();

	if (rows.isEmpty()) {
		QMessageBox::warning(this, tr("Warning"), tr("Please supply a value for number of rows"));
		return;
	}

	const int rowValue = rows.toInt(&ok);
	if (!ok) {
		QMessageBox::warning(this, tr("Warning"), tr("Can't read integer value \"%1\" in number of rows").arg(rows));
		return;
	}

	if (columns.isEmpty()) {
		QMessageBox::warning(this, tr("Warning"), tr("Please supply a value for number of columns"));
		return;
	}

	const int colValue = columns.toInt(&ok);
	if (!ok) {
		QMessageBox::warning(this, tr("Warning"), tr("Can't read integer value \"%1\" in number of columns").arg(columns));
		return;
	}

	Preferences::SetPrefRows(rowValue);
	Preferences::SetPrefCols(colValue);
	accept();
}
