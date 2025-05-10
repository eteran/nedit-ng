
#include "DialogWindowTitle.h"
#include "DocumentWidget.h"
#include "Help.h"
#include "Preferences.h"
#include "SignalBlocker.h"
#include "Util/ClearCase.h"
#include "Util/FileSystem.h"
#include "Util/Host.h"
#include "Util/User.h"
#include "nedit.h"

#include <QRegularExpression>
#include <QRegularExpressionValidator>

namespace {

/**
 * @brief Cleans up a window title.
 * Remove empty parenthesis pairs.
 * Replace multiple spaces in a row with one space.
 * Remove leading and trailing spaces and dashes.
 *
 * @param title The title to compress.
 * @return A compressed version of the title.
 */
QString CompressWindowTitle(const QString &title) {

	QString result = title;

	// remove empty brackets
	result.replace(QStringLiteral("()"), QString());
	result.replace(QStringLiteral("{}"), QString());
	result.replace(QStringLiteral("[]"), QString());

	// remove leading/trailing whitespace/dashes
	static const QRegularExpression regex(QStringLiteral("((^[\\s-]+)|([\\s-]+$))"));
	result.replace(regex, QString());

	return result.simplified();
}

}

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

/**
 * @brief Constructor for the DialogWindowTitle class.
 *
 * @param document The DocumentWidget associated with this dialog, which provides the context for the title formatting.
 * @param parent The parent widget for this dialog, defaults to nullptr.
 * @param f The window flags for the dialog, defaults to Qt::WindowFlags().
 */
DialogWindowTitle::DialogWindowTitle(DocumentWidget *document, QWidget *parent, Qt::WindowFlags f)
	: Dialog(parent, f) {
	ui.setupUi(this);
	connectSlots();

	Dialog::shrinkToFit(this);

	static const QRegularExpression rx(QStringLiteral("[0-9]"));
	ui.editDirectory->setValidator(new QRegularExpressionValidator(rx, this));

	/* copy attributes from current this so that we can use as many
	 * 'real world' defaults as possible when testing the effect
	 * of different formatting strings.
	 */
	path_     = document->path();
	filename_ = document->filename();

	const QString clearCase = ClearCase::GetViewTag();

	viewTag_     = !clearCase.isNull() ? clearCase : tr("viewtag");
	serverName_  = IsServer ? Preferences::GetPrefServerName() : tr("servername");
	isServer_    = IsServer;
	filenameSet_ = document->filenameSet();
	lockReasons_ = document->lockReasons();
	fileChanged_ = document->fileChanged();

	ui.checkClearCasePresent->setChecked(!clearCase.isNull());

	suppressFormatUpdate_ = false;

	ui.editFormat->setText(Preferences::GetPrefTitleFormat());

	// force update of the dialog
	setToggleButtons();

	formatChangedCB();
}

/**
 * @brief Connects the slots for the dialog's buttons and other UI elements.
 */
