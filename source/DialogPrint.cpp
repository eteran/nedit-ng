
#include "DialogPrint.h"
#include "preferences.h"
#include "Settings.h"

#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

#include <fstream>

namespace {

QString PrintCommand;  /* print command string */
QString CopiesOption;  /* # of copies argument string */
QString QueueOption;   /* queue name argument string */
QString NameOption;    /* print job name argument string */
QString HostOption;    /* host name argument string */
QString DefaultQueue;  /* default print queue */
QString DefaultHost;   /* default host name */

int Copies;                       /* # of copies last entered by user */
QString Queue;                    /* queue name last entered by user */
QString Host;                     /* host name last entered by user */
QString CmdText;                  /* print command last entered by user */
bool CmdFieldModified = false;    /* user last changed the print command field, so don't trust the rest */
bool PreferencesLoaded = false;

/**
 * Is flpr present in the search path and is it executable ?
 *
 * @brief flprPresent
 * @return
 */
bool flprPresent() {
    /* Is flpr present in the search path and is it executable ? */
    QString path = QStandardPaths::findExecutable(QLatin1String("flpr"));
    return !path.isEmpty();
}

/**
 * @brief foundTag
 * @param tagfilename
 * @param tagname
 * @return
 */
QByteArray foundTag(const char *tagfilename, const char *tagname) {

    char tagformat[512];
    snprintf(tagformat, sizeof(tagformat), "%s %%s", tagname);

    std::ifstream file(tagfilename);
    for(std::string line; std::getline(file, line); ) {
        char result_buffer[1024];

        // NOTE(eteran): format string vuln?
        if (sscanf(line.c_str(), tagformat, result_buffer) != 0) {
            return QByteArray(result_buffer);
        }
    }

    return QByteArray();
}

/**
 * @brief getFlprQueueDefault
 * @return
 */
QString getFlprQueueDefault() {

    QByteArray defqueue = qgetenv("FLPQUE");
    if(defqueue.isNull()) {
        defqueue = foundTag("/usr/local/etc/flp.defaults", "queue");
    }

    return QString::fromLocal8Bit(defqueue);
}

/**
 * @brief getFlprHostDefault
 * @return
 */
QString getFlprHostDefault() {

    QByteArray defhost = qgetenv("FLPHOST");
    if(defhost.isNull()) {
        defhost = foundTag("/usr/local/etc/flp.defaults", "host");
    }

    return QString::fromLocal8Bit(defhost);
}

/**
 * @brief getLprQueueDefault
 * @return
 */
QString getLprQueueDefault() {

    QByteArray defqueue = qgetenv("PRINTER");
    return QString::fromLocal8Bit(defqueue);
}

}

/**
 * @brief DialogPrint::DialogPrint
 * @param PrintFileName
 * @param jobName
 * @param parent
 * @param f
 */
DialogPrint::DialogPrint(const QString &PrintFileName, const QString &jobName, QWidget *parent, Qt::WindowFlags f) : Dialog(parent, f) {
	ui.setupUi(this);
	
	ui.spinCopies->setSpecialValueText(tr(" "));
	
	// Prohibit a relatively random sampling of characters that will cause
	// problems on command lines
	auto validator = new QRegExpValidator(QRegExp(QLatin1String("[^ \t,;|<>()\\[\\]{}!@?]*")), this);
	
	ui.editHost->setValidator(validator);
	ui.editQueue->setValidator(validator);
	
	// In case the program hasn't called LoadPrintPreferences, 
	// set up the default values for the print preferences
	if (!PreferencesLoaded) {
        LoadPrintPreferencesEx(true);
	}

	// Make the PrintFile information available to the callback routines
	PrintFileName_ = PrintFileName;
	PrintJobName_  = jobName;
	
	if (!DefaultQueue.isEmpty()) {
		ui.labelQueue->setText(tr("Queue (%1)").arg(DefaultQueue));
	} else {
		ui.labelQueue->setText(tr("Queue"));
	}
	
	if (HostOption.isEmpty()) {
		ui.labelHost->setVisible(false);
		ui.editHost->setVisible(false);
	} else {
		ui.labelHost->setVisible(true);
		ui.editHost->setVisible(true);

		if (!DefaultHost.isEmpty()) {
			ui.labelHost->setText(tr("Host (%1)").arg(DefaultHost));
		} else {
			ui.labelHost->setText(tr("Host"));
		}
	}
	
	if(CmdFieldModified) {
		// if they printed in the past, restore the command they used
		ui.editCommand->setText(CmdText);	
	} else {
		updatePrintCmd();
	}
}

