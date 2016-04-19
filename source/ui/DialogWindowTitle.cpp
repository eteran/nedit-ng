
#include <QRegExp>
#include <QRegExpValidator>
#include "DialogWindowTitle.h"
#include "Document.h"
#include "clearcase.h"
#include "preferences.h"
#include "nedit.h"
#include "utils.h"


#if 0
<widget class="QCheckBox" name="checkFileName">   == fileW
<widget class="QCheckBox" name="checkHostName">   == hostW
<widget class="QCheckBox" name="checkFileStatus"> == statusW
<widget class="QCheckBox" name="checkBrief">      == shortStatusW
<widget class="QCheckBox" name="checkUserName">   == nameW
<widget class="QCheckBox" name="checkClearCase">  == ccW
<widget class="QCheckBox" name="checkServerName"> == serverW
<widget class="QCheckBox" name="checkDirectory">  == dirW
<widget class="QLineEdit" name="editDirectory">   == ndirW

<widget class="QCheckBox" name="checkFileModified">      == oFileChangedW
<widget class="QCheckBox" name="checkFileReadOnly">      == oFileReadOnlyW
<widget class="QCheckBox" name="checkFileLocked">        == oFileLockedW
<widget class="QCheckBox" name="checkServerNamePresent"> == oServerNameW
<widget class="QCheckBox" name="checkClearCasePresent">  == oCcViewTagW
<widget class="QCheckBox" name="checkServerEqualsCC">    == oServerEqualViewW
<widget class="QCheckBox" name="checkDirectoryPresent">  == oDirW
#endif


