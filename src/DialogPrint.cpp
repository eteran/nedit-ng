
#include "DialogPrint.h"
#include "DocumentWidget.h"

#include <QMessageBox>
#include <QPrinterInfo>
#include <QTextDocument>
#include <QDir>
#include <QPainter>
#include <QFileDialog>
#include <utility>

/**
 * @brief DialogPrint::DialogPrint
 * @param contents
 * @param jobName
 * @param document
 * @param parent
 * @param f
 */
DialogPrint::DialogPrint(QString contents, QString jobname, DocumentWidget *document, QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f), document_(document), contents_(std::move(contents)), jobname_(std::move(jobname)) {
	ui.setupUi(this);
	connectSlots();

	QStringList printers = QPrinterInfo::availablePrinterNames();
	QString defaultPrinter = QPrinterInfo::defaultPrinterName();
	ui.printers->addItem(tr("Print to File (PDF)"));

	ui.printers->addItems(printers);
	ui.printers->setCurrentText(defaultPrinter);
}

/**
 * @brief DialogPrint::connectSlots
 */
void DialogPrint::connectSlots() {
	connect(ui.buttonPrint, &QPushButton::clicked, this, &DialogPrint::buttonPrint_clicked);
}


/**
 * @brief DialogPrint::on_printers_currentIndexChanged
 * @param index
 */
void DialogPrint::on_printers_currentIndexChanged(int index) {
	if(index == 0) {
		ui.labelCopies->setEnabled(false);
		ui.spinCopies->setEnabled(false);
	} else {
		ui.labelCopies->setEnabled(true);
		ui.spinCopies->setEnabled(true);
	}
}

/**
 * @brief DialogPrint::print
 * @param printer
 */
void DialogPrint::print(QPrinter *printer) {
	QTextDocument doc;
	doc.setDefaultFont(document_->defaultFont());
	doc.setPlainText(contents_);
	doc.print(printer);
}

/**
 * @brief DialogPrint::buttonPrint_clicked
 */
void DialogPrint::buttonPrint_clicked() {

	setCursor(Qt::WaitCursor);

	if(ui.printers->currentIndex() == 0) {

		QFileDialog dialog(this, tr("Print to File"));
		dialog.setFileMode(QFileDialog::AnyFile);
		dialog.setAcceptMode(QFileDialog::AcceptSave);
		dialog.setDirectory(QDir::currentPath());
		dialog.setOptions(QFileDialog::DontUseNativeDialog | QFileDialog::DontUseCustomDirectoryIcons);
		dialog.setFilter(QDir::AllDirs | QDir::AllEntries | QDir::Hidden | QDir::System);
		dialog.setNameFilter(tr("*.pdf"));

		if(dialog.exec()) {
			QStringList selectedFiles = dialog.selectedFiles();
			QString filename          = selectedFiles[0];

			QPrinter printer;
			printer.setOutputFormat(QPrinter::PdfFormat);
			printer.setOutputFileName(filename);

			print(&printer);

			if(printer.printerState() == QPrinter::Error) {
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

		if(printer.printerState() == QPrinter::Error) {
			QMessageBox::warning(
						this,
						tr("Error Printing to File"),
						tr("An error occured while printing."));
		}
	}

	setCursor(Qt::ArrowCursor);
	accept();
}

/**
 * @brief DialogPrint::showEvent
 * @param event
 */
void DialogPrint::showEvent(QShowEvent *event) {
	Q_UNUSED(event)
	resize(width(), minimumHeight());
}
