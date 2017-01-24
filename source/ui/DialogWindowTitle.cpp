
#include <QRegExp>
#include <QRegExpValidator>
#include "DialogWindowTitle.h"
#include "MainWindow.h"
#include "DocumentWidget.h"
#include "util/clearcase.h"
#include "preferences.h"
#include "nedit.h"
#include "util/utils.h"
#include "util/fileUtils.h"

struct UpdateState {
	bool fileNamePresent;
	bool hostNamePresent;
	bool userNamePresent;
	bool serverNamePresent;
	bool clearCasePresent;
	bool fileStatusPresent;
	bool dirNamePresent;
	bool shortStatus;
	int noOfComponents;
};

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogWindowTitle::DialogWindowTitle(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
	
	inConstructor_ = true;
	
	ui.editDirectory->setValidator(new QRegExpValidator(QRegExp(QLatin1String("[0-9]")), this));

	/* copy attributes from current this so that we can use as many
	 * 'real world' defaults as possible when testing the effect
	 * of different formatting strings.
     */
    path_        = document->path_;
    filename_    = document->filename_;
	
	QString clearCase = GetClearCaseViewTag();
	
    viewTag_     = !clearCase.isNull() ? clearCase : tr("viewtag");
    serverName_  = IsServer ? QLatin1String(GetPrefServerName()) : tr("servername");
	isServer_    = IsServer;
    filenameSet_ = document->filenameSet_;
    lockReasons_ = document->lockReasons_;
    fileChanged_ = document->fileChanged_;
	
	ui.checkClearCasePresent->setChecked(!clearCase.isNull());
	
	suppressFormatUpdate_ = false; 
	
	// set initial value of format field 
	ui.editFormat->setText(QLatin1String(GetPrefTitleFormat()));
	
	// force update of the dialog 
	setToggleButtons();

	formatChangedCB();
	
	inConstructor_ = false;
	
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogWindowTitle::~DialogWindowTitle() {
}

// a utility that sets the values of all toggle buttons 
void DialogWindowTitle::setToggleButtons() {

	ui.checkDirectoryPresent->setChecked(filenameSet_);
	ui.checkFileModified->setChecked(fileChanged_);
	ui.checkFileReadOnly->setChecked(lockReasons_.isPermLocked());
	ui.checkFileLocked->setChecked(lockReasons_.isUserLocked());
	// Read-only takes precedence on locked 
	ui.checkFileLocked->setEnabled(!lockReasons_.isPermLocked());

	QString ccTag = GetClearCaseViewTag();

	ui.checkClearCasePresent->setChecked(!ccTag.isNull());
	ui.checkServerNamePresent->setChecked(isServer_);

	if (!ccTag.isNull() && isServer_ && GetPrefServerName()[0] != '\0' && ccTag == QLatin1String(GetPrefServerName())) {
		ui.checkServerEqualsCC->setChecked(true);
	} else {
		ui.checkServerEqualsCC->setChecked(false);
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_editFormat_textChanged(const QString &text) {
	Q_UNUSED(text);
	formatChangedCB();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::formatChangedCB() {


	bool filenameSet = ui.checkDirectoryPresent->isChecked();
	

	if (suppressFormatUpdate_) {
		return; // Prevent recursive feedback 
	}

	QString format = ui.editFormat->text();
	QString serverName;
	if (ui.checkServerEqualsCC->isChecked() && ui.checkClearCasePresent->isChecked()) {
		serverName = viewTag_;
	} else {
		serverName = ui.checkServerNamePresent->isChecked() ? serverName_ : QLatin1String("");
	}


	QString title = FormatWindowTitleEx(
		filename_, 
		filenameSet_ ? path_ : tr("/a/very/long/path/used/as/example/"), 
		ui.checkClearCasePresent->isChecked() ? viewTag_ : QString(), 
		serverName, 
		isServer_, 
		filenameSet, 
		lockReasons_, 
		ui.checkFileModified->isChecked(),
		format);

	ui.editPreview->setText(title);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
QString DialogWindowTitle::FormatWindowTitle(const QString &filename, const QString &path, const QString &clearCaseViewTag, const QString &serverName, bool isServer, bool filenameSet, LockReasons lockReasons, bool fileChanged, const QString &titleFormat) {
	return FormatWindowTitleInternal(filename, path, clearCaseViewTag, serverName, isServer, filenameSet, lockReasons, fileChanged, titleFormat, nullptr);
	
}

/*
** Format the windows title using a printf like formatting string.
** The following flags are recognised:
**  %c    : ClearCase view tag
**  %s    : server name
**  %[n]d : directory, with one optional digit specifying the max number
**          of trailing directory components to display. Skipped components are
**          replaced by an ellipsis (...).
**  %f    : file name
**  %h    : host name
**  %S    : file status
**  %u    : user name
**
**  if the ClearCase view tag and server name are identical, only the first one
**  specified in the formatting string will be displayed.
*/
QString DialogWindowTitle::FormatWindowTitleEx(const QString &filename, const QString &path, const QString &clearCaseViewTag, const QString &serverName, bool isServer, bool filenameSet, LockReasons lockReasons, bool fileChanged, const QString &titleFormat) {


	UpdateState state;
	QString title = FormatWindowTitleInternal(filename, path, clearCaseViewTag, serverName, isServer, filenameSet, lockReasons, fileChanged, titleFormat, &state);

	// Prevent recursive callback loop 
	suppressFormatUpdate_ = true;

	/* Sync radio buttons with format string (in case the user entered
	   the format manually) */
	ui.checkFileName->setChecked(state.fileNamePresent);
	ui.checkFileStatus->setChecked(state.fileStatusPresent);
	ui.checkServerName->setChecked(state.serverNamePresent);
	ui.checkClearCase->setChecked(state.clearCasePresent);

	ui.checkDirectory->setChecked(state.dirNamePresent);
	ui.checkHostName->setChecked(state.hostNamePresent);
	ui.checkUserName->setChecked(state.userNamePresent);

	ui.checkBrief->setEnabled(state.fileStatusPresent);


	if (state.fileStatusPresent) {
		ui.checkBrief->setChecked(state.shortStatus);
	}

	// Directory components are also sensitive to presence of dir 
	ui.editDirectory->setEnabled(state.dirNamePresent);
	ui.labelDirectory->setEnabled(state.dirNamePresent);

	// Avoid erasing number when not active 
	if (state.dirNamePresent)  {

		if (state.noOfComponents >= 0) {

			QString value = ui.editDirectory->text();
			QString comp  = tr("%1").arg(state.noOfComponents);

			// Don't overwrite unless diff. 
			if (value != comp) {
				ui.editDirectory->setText(comp);
			}
		} else {
			ui.editDirectory->setText(tr(""));
		}
	}

	// Enable/disable test buttons, depending on presence of codes 
	ui.checkFileModified->setEnabled(state.fileStatusPresent);
	ui.checkFileReadOnly->setEnabled(state.fileStatusPresent);
	ui.checkFileLocked->setEnabled(state.fileStatusPresent && !lockReasons_.isPermLocked());
	ui.checkServerNamePresent->setEnabled(state.serverNamePresent);
	ui.checkClearCasePresent->setEnabled(state.clearCasePresent);
	ui.checkServerEqualsCC->setEnabled(state.clearCasePresent && state.serverNamePresent);
	ui.checkDirectoryPresent->setEnabled(state.dirNamePresent);
	suppressFormatUpdate_ = false;

	return title;
}

/*
** Remove empty paranthesis pairs and multiple spaces in a row
** with one space.
** Also remove leading and trailing spaces and dashes.
*/
QString DialogWindowTitle::compressWindowTitle(const QString &title) {

	QString result = title;

	// remove empty brackets
	result.replace(QLatin1String("()"), QString());
	result.replace(QLatin1String("{}"), QString());
	result.replace(QLatin1String("[]"), QString());
	
	// remove leading/trailing whitspace/dashes
	result.replace(QRegExp(QLatin1String("^[\\s-]+")), QString());
	result.replace(QRegExp(QLatin1String("[\\s-]+$")), QString()); 
	return result.simplified();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkFileName_toggled(bool checked) {

	if(inConstructor_) {
		return;
	}
	
	if (checked) {
		appendToFormat(QLatin1String(" %f"));
	} else {
		removeFromFormat(QLatin1String("%f"));
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkHostName_toggled(bool checked) {

	if(inConstructor_) {
		return;
	}
	
	if (checked) {
		appendToFormat(QLatin1String(" [%h]"));
	} else {
		removeFromFormat(QLatin1String("%h"));
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkFileStatus_toggled(bool checked) {

	if(inConstructor_) {
		return;
	}
	
	ui.checkBrief->setEnabled(checked);

	if (checked) {
		if (ui.checkBrief->isChecked()) {
			appendToFormat(QLatin1String(" (%*S)"));
		} else {
			appendToFormat(QLatin1String(" (%S)"));
		}
	} else {
		removeFromFormat(QLatin1String("%S"));
		removeFromFormat(QLatin1String("%*S"));
	}

}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkBrief_toggled(bool checked) {

	if(inConstructor_) {
		return;
	}
	
	if (suppressFormatUpdate_) {
		return;
	}
	
	QString format = ui.editFormat->text();
	
	if (checked) {
		// Find all %S occurrences and replace them by %*S 
		format.replace(QLatin1String("%S"), QLatin1String("%*S"));
	} else {
		// Replace all %*S occurences by %S 
		format.replace(QLatin1String("%*S"), QLatin1String("%S"));
	}

	ui.editFormat->setText(format);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkUserName_toggled(bool checked) {

	if(inConstructor_) {
		return;
	}
	
	if (checked) {
		appendToFormat(QLatin1String(" %u"));
	} else {
		removeFromFormat(QLatin1String("%u"));
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkClearCase_toggled(bool checked) {

	if(inConstructor_) {
		return;
	}
	
	if (checked) {
		appendToFormat(QLatin1String(" {%c}"));
	} else {
		removeFromFormat(QLatin1String("%c"));
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkServerName_toggled(bool checked) {

	if(inConstructor_) {
		return;
	}

	if (checked) {
		appendToFormat(QLatin1String(" [%s]"));
	} else {
		removeFromFormat(QLatin1String("%s"));
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkDirectory_toggled(bool checked) {
	
	if(inConstructor_) {
		return;
	}
	
	ui.editDirectory->setEnabled(checked);
	ui.labelDirectory->setEnabled(checked);
	
	if (checked) {
		QString buf;
		QString value = ui.editDirectory->text();
		if (!value.isEmpty()) {
		
			bool ok;
			int maxComp = value.toInt(&ok);
			
			if(ok && maxComp > 0) {
				buf = tr(" %%1d ").arg(maxComp);
			} else {
				buf = tr(" %d "); // Should not be necessary 
			}
		} else {
			buf = tr(" %d ");
		}
		appendToFormat(buf);
	} else {
		removeFromFormat(QLatin1String("%d"));
		for(int i = 1; i < 10; ++i) {
			removeFromFormat(tr("%%1d").arg(i));
		}
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkFileModified_toggled(bool checked) {
	fileChanged_ = checked;
	formatChangedCB();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkFileReadOnly_toggled(bool checked) {
	lockReasons_.setPermLocked(checked);
	formatChangedCB();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkFileLocked_toggled(bool checked) {
	lockReasons_.setUserLocked(checked);
	formatChangedCB();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkServerNamePresent_toggled(bool checked) {

	if (!checked) {
		ui.checkServerEqualsCC->setChecked(false);
	}
	
	isServer_ = checked;
	
	formatChangedCB();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkClearCasePresent_toggled(bool checked) {
	if (!checked) {
		ui.checkServerEqualsCC->setChecked(false);
	}
	
	formatChangedCB();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkDirectoryPresent_toggled(bool checked) {
	Q_UNUSED(checked);
	formatChangedCB();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkServerEqualsCC_toggled(bool checked) {
	if (checked) {
		ui.checkClearCasePresent->setChecked(true);
		ui.checkServerNamePresent->setChecked(true);
		isServer_ = true;
	}
	formatChangedCB();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::appendToFormat(const QString &string) {

	QString format = ui.editFormat->text();
	QString buf;
	buf.reserve(string.size() + format.size());	
	buf.append(format);
	buf.append(string);
	ui.editFormat->setText(buf);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::removeFromFormat(const QString &string) {

	QString format = ui.editFormat->text();
	
	// If the string is preceded or followed by a brace, include
	// the brace(s) for removal
	format.replace(QRegExp(tr("[\\{\\(\\[\\<]?%1[\\}\\)\\]\\>]?").arg(QRegExp::escape(string))), QString());
	
	// remove leading/trailing whitspace/dashes
	format.replace(QRegExp(QLatin1String("^[\\s]+")), QString());
	format.replace(QRegExp(QLatin1String("[\\s]+$")), QString()); 

	ui.editFormat->setText(format);
}

//------------------------------------------------------------------------------
// Name: on_buttonBox_clicked
//------------------------------------------------------------------------------
void DialogWindowTitle::on_buttonBox_clicked(QAbstractButton *button) {
	if(ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {

		QString format = ui.editFormat->text();

		if (format != QLatin1String(GetPrefTitleFormat())) {
			SetPrefTitleFormat(format.toLatin1().data());
		}

	} else if(ui.buttonBox->standardButton(button) == QDialogButtonBox::RestoreDefaults) {
		ui.editFormat->setText(QLatin1String("{%c} [%s] %f (%S) - %d"));
	} else if(ui.buttonBox->standardButton(button) == QDialogButtonBox::Help) {

	#if 0
		Help(HELP_CUSTOM_TITLE_DIALOG);
	#endif	
	}
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_editDirectory_textChanged(const QString &text) {

	if (suppressFormatUpdate_) {
		return;
	}

	QString format = ui.editFormat->text();
	QString value = text;
	bool ok;
	int maxComp = value.toInt(&ok);
	if(ok && maxComp >= 0) {
		format.replace(QRegExp(QLatin1String("%[0-9]?d")), tr("%%1d").arg(maxComp)); 
	} else {
		format.replace(QRegExp(QLatin1String("%[0-9]?d")), tr("%d")); 
	}

	ui.editFormat->setText(format);
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
QString DialogWindowTitle::FormatWindowTitleInternal(const QString &filename, const QString &path, const QString &clearCaseViewTag, const QString &serverName, bool isServer, bool filenameSet, LockReasons lockReasons, bool fileChanged, const QString &titleFormat, UpdateState *state) {
	QString title;

	// Flags to supress one of these if both are specified and they are identical 
	bool serverNameSeen = false;
	bool clearCaseViewTagSeen = false;

	bool fileNamePresent   = false;
	bool hostNamePresent   = false;
	bool userNamePresent   = false;
	bool serverNamePresent = false;
	bool clearCasePresent  = false;
	bool fileStatusPresent = false;
	bool dirNamePresent    = false;
	int noOfComponents     = -1;
	bool shortStatus       = false;

	auto format_it = titleFormat.begin();

	while (format_it != titleFormat.end()) {
		QChar c = *format_it++;
		if (c == QLatin1Char('%')) {
			
			if (format_it == titleFormat.end()) {
				title.push_back(QLatin1Char('%'));
				break;
			}
			
			c = *format_it++;
			
			switch (c.toLatin1()) {
			case 'c': // ClearCase view tag 
				clearCasePresent = true;
				if (!clearCaseViewTag.isNull()) {
					if (!serverNameSeen || serverName != clearCaseViewTag) {
						title.append(clearCaseViewTag);
						clearCaseViewTagSeen = true;
					}
				}
				break;

			case 's': // server name 
				serverNamePresent = true;
				if (isServer && !serverName.isEmpty()) { // only applicable for servers 
					if (!clearCaseViewTagSeen || serverName != clearCaseViewTag) {
						title.append(serverName);
						serverNameSeen = true;
					}
				}
				break;

			case 'd': // directory without any limit to no. of components 
				dirNamePresent = true;
				if (filenameSet) {
					title.append(path);
				}
				break;

			case '0': // directory with limited no. of components 
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (*format_it == QLatin1Char('d')) {
					dirNamePresent = true;
					noOfComponents = c.toLatin1() - '0';
					format_it++; // delete the argument 

					if (filenameSet) {
						QString trailingPath = GetTrailingPathComponentsEx(path, noOfComponents);

						// prefix with ellipsis if components were skipped 
						if (trailingPath != path) {
							title.append(QLatin1String("..."));
						}
						title.append(trailingPath);
					}
				}
				break;

			case 'f': // file name 
				fileNamePresent = true;
				title.append(filename);
				break;

			case 'h': // host name 
				hostNamePresent = true;
				title.append(GetNameOfHostEx());
				break;

			case 'S': // file status 
				fileStatusPresent = true;
				if (lockReasons.isAnyLockedIgnoringUser() && fileChanged)
					title.append(tr("read only, modified"));
				else if (lockReasons.isAnyLockedIgnoringUser())
					title.append(tr("read only"));
				else if (lockReasons.isUserLocked() && fileChanged)
					title.append(tr("locked, modified"));
				else if (lockReasons.isUserLocked())
					title.append(tr("locked"));
				else if (fileChanged)
					title.append(tr("modified"));
				break;

			case 'u': // user name 
				userNamePresent = true;
				title.append(GetUserNameEx());
				break;

			case '%': // escaped % 
				title.append(QLatin1Char('%'));
				break;

			case '*': // short file status ? 
				fileStatusPresent = true;
				if (format_it != titleFormat.end() && *format_it == QLatin1Char('S')) {
					++format_it;
					shortStatus = true;
					if (lockReasons.isAnyLockedIgnoringUser() && fileChanged)
						title.append(tr("RO*"));
					else if (lockReasons.isAnyLockedIgnoringUser())
						title.append(tr("RO"));
					else if (lockReasons.isUserLocked() && fileChanged)
						title.append(tr("LO*"));
					else if (lockReasons.isUserLocked())
						title.append(tr("LO"));
					else if (fileChanged)
						title.append(tr("*"));
					break;
				}
			// fall-through 
			default:
				title.append(c);
				break;
			}
		} else {
			title.append(c);
		}
	}

	title = DialogWindowTitle::compressWindowTitle(title);

	if (title.isEmpty()) {
		title = tr("<empty>"); // For preview purposes only 
	}
	
	if(state) {
		state->fileNamePresent   = fileNamePresent;
		state->hostNamePresent   = hostNamePresent;
		state->userNamePresent   = userNamePresent;
		state->serverNamePresent = serverNamePresent;
		state->clearCasePresent  = clearCasePresent;
		state->fileStatusPresent = fileStatusPresent; 
		state->dirNamePresent    = dirNamePresent;
		state->shortStatus       = shortStatus;
		state->noOfComponents    = noOfComponents;
	}

	return title;
}