//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
DialogWindowTitle::DialogWindowTitle(Document *window, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);
	
	inConstructor_ = true;
	
	QValidator *validator = new QRegExpValidator(QRegExp(QLatin1String("[0-9]")), this);
	ui.editDirectory->setValidator(validator);

	/* copy attributes from current this so that we can use as many
	 * 'real world' defaults as possible when testing the effect
	 * of different formatting strings.
	 */	
	path_        = QString::fromStdString(window->path_);
	filename_    = QString::fromStdString(window->filename_);
	
	QString clearCase = GetClearCaseViewTag();
	
	viewTag_     = !clearCase.isNull() ? clearCase : QLatin1String("viewtag");
	serverName_  = IsServer ? QLatin1String(GetPrefServerName()) : QLatin1String("servername");
	isServer_    = IsServer;
	filenameSet_ = window->filenameSet_;
	lockReasons_ = window->lockReasons_;
	fileChanged_ = window->fileChanged_;
	
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
	ui.checkFileReadOnly->setChecked(IS_PERM_LOCKED(lockReasons_));
	ui.checkFileLocked->setChecked(IS_USER_LOCKED(lockReasons_));
	// Read-only takes precedence on locked 
	ui.checkFileLocked->setEnabled(!IS_PERM_LOCKED(lockReasons_));

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
		filenameSet_ ? path_.toLatin1().data() : "/a/very/long/path/used/as/example/", 
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
QString DialogWindowTitle::FormatWindowTitle(const QString &filename, const char *path, const QString &clearCaseViewTag, const QString &serverName, int isServer, int filenameSet, int lockReasons, int fileChanged, const QString &titleFormat) {
	QString title;

	// Flags to supress one of these if both are specified and they are identical 
	bool serverNameSeen = false;
	bool clearCaseViewTagSeen = false;

	int noOfComponents     = -1;

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
				if (!clearCaseViewTag.isNull()) {
					if (!serverNameSeen || serverName != clearCaseViewTag) {
						title.append(clearCaseViewTag);
						clearCaseViewTagSeen = true;
					}
				}
				break;

			case 's': // server name 
				if (isServer && !serverName.isEmpty()) { // only applicable for servers 
					if (!clearCaseViewTagSeen || serverName != clearCaseViewTag) {
						title.append(serverName);
						serverNameSeen = true;
					}
				}
				break;

			case 'd': // directory without any limit to no. of components 
				if (filenameSet) {
					title.append(QLatin1String(path));
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
					noOfComponents = c.toLatin1() - '0';
					format_it++; // delete the argument 

					if (filenameSet) {
						const char *trailingPath = GetTrailingPathComponents(path, noOfComponents);

						// prefix with ellipsis if components were skipped 
						if (trailingPath > path) {
							title.append(QLatin1String("..."));
						}
						title.append(QLatin1String(trailingPath));
					}
				}
				break;

			case 'f': // file name 
				title.append(filename);
				break;

			case 'h': // host name 
				title.append(GetNameOfHostEx());
				break;

			case 'S': // file status 
				if (IS_ANY_LOCKED_IGNORING_USER(lockReasons) && fileChanged)
					title.append(tr("read only, modified"));
				else if (IS_ANY_LOCKED_IGNORING_USER(lockReasons))
					title.append(tr("read only"));
				else if (IS_USER_LOCKED(lockReasons) && fileChanged)
					title.append(tr("locked, modified"));
				else if (IS_USER_LOCKED(lockReasons))
					title.append(tr("locked"));
				else if (fileChanged)
					title.append(tr("modified"));
				break;

			case 'u': // user name 
				title.append(GetUserNameEx());
				break;

			case '%': // escaped % 
				title.append(QLatin1Char('%'));
				break;

			case '*': // short file status ? 
				if (format_it != titleFormat.end() && *format_it == QLatin1Char('S')) {
					++format_it;
					if (IS_ANY_LOCKED_IGNORING_USER(lockReasons) && fileChanged)
						title.append(tr("RO*"));
					else if (IS_ANY_LOCKED_IGNORING_USER(lockReasons))
						title.append(tr("RO"));
					else if (IS_USER_LOCKED(lockReasons) && fileChanged)
						title.append(tr("LO*"));
					else if (IS_USER_LOCKED(lockReasons))
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

	title = compressWindowTitle(title);

	if (title.isEmpty()) {
		title = tr("<empty>"); // For preview purposes only 
	}

	return title;
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
QString DialogWindowTitle::FormatWindowTitleEx(const QString &filename, const char *path, const QString &clearCaseViewTag, const QString &serverName, int isServer, int filenameSet, int lockReasons, int fileChanged, const QString &titleFormat) {

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
					title.append(QLatin1String(path));
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
						const char *trailingPath = GetTrailingPathComponents(path, noOfComponents);

						// prefix with ellipsis if components were skipped 
						if (trailingPath > path) {
							title.append(QLatin1String("..."));
						}
						title.append(QLatin1String(trailingPath));
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
				if (IS_ANY_LOCKED_IGNORING_USER(lockReasons) && fileChanged)
					title.append(tr("read only, modified"));
				else if (IS_ANY_LOCKED_IGNORING_USER(lockReasons))
					title.append(tr("read only"));
				else if (IS_USER_LOCKED(lockReasons) && fileChanged)
					title.append(tr("locked, modified"));
				else if (IS_USER_LOCKED(lockReasons))
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
					if (IS_ANY_LOCKED_IGNORING_USER(lockReasons) && fileChanged)
						title.append(tr("RO*"));
					else if (IS_ANY_LOCKED_IGNORING_USER(lockReasons))
						title.append(tr("RO"));
					else if (IS_USER_LOCKED(lockReasons) && fileChanged)
						title.append(tr("LO*"));
					else if (IS_USER_LOCKED(lockReasons))
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

	title = compressWindowTitle(title);

	if (title.isEmpty()) {
		title = tr("<empty>"); // For preview purposes only 
	}

	// Prevent recursive callback loop 
	suppressFormatUpdate_ = true;

	/* Sync radio buttons with format string (in case the user entered
	   the format manually) */
	ui.checkFileName->setChecked(fileNamePresent);
	ui.checkFileStatus->setChecked(fileStatusPresent);
	ui.checkServerName->setChecked(serverNamePresent);
	ui.checkClearCase->setChecked(clearCasePresent);

	ui.checkDirectory->setChecked(dirNamePresent);
	ui.checkHostName->setChecked(hostNamePresent);
	ui.checkUserName->setChecked(userNamePresent);

	ui.checkBrief->setEnabled(fileStatusPresent);


	if (fileStatusPresent) {
		ui.checkBrief->setChecked(shortStatus);
	}

	// Directory components are also sensitive to presence of dir 
	ui.editDirectory->setEnabled(dirNamePresent);
	ui.labelDirectory->setEnabled(dirNamePresent);

	// Avoid erasing number when not active 
	if (dirNamePresent)  {

		if (noOfComponents >= 0) {

			QString value = ui.editDirectory->text();
			QString comp  = tr("%1").arg(noOfComponents);

			// Don't overwrite unless diff. 
			if (value != comp) {
				ui.editDirectory->setText(comp);
			}
		} else {
			ui.editDirectory->setText(tr(""));
		}
	}

	// Enable/disable test buttons, depending on presence of codes 
	ui.checkFileModified->setEnabled(fileStatusPresent);
	ui.checkFileReadOnly->setEnabled(fileStatusPresent);
	ui.checkFileLocked->setEnabled(fileStatusPresent && !IS_PERM_LOCKED(lockReasons_));
	ui.checkServerNamePresent->setEnabled(serverNamePresent);
	ui.checkClearCasePresent->setEnabled(clearCasePresent);
	ui.checkServerEqualsCC->setEnabled(clearCasePresent && serverNamePresent);
	ui.checkDirectoryPresent->setEnabled(dirNamePresent);
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
	SET_PERM_LOCKED(lockReasons_, checked);
	formatChangedCB();
}

//------------------------------------------------------------------------------
// Name: 
//------------------------------------------------------------------------------
void DialogWindowTitle::on_checkFileLocked_toggled(bool checked) {
	SET_USER_LOCKED(lockReasons_, checked);
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