void DialogWindowTitle::connectSlots() {
	connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &DialogWindowTitle::buttonBox_clicked);
	connect(ui.checkFileName, &QCheckBox::toggled, this, &DialogWindowTitle::checkFileName_toggled);
	connect(ui.checkHostName, &QCheckBox::toggled, this, &DialogWindowTitle::checkHostName_toggled);
	connect(ui.checkFileStatus, &QCheckBox::toggled, this, &DialogWindowTitle::checkFileStatus_toggled);
	connect(ui.checkBrief, &QCheckBox::toggled, this, &DialogWindowTitle::checkBrief_toggled);
	connect(ui.checkUserName, &QCheckBox::toggled, this, &DialogWindowTitle::checkUserName_toggled);
	connect(ui.checkClearCase, &QCheckBox::toggled, this, &DialogWindowTitle::checkClearCase_toggled);
	connect(ui.checkServerName, &QCheckBox::toggled, this, &DialogWindowTitle::checkServerName_toggled);
	connect(ui.checkDirectory, &QCheckBox::toggled, this, &DialogWindowTitle::checkDirectory_toggled);
	connect(ui.checkFileModified, &QCheckBox::toggled, this, &DialogWindowTitle::checkFileModified_toggled);
	connect(ui.checkFileReadOnly, &QCheckBox::toggled, this, &DialogWindowTitle::checkFileReadOnly_toggled);
	connect(ui.checkFileLocked, &QCheckBox::toggled, this, &DialogWindowTitle::checkFileLocked_toggled);
	connect(ui.checkServerNamePresent, &QCheckBox::toggled, this, &DialogWindowTitle::checkServerNamePresent_toggled);
	connect(ui.checkClearCasePresent, &QCheckBox::toggled, this, &DialogWindowTitle::checkClearCasePresent_toggled);
	connect(ui.checkServerEqualsCC, &QCheckBox::toggled, this, &DialogWindowTitle::checkServerEqualsCC_toggled);
	connect(ui.checkDirectoryPresent, &QCheckBox::toggled, this, &DialogWindowTitle::checkDirectoryPresent_toggled);
	connect(ui.editDirectory, &QLineEdit::textChanged, this, &DialogWindowTitle::editDirectory_textChanged);
	connect(ui.editFormat, &QLineEdit::textChanged, this, &DialogWindowTitle::editFormat_textChanged);
}

/**
 * @brief Sets the values of all toggle buttons
 */
void DialogWindowTitle::setToggleButtons() {

	ui.checkDirectoryPresent->setChecked(filenameSet_);
	ui.checkFileModified->setChecked(fileChanged_);
	ui.checkFileReadOnly->setChecked(lockReasons_.isPermLocked());
	ui.checkFileLocked->setChecked(lockReasons_.isUserLocked());
	// Read-only takes precedence on locked
	ui.checkFileLocked->setEnabled(!lockReasons_.isPermLocked());

	const QString ccTag = ClearCase::GetViewTag();

	ui.checkClearCasePresent->setChecked(!ccTag.isNull());
	ui.checkServerNamePresent->setChecked(isServer_);

	if (!ccTag.isNull() && isServer_ && !Preferences::GetPrefServerName().isEmpty() && ccTag == Preferences::GetPrefServerName()) {
		ui.checkServerEqualsCC->setChecked(true);
	} else {
		ui.checkServerEqualsCC->setChecked(false);
	}
}

/**
 * @brief Handles the text change event for the format edit field.
 *
 * @param text The new text in the format edit field.
 */
void DialogWindowTitle::editFormat_textChanged(const QString &text) {
	Q_UNUSED(text)
	formatChangedCB();
}

/**
 * @brief Handles the text change event for the directory edit field.
 */
