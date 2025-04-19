
#include "DialogPrint.h"
#include "DocumentWidget.h"

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QPainter>
#include <QPrinterInfo>
#include <QTextDocument>

#include <utility>

/**
 * @brief Constructor for DialogPrint class
 *
 * @param contents The contents to be printed, typically the text from a document.
 * @param jobName The name of the print job, used for identification.
 * @param document The DocumentWidget instance that contains the document to be printed.
 * @param parent The parent widget, defaults to nullptr.
 * @param f The window flags, defaults to Qt::WindowFlags().
 */
DialogPrint::DialogPrint(QString contents, QString jobName, DocumentWidget *document, QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f), document_(document), contents_(std::move(contents)), jobName_(std::move(jobName)) {
	ui.setupUi(this);
	connectSlots();

	const QStringList printers   = QPrinterInfo::availablePrinterNames();
	const QString defaultPrinter = QPrinterInfo::defaultPrinterName();
	ui.printers->addItem(tr("Print to File (PDF)"));

	ui.printers->addItems(printers);
	ui.printers->setCurrentText(defaultPrinter);
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogPrint::connectSlots() {
	connect(ui.buttonPrint, &QPushButton::clicked, this, &DialogPrint::buttonPrint_clicked);
}

/**
 * @brief Handles the change in the selected printer index.
 *
 * @param index The index of the currently selected printer in the dropdown.
 */
void DialogPrint::on_printers_currentIndexChanged(int index) {
	if (index == 0) {
		ui.labelCopies->setEnabled(false);
		ui.spinCopies->setEnabled(false);
	} else {
		ui.labelCopies->setEnabled(true);
		ui.spinCopies->setEnabled(true);
	}
}

/**
 * @brief Prints the contents of the dialog to the specified printer.
 *
 * @param printer The QPrinter instance to which the contents will be printed.
 */
void DialogPrint::print(QPrinter *printer) const {
	if (!document_) {
		return;
	}

	QTextDocument doc;
	doc.setDefaultFont(document_->defaultFont());
	doc.setPlainText(contents_);
	doc.print(printer);
}

/**
 * @brief Handles the print button click event.
 */
void DialogPrint::buttonPrint_clicked() {

	setCursor(Qt::WaitCursor);

	if (ui.printers->currentIndex() == 0) {

		QFileDialog dialog(this, tr("Print to File"));
		dialog.setFileMode(QFileDialog::AnyFile);
		dialog.setAcceptMode(QFileDialog::AcceptSave);
		dialog.setDirectory(QDir::currentPath());
		dialog.setOptions(QFileDialog::DontUseNativeDialog | QFileDialog::DontUseCustomDirectoryIcons);
		dialog.setFilter(QDir::AllDirs | QDir::AllEntries | QDir::Hidden | QDir::System);
		dialog.setNameFilter(tr("*.pdf"));

		if (dialog.exec()) {
			const QStringList selectedFiles = dialog.selectedFiles();
			const QString &filename         = selectedFiles[0];

			QPrinter printer;
			printer.setOutputFormat(QPrinter::PdfFormat);
			printer.setOutputFileName(filename);

			print(&printer);

			if (printer.printerState() == QPrinter::Error) {
				QMessageBox::warning(
					this,
					tr("Error Printing to File"),
					tr("Failed to print to file, is it writable?"));
			}

		} else {
			QMessageBox::warning(
				this,
				tr("Error Printing to File"),
				tr("No file was specified, please provide a filename to print to."));
		}

	} else {
		auto pi = QPrinterInfo::printerInfo(ui.printers->currentText());
		QPrinter printer(pi);
		printer.setCopyCount(ui.spinCopies->value());
		print(&printer);

		if (printer.printerState() == QPrinter::Error) {
			QMessageBox::warning(
				this,
				tr("Error Printing to File"),
				tr("An error occurred while printing."));
		}
	}

	setCursor(Qt::ArrowCursor);
	accept();
}

/**
 * @brief Handles the show event of the dialog.
 *
 * @param event The show event that is triggered when the dialog is displayed.
 */
void DialogPrint::showEvent(QShowEvent *event) {
	Q_UNUSED(event)
	resize(width(), minimumHeight());
}