/*
** LoadPrintPreferences
**
** Read an X database to obtain print dialog preferences.
**
**	prefDB		X database potentially containing print preferences
**	appName		Application name which can be used to qualify
**			resource names for database lookup.
**	appClass	Application class which can be used to qualify
**			resource names for database lookup.
**	lookForFlpr	Check if the flpr print command is installed
**			and use that for the default if it's found.
**			(flpr is a Fermilab utility for printing on
**			arbitrary systems that support the lpr protocol)
*/
void DialogPrint::LoadPrintPreferencesEx(bool lookForFlpr) {

    QString filename = Settings::configFile();
    QSettings settings(filename, QSettings::IniFormat);
    settings.beginGroup(QLatin1String("Printer"));

    /* check if flpr is installed, and otherwise choose appropriate
       printer per system type */
    if (lookForFlpr && flprPresent()) {

        QString defaultQueue = getFlprQueueDefault();
        QString defaultHost  = getFlprHostDefault();

        PrintCommand = settings.value(tr("printCommand"),      QLatin1String("flpr")).toString();
        CopiesOption = settings.value(tr("printCopiesOption"), QString()).toString();
        QueueOption  = settings.value(tr("printQueueOption"),  QLatin1String("-q")).toString();
        NameOption   = settings.value(tr("printNameOption"),   QLatin1String("-j ")).toString();
        HostOption   = settings.value(tr("printHostOption"),   QLatin1String("-h")).toString();
        DefaultQueue = settings.value(tr("printDefaultQueue"), defaultQueue).toString();
        DefaultHost  = settings.value(tr("printDefaultHost"),  defaultHost).toString();

    } else {

        QString defaultQueue = getLprQueueDefault();

        PrintCommand = settings.value(tr("printCommand"),      QLatin1String("lpr")).toString();
        CopiesOption = settings.value(tr("printCopiesOption"), QLatin1String("-# ")).toString();
        QueueOption  = settings.value(tr("printQueueOption"),  QLatin1String("-P ")).toString();
        NameOption   = settings.value(tr("printNameOption"),   QLatin1String("-J ")).toString();
        HostOption   = settings.value(tr("printHostOption"),   QString()).toString();
        DefaultQueue = settings.value(tr("printDefaultQueue"), defaultQueue).toString();
        DefaultHost  = settings.value(tr("printDefaultHost"),  QString()).toString();
    }

    PreferencesLoaded = true;
}

/**
 * @brief DialogPrint::on_spinCopies_valueChanged
 * @param n
 */
void DialogPrint::on_spinCopies_valueChanged(int n) {
	Q_UNUSED(n);
	updatePrintCmd();
}

/**
 * @brief DialogPrint::on_editQueue_textChanged
 * @param text
 */
void DialogPrint::on_editQueue_textChanged(const QString &text) {
	Q_UNUSED(text);
	updatePrintCmd();
}

/**
 * @brief DialogPrint::on_editHost_textChanged
 * @param text
 */
void DialogPrint::on_editHost_textChanged(const QString &text) {
	Q_UNUSED(text);
	updatePrintCmd();
}

/**
 * @brief DialogPrint::updatePrintCmd
 */
void DialogPrint::updatePrintCmd() {

	QString copiesArg;
	QString queueArg;	
	QString hostArg;
	QString jobArg;

	// read each text field in the dialog and generate the corresponding
	// command argument
	if (!CopiesOption.isEmpty()) {
		const int copiesValue = ui.spinCopies->value();
		if(copiesValue != 0) {
			copiesArg = tr(" %1%2").arg(CopiesOption).arg(copiesValue);
		}
	}
	
	if (!QueueOption.isEmpty()) {
		const QString str = ui.editQueue->text();
		if (str.isEmpty()) {
		} else {
			queueArg = tr(" %1%2").arg(QueueOption, str);
		}
	}
	

	if (!HostOption.isEmpty()) {
		const QString str = ui.editHost->text();
		if (str.isEmpty()) {
		} else {
			hostArg = tr(" %1%2").arg(HostOption, str);
		}
	}

	if (!NameOption.isEmpty()) {
		jobArg = tr(" %1\"%2\"").arg(NameOption, PrintJobName_);
	}

	// Compose the command from the options determined above
	ui.editCommand->setText(tr("%1%2%3%4%5").arg(PrintCommand, copiesArg, queueArg, hostArg, jobArg));


	// Indicate that the command field was synthesized from the other fields,
	// so future dialog invocations can safely re-generate the command without
	// overwriting commands specifically entered by the user
	CmdFieldModified = false;
}

/**
 * @brief DialogPrint::on_editCommand_textEdited
 * @param text
 */
void DialogPrint::on_editCommand_textEdited(const QString &text) {
	Q_UNUSED(text);
	CmdFieldModified = true;
}

/**
 * @brief DialogPrint::on_buttonPrint_clicked
 */
void DialogPrint::on_buttonPrint_clicked() {

    // TODO(eteran): 2.0 use Qt's built in print system

	// get the print command from the command text area
	const QString str = ui.editCommand->text();

	// add the file name and output redirection to the print command
	QString command = tr("cat %1 | %2 2>&1").arg(PrintFileName_, str);

    // create the process and connect the output streams to the readyRead events
    QProcess process;
    QString errorString;
    bool success = true;
    process.setProcessChannelMode(QProcess::MergedChannels);

    connect(&process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, [this, &errorString, &success, &process](int exitCode, QProcess::ExitStatus exitStatus){
        if (exitStatus != QProcess::NormalExit) {
            QMessageBox::warning(this, tr("Print Error"), tr("Unable to Print:\n%1").arg(process.errorString()));
            success = false;
            return;
        }

        if(exitCode != EXIT_SUCCESS) {
            QMessageBox::warning(this, tr("Print Error"), tr("Unable to Print:\n%1").arg(errorString));
            success = false;
            return;
        }
    });

    connect(&process, &QProcess::readyRead, [&errorString, &process]() {
        errorString = QString::fromLocal8Bit(process.readAll());
    });

    // start it off!
    QStringList args;
    args << QLatin1String("-c");
    args << command;
    process.start(GetPrefShell(), args);
    process.closeWriteChannel();
    process.waitForFinished();

    if(!success) {
        return;
    }
	
	// Print command succeeded, so retain the current print parameters
	if (!CopiesOption.isEmpty()) {
		Copies = ui.spinCopies->value();
	}
	
	if (!QueueOption.isEmpty()) {
		Queue = ui.editQueue->text();
	}
	
	if (!HostOption.isEmpty()) {
		Host = ui.editHost->text();
	}

	CmdText = ui.editCommand->text();	
	accept();
}

void DialogPrint::showEvent(QShowEvent *event) {
    Q_UNUSED(event);
    resize(width(), minimumHeight());
}