void DialogWindowTitle::formatChangedCB() {

	const bool filenameSet = ui.checkDirectoryPresent->isChecked();

	if (suppressFormatUpdate_) {
		return; // Prevent recursive feedback
	}

	const QString format = ui.editFormat->text();
	QString serverName;
	if (ui.checkServerEqualsCC->isChecked() && ui.checkClearCasePresent->isChecked()) {
		serverName = viewTag_;
	} else {
		serverName = ui.checkServerNamePresent->isChecked() ? serverName_ : QString();
	}

	const QString title = formatWindowTitleAndUpdate(
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

/**
 * @brief Formats the window title based on the provided parameters.
 *
 * @param document The DocumentWidget containing the document information.
 * @param clearCaseViewTag The ClearCase view tag to use in the title.
 * @param serverName The server name to use in the title.
 * @param isServer Indicates if we are in server mode.
 * @param titleFormat The format string to use for the title.
 * @return The formatted window title.
 */
QString DialogWindowTitle::formatWindowTitle(DocumentWidget *document, const QString &clearCaseViewTag, const QString &serverName, bool isServer, const QString &format) {
	return formatWindowTitleInternal(
		document->filename(),
		document->path(),
		clearCaseViewTag,
		serverName,
		isServer,
		document->filenameSet(),
		document->lockReasons(),
		document->fileChanged(),
		format,
		nullptr);
}

/**
 * @brief Format the windows title using a printf like formatting string.
 *
 *  The following flags are recognised:
 *  %c    : ClearCase view tag
 *  %s    : server name
 *  %[n]d : directory, with one optional digit specifying the max number
 *          of trailing directory components to display. Skipped components are
 *          replaced by an ellipsis (...).
 *  %f    : file name
 *  %h    : host name
 *  %S    : file status
 *  %u    : user name
 *
 * @param filename The name of the file to be displayed in the title.
 * @param path The path to the file, used for directory components.
 * @param clearCaseViewTag The ClearCase view tag to use in the title, if applicable.
 * @param serverName The server name to use in the title, if applicable.
 * @param isServer Indicates if we are in server mode.
 * @param filenameSet Indicates if the filename is set.
 * @param lockReasons The lock reasons for the file, used to determine its status.
 * @param fileChanged Indicates if the file has been changed.
 * @param titleFormat The format string to use for the title, which may contain placeholders for the various components.
 * @return The formatted window title.
 *
 * @note If the ClearCase view tag and server name are identical, only the first one
 *       specified in the formatting string will be displayed.
 */
QString DialogWindowTitle::formatWindowTitleAndUpdate(const QString &filename, const QString &path, const QString &clearCaseViewTag, const QString &serverName, bool isServer, bool filenameSet, LockReasons lockReasons, bool fileChanged, const QString &titleFormat) {

	UpdateState state;
	QString title = formatWindowTitleInternal(
		filename,
		path,
		clearCaseViewTag,
		serverName,
		isServer,
		filenameSet,
		lockReasons,
		fileChanged,
		titleFormat,
		&state);

	// Prevent recursive callback loop
	suppressFormatUpdate_ = true;

	/* Sync radio buttons with format string (in case the user entered
	   the format manually) */
	no_signals(ui.checkFileName)->setChecked(state.fileNamePresent);
	no_signals(ui.checkFileStatus)->setChecked(state.fileStatusPresent);
	no_signals(ui.checkServerName)->setChecked(state.serverNamePresent);
	no_signals(ui.checkClearCase)->setChecked(state.clearCasePresent);

	no_signals(ui.checkDirectory)->setChecked(state.dirNamePresent);
	no_signals(ui.checkHostName)->setChecked(state.hostNamePresent);
	no_signals(ui.checkUserName)->setChecked(state.userNamePresent);

	ui.checkBrief->setEnabled(state.fileStatusPresent);

	if (state.fileStatusPresent) {
		no_signals(ui.checkBrief)->setChecked(state.shortStatus);
	}

	// Directory components are also sensitive to presence of dir
	ui.editDirectory->setEnabled(state.dirNamePresent);
	ui.labelDirectory->setEnabled(state.dirNamePresent);

	// Avoid erasing number when not active
	if (state.dirNamePresent) {

		if (state.noOfComponents >= 0) {

			const QString value = ui.editDirectory->text();
			const auto comp     = QString::number(state.noOfComponents);

			// Don't overwrite unless diff.
			if (value != comp) {
				ui.editDirectory->setText(comp);
			}
		} else {
			ui.editDirectory->setText(QString());
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

/**
 * @brief Handler for the filename toggle button.
 *
 * @param checked Indicates whether the filename checkbox is checked.
 */
void DialogWindowTitle::checkFileName_toggled(bool checked) {
	if (checked) {
		appendToFormat(QStringLiteral(" %f"));
	} else {
		removeFromFormat(QStringLiteral("%f"));
	}
}

/**
 * @brief Handler for the hostname toggle button.
 *
 * @param checked Indicates whether the hostname checkbox is checked.
 */
void DialogWindowTitle::checkHostName_toggled(bool checked) {
	if (checked) {
		appendToFormat(QStringLiteral(" [%h]"));
	} else {
		removeFromFormat(QStringLiteral("%h"));
	}
}

/**
 * @brief Handler for the file status toggle button.
 *
 * @param checked Indicates whether the file status checkbox is checked.
 */
void DialogWindowTitle::checkFileStatus_toggled(bool checked) {
	ui.checkBrief->setEnabled(checked);

	if (checked) {
		if (ui.checkBrief->isChecked()) {
			appendToFormat(QStringLiteral(" (%*S)"));
		} else {
			appendToFormat(QStringLiteral(" (%S)"));
		}
	} else {
		removeFromFormat(QStringLiteral("%S"));
		removeFromFormat(QStringLiteral("%*S"));
	}
}

/**
 * @brief Handler for the brief toggle button.
 *
 * @param checked Indicates whether the brief checkbox is checked.
 */
void DialogWindowTitle::checkBrief_toggled(bool checked) {

	if (suppressFormatUpdate_) {
		return;
	}

	QString format = ui.editFormat->text();

	if (checked) {
		// Find all %S occurrences and replace them by %*S
		format.replace(QStringLiteral("%S"), QStringLiteral("%*S"));
	} else {
		// Replace all %*S occurrences by %S
		format.replace(QStringLiteral("%*S"), QStringLiteral("%S"));
	}

	ui.editFormat->setText(format);
}

/**
 * @brief Handler for the username toggle button.
 *
 * @param checked Indicates whether the username checkbox is checked.
 */
void DialogWindowTitle::checkUserName_toggled(bool checked) {
	if (checked) {
		appendToFormat(QStringLiteral(" %u"));
	} else {
		removeFromFormat(QStringLiteral("%u"));
	}
}

/**
 * @brief Handler for the ClearCase toggle button.
 *
 * @param checked Indicates whether the ClearCase checkbox is checked.
 */
void DialogWindowTitle::checkClearCase_toggled(bool checked) {
	if (checked) {
		appendToFormat(QStringLiteral(" {%c}"));
	} else {
		removeFromFormat(QStringLiteral("%c"));
	}
}

/**
 * @brief Handler for the server name toggle button.
 *
 * @param checked Indicates whether the server name checkbox is checked.
 */
void DialogWindowTitle::checkServerName_toggled(bool checked) {
	if (checked) {
		appendToFormat(QStringLiteral(" [%s]"));
	} else {
		removeFromFormat(QStringLiteral("%s"));
	}
}

/**
 * @brief Handler for the directory toggle button.
 *
 * @param checked Indicates whether the directory checkbox is checked.
 */
void DialogWindowTitle::checkDirectory_toggled(bool checked) {

	ui.editDirectory->setEnabled(checked);
	ui.labelDirectory->setEnabled(checked);

	if (checked) {
		QString buf;
		const QString value = ui.editDirectory->text();
		if (!value.isEmpty()) {

			bool ok;
			const int maxComp = value.toInt(&ok);

			if (ok && maxComp > 0) {
				buf = tr(" %%1d ").arg(maxComp);
			} else {
				buf = tr(" %d "); // Should not be necessary
			}
		} else {
			buf = tr(" %d ");
		}
		appendToFormat(buf);
	} else {
		removeFromFormat(QStringLiteral("%d"));
		for (int i = 1; i < 10; ++i) {
			removeFromFormat(tr("%%1d").arg(i));
		}
	}
}

/**
 * @brief Handler for the file modified toggle button.
 *
 * @param checked Indicates whether the file modified checkbox is checked.
 */
void DialogWindowTitle::checkFileModified_toggled(bool checked) {
	fileChanged_ = checked;
	formatChangedCB();
}

/**
 * @brief Handler for the file read-only toggle button.
 *
 * @param checked Indicates whether the file read-only checkbox is checked.
 */
void DialogWindowTitle::checkFileReadOnly_toggled(bool checked) {
	lockReasons_.setPermLocked(checked);
	formatChangedCB();
}

/**
 * @brief Handler for the file locked toggle button.
 *
 * @param checked Indicates whether the file locked checkbox is checked.
 */
void DialogWindowTitle::checkFileLocked_toggled(bool checked) {
	lockReasons_.setUserLocked(checked);
	formatChangedCB();
}

/**
 * @brief Handler for the server name present toggle button.
 *
 * @param checked Indicates whether the server name present checkbox is checked.
 */
void DialogWindowTitle::checkServerNamePresent_toggled(bool checked) {

	if (!checked) {
		ui.checkServerEqualsCC->setChecked(false);
	}

	isServer_ = checked;

	formatChangedCB();
}

/**
 * @brief Handler for the ClearCase present toggle button.
 *
 * @param checked Indicates whether the ClearCase present checkbox is checked.
 */
void DialogWindowTitle::checkClearCasePresent_toggled(bool checked) {
	if (!checked) {
		ui.checkServerEqualsCC->setChecked(false);
	}

	formatChangedCB();
}

/**
 * @brief Handler for the directory present toggle button.
 *
 * @param checked Indicates whether the directory present checkbox is checked.
 */
void DialogWindowTitle::checkDirectoryPresent_toggled(bool checked) {
	Q_UNUSED(checked)
	formatChangedCB();
}

/**
 * @brief Handler for the server equals ClearCase toggle button.
 *
 * @param checked Indicates whether the server equals ClearCase checkbox is checked.
 */
void DialogWindowTitle::checkServerEqualsCC_toggled(bool checked) {
	if (checked) {
		ui.checkClearCasePresent->setChecked(true);
		ui.checkServerNamePresent->setChecked(true);
		isServer_ = true;
	}
	formatChangedCB();
}

/**
 * @brief Appends a string to the format edit field.
 *
 * @param string The string to append to the format.
 */
void DialogWindowTitle::appendToFormat(const QString &string) {
	const QString format = ui.editFormat->text();
	ui.editFormat->setText(format + string);
}

/**
 * @brief Removes a string from the format edit field.
 *
 * @param string The string to remove from the format.
 */
void DialogWindowTitle::removeFromFormat(const QString &string) {

	QString format = ui.editFormat->text();

	// If the string is preceded or followed by a brace, include
	// the brace(s) for removal

	// NOTE(eteran): this one can't be static because it is parameterized
	const QRegularExpression re1(QStringLiteral(R"([\{\(\[\<]?%1[\}\)\]\>]?)").arg(QRegularExpression::escape(string)));
	format.replace(re1, QString());

	// remove leading/trailing whitespace/dashes
	static const QRegularExpression re2(QStringLiteral("^[\\s]+"));
	static const QRegularExpression re3(QStringLiteral("[\\s]+$"));
	format.replace(re2, QString());
	format.replace(re3, QString());

	ui.editFormat->setText(format);
}

/**
 * @brief Handles the button box click event.
 *
 * @param button The button that was clicked in the button box.
 */
void DialogWindowTitle::buttonBox_clicked(QAbstractButton *button) {
	if (ui.buttonBox->standardButton(button) == QDialogButtonBox::Apply) {

		const QString format = ui.editFormat->text();

		if (format != Preferences::GetPrefTitleFormat()) {
			Preferences::SetPrefTitleFormat(format);
		}

	} else if (ui.buttonBox->standardButton(button) == QDialogButtonBox::RestoreDefaults) {
		ui.editFormat->setText(QStringLiteral("{%c} [%s] %f (%S) - %d"));
	} else if (ui.buttonBox->standardButton(button) == QDialogButtonBox::Help) {
		Help::displayTopic(Help::Topic::CustomTitleDialog);
	}
}

/**
 * @brief Handles the text change event for the directory edit field.
 *
 * @param text The new text in the directory edit field.
 */
void DialogWindowTitle::editDirectory_textChanged(const QString &text) {

	if (suppressFormatUpdate_) {
		return;
	}

	QString format = ui.editFormat->text();
	bool ok;
	const int maxComp = text.toInt(&ok);

	static const QRegularExpression re(QStringLiteral("%[0-9]?d"));

	if (ok && maxComp > 0) {
		format.replace(re, tr("%%1d").arg(maxComp));
	} else {
		format.replace(re, tr("%d"));
	}

	ui.editFormat->setText(format);
}

/**
 * @brief Formats the window title based on the provided parameters.
 *
 * @param filename The name of the file to be displayed in the title.
 * @param path The path to the file, used for directory components.
 * @param clearCaseViewTag The ClearCase view tag to use in the title, if applicable.
 * @param serverName The server name to use in the title, if applicable.
 * @param isServer Indicates if we are in server mode.
 * @param filenameSet Indicates if the filename is set.
 * @param lockReasons The lock reasons for the file, used to determine its status.
 * @param fileChanged Indicates if the file has been changed.
 * @param titleFormat The format string to use for the title, which may contain placeholders for the various components.
 * @param state An UpdateState structure to store the state of the title components.
 * @return The formatted window title.
 */
QString DialogWindowTitle::formatWindowTitleInternal(const QString &filename, const QString &path, const QString &clearCaseViewTag, const QString &serverName, bool isServer, bool filenameSet, LockReasons lockReasons, bool fileChanged, const QString &format, UpdateState *state) {
	QString title;

	// Flags to suppress one of these if both are specified and they are identical
	bool serverNameSeen       = false;
	bool clearCaseViewTagSeen = false;

	bool fileNamePresent   = false;
	bool hostNamePresent   = false;
	bool userNamePresent   = false;
	bool serverNamePresent = false;
	bool clearCasePresent  = false;
	bool fileStatusPresent = false;
	bool dirNamePresent    = false;
	bool shortStatus       = false;
	int noOfComponents     = -1;

	auto format_it = format.begin();

	while (format_it != format.end()) {
		QChar c = *format_it++;
		if (c == QLatin1Char('%')) {

			if (format_it == format.end()) {
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
					noOfComponents = c.digitValue();
					format_it++; // delete the argument

					if (filenameSet) {
						const QString trailingPath = GetTrailingPathComponents(path, noOfComponents);

						// prefix with ellipsis if components were skipped
						if (trailingPath != path) {
							title.append(QStringLiteral("..."));
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
				title.append(GetNameOfHost());
				break;

			case 'S': // file status
				fileStatusPresent = true;
				if (lockReasons.isAnyLockedIgnoringUser() && fileChanged) {
					title.append(tr("read only, modified"));
				} else if (lockReasons.isAnyLockedIgnoringUser()) {
					title.append(tr("read only"));
				} else if (lockReasons.isUserLocked() && fileChanged) {
					title.append(tr("locked, modified"));
				} else if (lockReasons.isUserLocked()) {
					title.append(tr("locked"));
				} else if (fileChanged) {
					title.append(tr("modified"));
				}
				break;

			case 'u': // user name
				userNamePresent = true;
				title.append(GetUser());
				break;

			case '%': // escaped %
				title.append(QLatin1Char('%'));
				break;

			case '*': // short file status ?
				fileStatusPresent = true;
				if (format_it != format.end() && *format_it == QLatin1Char('S')) {
					++format_it;
					shortStatus = true;
					if (lockReasons.isAnyLockedIgnoringUser() && fileChanged) {
						title.append(tr("RO*"));
					} else if (lockReasons.isAnyLockedIgnoringUser()) {
						title.append(tr("RO"));
					} else if (lockReasons.isUserLocked() && fileChanged) {
						title.append(tr("LO*"));
					} else if (lockReasons.isUserLocked()) {
						title.append(tr("LO"));
					} else if (fileChanged) {
						title.append(tr("*"));
					}
					break;
				}
				title.append(c);
				break;
			default:
				title.append(c);
				break;
			}
		} else {
			title.append(c);
		}
	}

	title = CompressWindowTitle(title);

	if (title.isEmpty()) {
		title = tr("<empty>"); // For preview purposes only
	}

	if (state) {
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
