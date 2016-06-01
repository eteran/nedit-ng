/*******************************************************************************
*                                                                              *
* shell.c -- Nirvana Editor shell command execution                            *
*                                                                              *
* Copyright (C) 1999 Mark Edel                                                 *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute version of this program linked to   *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* December, 1993                                                               *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include <QApplication>
#include <QMessageBox>
#include <QPushButton>

#include "textP.h"
#include "TextDisplay.h"
#include "shell.h"
#include "Document.h"
#include "MenuItem.h"
#include "MotifHelper.h"
#include "TextBuffer.h"
#include "file.h"
#include "interpret.h"
#include "macro.h"
#include "menu.h"
#include "misc.h"
#include "nedit.h"
#include "preferences.h"
#include "text.h"
#include "window.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <Xm/PushBG.h>

namespace {

// Tuning parameters 
const int IO_BUF_SIZE         = 4096; // size of buffers for collecting cmd output 
const int MAX_OUT_DIALOG_ROWS = 30;   // max height of dialog for command output 
const int MAX_OUT_DIALOG_COLS = 80;   // max width of dialog for command output 
const int OUTPUT_FLUSH_FREQ	  = 1000; // how often (msec) to flush output buffers when process is taking too long 
const int BANNER_WAIT_TIME	  = 6000; // how long to wait (msec) before putting up Shell Command Executing... banner 

// flags for issueCommand
enum {
	ACCUMULATE  	  = 1,
	ERROR_DIALOGS	  = 2,
	REPLACE_SELECTION = 4,
	RELOAD_FILE_AFTER = 8,
	OUTPUT_TO_DIALOG  = 16,
	OUTPUT_TO_STRING  = 32
};

}

// element of a buffer list for collecting output from shell processes 
struct bufElem {
	int length;
	char contents[IO_BUF_SIZE];
};

/* data attached to window during shell command execution with
   information for controling and communicating with the process */
struct shellCmdInfo {
	int flags;
	int stdinFD;
	int stdoutFD;
	int stderrFD;
	pid_t childPid;
	XtInputId stdinInputID;
	XtInputId stdoutInputID;
	XtInputId stderrInputID;
	std::list<bufElem *> outBufs;
	std::list<bufElem *> errBufs;
	std::string input;
	int inIndex;
	Widget textW;
	int leftPos;
	int rightPos;
	int inLength;
	XtIntervalId bannerTimeoutID, flushTimeoutID;
	char bannerIsUp;
	char fromMacro;
};

static void issueCommand(Document *window, const std::string &command, const std::string &input, int flags, Widget textW, int replaceLeft, int replaceRight, int fromMacro);
static void stdoutReadProc(XtPointer clientData, int *source, XtInputId *id);
static void stderrReadProc(XtPointer clientData, int *source, XtInputId *id);
static void stdinWriteProc(XtPointer clientData, int *source, XtInputId *id);
static void finishCmdExecution(Document *window, int terminatedOnError);
static pid_t forkCommand(Widget parent, const std::string &command, const std::string &cmdDir, int *stdinFD, int *stdoutFD, int *stderrFD);
static std::string coalesceOutputEx(std::list<bufElem *> &bufList);
static void freeBufListEx(std::list<bufElem *> &bufList);
static void removeTrailingNewlines(std::string &string);
static void createOutputDialog(Widget parent, const std::string &text);
static void destroyOutDialogCB(Widget w, XtPointer callback, XtPointer closure);
static void measureText(view::string_view text, int wrapWidth, int *rows, int *cols, int *wrapped);
static void truncateString(std::string &string, int length);
static void bannerTimeoutProc(XtPointer clientData, XtIntervalId *id);
static void flushTimeoutProc(XtPointer clientData, XtIntervalId *id);
static void safeBufReplace(TextBuffer *buf, int *start, int *end, const std::string &text);
static char *shellCommandSubstitutes(const char *inStr, const char *fileStr, const char *lineStr);
static int shellSubstituter(char *outStr, const char *inStr, const char *fileStr, const char *lineStr, int outLen, int predictOnly);

/*
** Filter the current selection through shell command "command".  The selection
** is removed, and replaced by the output from the command execution.  Failed
** command status and output to stderr are presented in dialog form.
*/
void FilterSelection(Document *window, const std::string &command, int fromMacro) {

	// Can't do two shell commands at once in the same window 
	if (window->shellCmdData_) {
		QApplication::beep();
		return;
	}

	/* Get the selection and the range in character positions that it
	   occupies.  Beep and return if no selection */
	std::string text = window->buffer_->BufGetSelectionTextEx();
	if (text.empty()) {
		QApplication::beep();
		return;
	}
	window->buffer_->BufUnsubstituteNullCharsEx(text);
	int left  = window->buffer_->primary_.start;
	int right = window->buffer_->primary_.end;

	// Issue the command and collect its output 
	issueCommand(window, command, text, ACCUMULATE | ERROR_DIALOGS | REPLACE_SELECTION, window->lastFocus_, left, right, fromMacro);
}

/*
** Execute shell command "command", depositing the result at the current
** insert position or in the current selection if the window has a
** selection.
*/
void ExecShellCommand(Document *window, const std::string &command, int fromMacro) {
	int left;
	int right;
	int flags = 0;
	char *subsCommand;
	int pos;
	int line;
	int column;
	char lineNumber[11];

	// Can't do two shell commands at once in the same window 
	if (window->shellCmdData_) {
		QApplication::beep();
		return;
	}
	
	auto textD = reinterpret_cast<TextWidget>(window->lastFocus_)->text.textD;
	

	// get the selection or the insert position 
	pos = textD->TextGetCursorPos();
	
	if (window->buffer_->GetSimpleSelection(&left, &right)) {
		flags = ACCUMULATE | REPLACE_SELECTION;
	} else {
		left = right = pos;
	}

	/* Substitute the current file name for % and the current line number
	   for # in the shell command */
	QString fullName = window->FullPath();
	
	textD->TextDPosToLineAndCol(pos, &line, &column);
	sprintf(lineNumber, "%d", line);

	subsCommand = shellCommandSubstitutes(command.c_str(), fullName.toLatin1().data(), lineNumber);
	if(!subsCommand) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Shell Command"), QLatin1String("Shell command is too long due to\n"
		                                                   "filename substitutions with '%%' or\n"
		                                                   "line number substitutions with '#'"));
		return;
	}

	// issue the command 
	issueCommand(window, subsCommand, std::string(), flags, window->lastFocus_, left, right, fromMacro);
	delete [] subsCommand;
}

/*
** Execute shell command "command", on input string "input", depositing the
** in a macro string (via a call back to ReturnShellCommandOutput).
*/
void ShellCmdToMacroString(Document *window, const std::string &command, const std::string &input) {

	// fork the command and begin processing input/output 
	issueCommand(window, command, input, ACCUMULATE | OUTPUT_TO_STRING, nullptr, 0, 0, True);
}

/*
** Execute the line of text where the the insertion cursor is positioned
** as a shell command.
*/
void ExecCursorLine(Document *window, int fromMacro) {
	int left;
	int right;
	int insertPos;
	char *subsCommand;
	int line;
	int column;
	char lineNumber[11];

	// Can't do two shell commands at once in the same window 
	if (window->shellCmdData_) {
		QApplication::beep();
		return;
	}
	
	auto textD = reinterpret_cast<TextWidget>(window->lastFocus_)->text.textD;

	// get all of the text on the line with the insert position 
	int pos = textD->TextGetCursorPos();
	
	if (!window->buffer_->GetSimpleSelection(&left, &right)) {
		left = right = pos;
		left = window->buffer_->BufStartOfLine(left);
		right = window->buffer_->BufEndOfLine( right);
		insertPos = right;
	} else
		insertPos = window->buffer_->BufEndOfLine( right);
	std::string cmdText = window->buffer_->BufGetRangeEx(left, right);
	window->buffer_->BufUnsubstituteNullCharsEx(cmdText);

	// insert a newline after the entire line 
	window->buffer_->BufInsertEx(insertPos, "\n");

	/* Substitute the current file name for % and the current line number
	   for # in the shell command */
	QString fullName = QString(QLatin1String("%1%2")).arg(window->path_).arg(window->filename_);
	
	textD->TextDPosToLineAndCol(pos, &line, &column);
	sprintf(lineNumber, "%d", line);

	subsCommand = shellCommandSubstitutes(cmdText.c_str(), fullName.toLatin1().data(), lineNumber);
	if(!subsCommand) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Shell Command"), QLatin1String("Shell command is too long due to\n"
		                                                   "filename substitutions with '%%' or\n"
		                                                   "line number substitutions with '#'"));
		return;
	}

	// issue the command 
	issueCommand(window, subsCommand, std::string(), 0, window->lastFocus_, insertPos + 1, insertPos + 1, fromMacro);
	delete [] subsCommand;
}

/*
** Do a shell command, with the options allowed to users (input source,
** output destination, save first and load after) in the shell commands
** menu.
*/
void DoShellMenuCmd(Document *window, const std::string &command, int input, int output, int outputReplacesInput, int saveFirst, int loadAfter, int fromMacro) {
	int flags = 0;
	char *subsCommand;
	int left = 0;
	int right = 0;
	int line, column;
	char lineNumber[11];
	Document *inWindow = window;

	// Can't do two shell commands at once in the same window 
	if (window->shellCmdData_) {
		QApplication::beep();
		return;
	}
	
	auto textD = reinterpret_cast<TextWidget>(window->lastFocus_)->text.textD;

	/* Substitute the current file name for % and the current line number
	   for # in the shell command */
	QString fullName = QString(QLatin1String("%1%2")).arg(window->path_).arg(window->filename_);
	int pos = textD->TextGetCursorPos();
	textD->TextDPosToLineAndCol(pos, &line, &column);
	sprintf(lineNumber, "%d", line);

	subsCommand = shellCommandSubstitutes(command.c_str(), fullName.toLatin1().data(), lineNumber);
	if(!subsCommand) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Shell Command"), QLatin1String("Shell command is too long due to filename substitutions with '%%' or line number substitutions with '#'"));
		return;
	}

	/* Get the command input as a text string.  If there is input, errors
	  shouldn't be mixed in with output, so set flags to ERROR_DIALOGS */
	std::string text;
	if (input == FROM_SELECTION) {
		text = window->buffer_->BufGetSelectionTextEx();
		if (text.empty()) {
			delete [] subsCommand;
			QApplication::beep();
			return;
		}
		flags |= ACCUMULATE | ERROR_DIALOGS;
	} else if (input == FROM_WINDOW) {
		text = window->buffer_->BufGetAllEx();
		flags |= ACCUMULATE | ERROR_DIALOGS;
	} else if (input == FROM_EITHER) {
		text = window->buffer_->BufGetSelectionTextEx();
		if (text.empty()) {
			text = window->buffer_->BufGetAllEx();
		}
		flags |= ACCUMULATE | ERROR_DIALOGS;
	} else {
		// FROM_NONE 
		text = std::string();
	}

	/* If the buffer was substituting another character for ascii-nuls,
	   put the nuls back in before exporting the text */
	window->buffer_->BufUnsubstituteNullCharsEx(text);

	/* Assign the output destination.  If output is to a new window,
	   create it, and run the command from it instead of the current
	   one, to free the current one from waiting for lengthy execution */
	Widget outWidget = nullptr;
	switch(output) {
	case TO_DIALOG:
		outWidget = nullptr;
		flags |= OUTPUT_TO_DIALOG;
		left  = 0;	
		right = 0;
		break;
	case TO_NEW_WINDOW:
		if(Document *win = EditNewFile(GetPrefOpenInTab() ? inWindow : nullptr, nullptr, false, nullptr, window->path_.toLatin1().data())) {
			inWindow  = win;
			outWidget = win->textArea_;		
			left      = 0;
			right     = 0;
			CheckCloseDim();
		}
		break;
	case TO_SAME_WINDOW:
		outWidget = window->lastFocus_;
		if (outputReplacesInput && input != FROM_NONE) {
			if (input == FROM_WINDOW) {
				left = 0;
				right = window->buffer_->BufGetLength();
			} else if (input == FROM_SELECTION) {
				window->buffer_->GetSimpleSelection(&left, &right);
				flags |= ACCUMULATE | REPLACE_SELECTION;
			} else if (input == FROM_EITHER) {
				if (window->buffer_->GetSimpleSelection(&left, &right)) {
					flags |= ACCUMULATE | REPLACE_SELECTION;
				} else {
					left = 0;
					right = window->buffer_->BufGetLength();
				}
			}
		} else {
			if (window->buffer_->GetSimpleSelection(&left, &right)) {
				flags |= ACCUMULATE | REPLACE_SELECTION;
			} else {
				left = right = textD->TextGetCursorPos();
			}
		}	
		break;
	default:
		Q_ASSERT(0);
		break;
	}

	// If the command requires the file be saved first, save it 
	if (saveFirst) {
		if (!SaveWindow(window)) {
			if (input != FROM_NONE)
			delete [] subsCommand;
			return;
		}
	}

	/* If the command requires the file to be reloaded after execution, set
	   a flag for issueCommand to deal with it when execution is complete */
	if (loadAfter)
		flags |= RELOAD_FILE_AFTER;

	// issue the command 
	issueCommand(inWindow, subsCommand, text, flags, outWidget, left, right, fromMacro);
	delete [] subsCommand;
}

/*
** Cancel the shell command in progress
*/
void AbortShellCommand(Document *window) {
	if(auto cmdData = static_cast<shellCmdInfo *>(window->shellCmdData_)) {
		kill(-cmdData->childPid, SIGTERM);
		finishCmdExecution(window, True);
	}
}

/*
** Issue a shell command and feed it the string "input".  Output can be
** directed either to text widget "textW" where it replaces the text between
** the positions "replaceLeft" and "replaceRight", to a separate pop-up dialog
** (OUTPUT_TO_DIALOG), or to a macro-language string (OUTPUT_TO_STRING).  If
** "input" is nullptr, no input is fed to the process.  If an input string is
** provided, it is freed when the command completes.  Flags:
**
**   ACCUMULATE     	Causes output from the command to be saved up until
**  	    	    	the command completes.
**   ERROR_DIALOGS  	Presents stderr output separately in popup a dialog,
**  	    	    	and also reports failed exit status as a popup dialog
**  	    	    	including the command output.
**   REPLACE_SELECTION  Causes output to replace the selection in textW.
**   RELOAD_FILE_AFTER  Causes the file to be completely reloaded after the
**  	    	    	command completes.
**   OUTPUT_TO_DIALOG   Send output to a pop-up dialog instead of textW
**   OUTPUT_TO_STRING   Output to a macro-language string instead of a text
**  	    	    	widget or dialog.
**
** REPLACE_SELECTION, ERROR_DIALOGS, and OUTPUT_TO_STRING can only be used
** along with ACCUMULATE (these operations can't be done incrementally).
*/
static void issueCommand(Document *window, const std::string &command, const std::string &input, int flags, Widget textW, int replaceLeft, int replaceRight, int fromMacro) {
	int stdinFD  = 0;
	int stdoutFD = 0;
	int stderrFD = 0;
	XtAppContext context = XtWidgetToApplicationContext(window->shell_);
	pid_t childPid;

	// verify consistency of input parameters 
	if ((flags & ERROR_DIALOGS || flags & REPLACE_SELECTION || flags & OUTPUT_TO_STRING) && !(flags & ACCUMULATE))
		return;

	/* a shell command called from a macro must be executed in the same
	   window as the macro, regardless of where the output is directed,
	   so the user can cancel them as a unit */
	if (fromMacro)
		window = MacroRunWindow();

	// put up a watch cursor over the waiting window 
	if (!fromMacro)
		BeginWait(window->shell_);

	// enable the cancel menu item 
	if (!fromMacro)
		window->SetSensitive(window->cancelShellItem_, True);

	// fork the subprocess and issue the command 
	childPid = forkCommand(window->shell_, command, window->path_.toLatin1().data(), &stdinFD, &stdoutFD, (flags & ERROR_DIALOGS) ? &stderrFD : nullptr);

	// set the pipes connected to the process for non-blocking i/o 
	if (fcntl(stdinFD, F_SETFL, O_NONBLOCK) < 0)
		perror("nedit: Internal error (fcntl)");
	if (fcntl(stdoutFD, F_SETFL, O_NONBLOCK) < 0)
		perror("nedit: Internal error (fcntl1)");
	if (flags & ERROR_DIALOGS) {
		if (fcntl(stderrFD, F_SETFL, O_NONBLOCK) < 0)
			perror("nedit: Internal error (fcntl2)");
	}

	// if there's nothing to write to the process' stdin, close it now 
	if(input.empty()) {
		close(stdinFD);
	}

	/* Create a data structure for passing process information around
	   amongst the callback routines which will process i/o and completion */
	auto cmdData = new shellCmdInfo;
	window->shellCmdData_ = cmdData;
	cmdData->flags       = flags;
	cmdData->stdinFD     = stdinFD;
	cmdData->stdoutFD    = stdoutFD;
	cmdData->stderrFD    = stderrFD;
	cmdData->childPid    = childPid;
	cmdData->input       = input;
	cmdData->inIndex     = 0;
	cmdData->textW       = textW;
	cmdData->bannerIsUp  = False;
	cmdData->fromMacro   = fromMacro;
	cmdData->leftPos     = replaceLeft;
	cmdData->rightPos    = replaceRight;
	cmdData->inLength    = input.size();

	// Set up timer proc for putting up banner when process takes too long 
	if (fromMacro) {
		cmdData->bannerTimeoutID = 0;
	} else {
		cmdData->bannerTimeoutID = XtAppAddTimeOut(context, BANNER_WAIT_TIME, bannerTimeoutProc, window);
	}

	// Set up timer proc for flushing output buffers periodically 
	if ((flags & ACCUMULATE) || !textW) {
		cmdData->flushTimeoutID = 0;
	} else {
		cmdData->flushTimeoutID = XtAppAddTimeOut(context, OUTPUT_FLUSH_FREQ, flushTimeoutProc, window);
	}

	// set up callbacks for activity on the file descriptors 
	cmdData->stdoutInputID = XtAppAddInput(context, stdoutFD, (XtPointer)XtInputReadMask, stdoutReadProc, window);
	if(!input.empty()) {
		cmdData->stdinInputID = XtAppAddInput(context, stdinFD, (XtPointer)XtInputWriteMask, stdinWriteProc, window);
	} else {
		cmdData->stdinInputID = 0;
	}
	
	if (flags & ERROR_DIALOGS) {
		cmdData->stderrInputID = XtAppAddInput(context, stderrFD, (XtPointer)XtInputReadMask, stderrReadProc, window);
	} else {
		cmdData->stderrInputID = 0;
	}

	/* If this was called from a macro, preempt the macro untill shell
	   command completes */
	if (fromMacro) {
		PreemptMacro();
	}
}

/*
** Called when the shell sub-process stdout stream has data.  Reads data into
** the "outBufs" buffer chain in the window->shellCommandData_ data structure.
*/
static void stdoutReadProc(XtPointer clientData, int *source, XtInputId *id) {

	(void)id;
	(void)source;

	auto window = static_cast<Document *>(clientData);
	auto cmdData = static_cast<shellCmdInfo *>(window->shellCmdData_);
	int nRead;

	// read from the process' stdout stream 
	auto buf = new bufElem;
	nRead = read(cmdData->stdoutFD, buf->contents, IO_BUF_SIZE);

	// error in read 
	if (nRead == -1) { // error 
		if (errno != EWOULDBLOCK && errno != EAGAIN) {
			perror("nedit: Error reading shell command output");
			delete buf;
			finishCmdExecution(window, True);
		}
		return;
	}

	/* end of data.  If the stderr stream is done too, execution of the
	   shell process is complete, and we can display the results */
	if (nRead == 0) {
		delete buf;
		XtRemoveInput(cmdData->stdoutInputID);
		cmdData->stdoutInputID = 0;
		if (cmdData->stderrInputID == 0)
			finishCmdExecution(window, False);
		return;
	}

	// characters were read successfully, add buf to linked list of buffers 
	buf->length = nRead;
	
	cmdData->outBufs.push_front(buf);
}

/*
** Called when the shell sub-process stderr stream has data.  Reads data into
** the "errBufs" buffer chain in the window->shellCommandData_ data structure.
*/
static void stderrReadProc(XtPointer clientData, int *source, XtInputId *id) {

	(void)id;
	(void)source;

	auto window = static_cast<Document *>(clientData);
	auto cmdData = static_cast<shellCmdInfo *>(window->shellCmdData_);
	int nRead;

	// read from the process' stderr stream 
	auto buf = new bufElem;
	nRead = read(cmdData->stderrFD, buf->contents, IO_BUF_SIZE);

	// error in read 
	if (nRead == -1) {
		if (errno != EWOULDBLOCK && errno != EAGAIN) {
			perror("nedit: Error reading shell command error stream");
			delete buf;
			finishCmdExecution(window, True);
		}
		return;
	}

	/* end of data.  If the stdout stream is done too, execution of the
	   shell process is complete, and we can display the results */
	if (nRead == 0) {
		delete buf;
		XtRemoveInput(cmdData->stderrInputID);
		cmdData->stderrInputID = 0;
		if (cmdData->stdoutInputID == 0)
			finishCmdExecution(window, False);
		return;
	}

	// characters were read successfully, add buf to linked list of buffers 
	buf->length = nRead;
	cmdData->errBufs.push_front(buf);
}

/*
** Called when the shell sub-process stdin stream is ready for input.  Writes
** data from the "input" text string passed to issueCommand.
*/
static void stdinWriteProc(XtPointer clientData, int *source, XtInputId *id) {

	(void)id;
	(void)source;

	auto window = static_cast<Document *>(clientData);
	auto cmdData = static_cast<shellCmdInfo *>(window->shellCmdData_);
	int nWritten;

	nWritten = write(cmdData->stdinFD, &cmdData->input[cmdData->inIndex], cmdData->inLength);
	if (nWritten == -1) {
		if (errno == EPIPE) {
			/* Just shut off input to broken pipes.  User is likely feeding
			   it to a command which does not take input */
			XtRemoveInput(cmdData->stdinInputID);
			cmdData->stdinInputID = 0;
			close(cmdData->stdinFD);
			cmdData->inIndex = -1;
		} else if (errno != EWOULDBLOCK && errno != EAGAIN) {
			perror("nedit: Write to shell command failed");
			finishCmdExecution(window, True);
		}
	} else {
		cmdData->inIndex += nWritten;
		cmdData->inLength -= nWritten;
		if (cmdData->inLength <= 0) {
			XtRemoveInput(cmdData->stdinInputID);
			cmdData->stdinInputID = 0;
			close(cmdData->stdinFD);
			cmdData->inIndex = -1;
		}
	}
}

/*
** Timer proc for putting up the "Shell Command in Progress" banner if
** the process is taking too long.
*/
static void bannerTimeoutProc(XtPointer clientData, XtIntervalId *id) {

	(void)id;

	auto window  = static_cast<Document *>(clientData);
	auto cmdData = static_cast<shellCmdInfo *>(window->shellCmdData_);
	XmString xmCancel;	

	cmdData->bannerIsUp = True;

	// Extract accelerator text from menu PushButtons 
	XtVaGetValues(window->cancelShellItem_, XmNacceleratorText, &xmCancel, nullptr);

	// Translate Motif string to char* 
	std::string cCancel = GetXmStringTextEx(xmCancel);

	// Free Motif String 
	XmStringFree(xmCancel);

	// Create message 
	QString message;
	if (cCancel.empty()) {
		message = QLatin1String("Shell Command in Progress");
	} else {
		message = QString(QLatin1String("Shell Command in Progress -- Press %1 to Cancel")).arg(QString::fromStdString(cCancel));
	}

	window->SetModeMessage(message.toLatin1().data());
	cmdData->bannerTimeoutID = 0;
}

/*
** Buffer replacement wrapper routine to be used for inserting output from
** a command into the buffer, which takes into account that the buffer may
** have been shrunken by the user (eg, by Undo). If necessary, the starting
** and ending positions (part of the state of the command) are corrected.
*/
static void safeBufReplace(TextBuffer *buf, int *start, int *end, const std::string &text) {
	if (*start > buf->BufGetLength()) {
		*start = buf->BufGetLength();
	}
	
	if (*end > buf->BufGetLength()) {
		*end = buf->BufGetLength();
	}
	
	buf->BufReplaceEx(*start, *end, text);
}

/*
** Timer proc for flushing output buffers periodically when the process
** takes too long.
*/
static void flushTimeoutProc(XtPointer clientData, XtIntervalId *id) {

	(void)id;

	auto window  = static_cast<Document *>(clientData);
	auto cmdData = static_cast<shellCmdInfo *>(window->shellCmdData_);
	auto textD   = reinterpret_cast<TextWidget>(cmdData->textW)->text.textD;
	
	TextBuffer *buf = textD->TextGetBuffer();

	// shouldn't happen, but it would be bad if it did 
	if (!cmdData->textW)
		return;

	std::string outText = coalesceOutputEx(cmdData->outBufs);
	if (!outText.empty()) {
		if (buf->BufSubstituteNullCharsEx(outText)) {
			safeBufReplace(buf, &cmdData->leftPos, &cmdData->rightPos, outText);

			textD->TextSetCursorPos(cmdData->leftPos + outText.size());
			cmdData->leftPos += outText.size();
			cmdData->rightPos = cmdData->leftPos;
		} else
			fprintf(stderr, "nedit: Too much binary data\n");
	}

	// re-establish the timer proc (this routine) to continue processing 
	cmdData->flushTimeoutID = XtAppAddTimeOut(XtWidgetToApplicationContext(window->shell_), OUTPUT_FLUSH_FREQ, flushTimeoutProc, clientData);
}

/*
** Clean up after the execution of a shell command sub-process and present
** the output/errors to the user as requested in the initial issueCommand
** call.  If "terminatedOnError" is true, don't bother trying to read the
** output, just close the i/o descriptors, free the memory, and restore the
** user interface state.
*/
static void finishCmdExecution(Document *window, int terminatedOnError) {
	auto cmdData = static_cast<shellCmdInfo *>(window->shellCmdData_);
	TextBuffer *buf;
	int status, failure, errorReport, reselectStart;
	bool cancel = false;
	int fromMacro = cmdData->fromMacro;
	std::string errText;
	std::string outText;

	// Cancel any pending i/o on the file descriptors 
	if (cmdData->stdoutInputID != 0)
		XtRemoveInput(cmdData->stdoutInputID);
	if (cmdData->stdinInputID != 0)
		XtRemoveInput(cmdData->stdinInputID);
	if (cmdData->stderrInputID != 0)
		XtRemoveInput(cmdData->stderrInputID);

	// Close any file descriptors remaining open 
	close(cmdData->stdoutFD);
	if (cmdData->flags & ERROR_DIALOGS)
		close(cmdData->stderrFD);
	if (cmdData->inIndex != -1)
		close(cmdData->stdinFD);

	// Cancel pending timeouts 
	if (cmdData->flushTimeoutID != 0)
		XtRemoveTimeOut(cmdData->flushTimeoutID);
	if (cmdData->bannerTimeoutID != 0)
		XtRemoveTimeOut(cmdData->bannerTimeoutID);

	// Clean up waiting-for-shell-command-to-complete mode 
	if (!cmdData->fromMacro) {
		EndWait(window->shell_);
		window->SetSensitive(window->cancelShellItem_, False);
		if (cmdData->bannerIsUp) {
			window->ClearModeMessage();
		}
	}

	// If the process was killed or became inaccessable, give up 
	if (terminatedOnError) {
		freeBufListEx(cmdData->outBufs);
		freeBufListEx(cmdData->errBufs);
		waitpid(cmdData->childPid, &status, 0);
		goto cmdDone;
	}

	/* Assemble the output from the process' stderr and stdout streams into
	   null terminated strings, and free the buffer lists used to collect it */
	outText = coalesceOutputEx(cmdData->outBufs);

	if (cmdData->flags & ERROR_DIALOGS) {
		errText = coalesceOutputEx(cmdData->errBufs);
	}

	// Wait for the child process to complete and get its return status 
	waitpid(cmdData->childPid, &status, 0);

	/* Present error and stderr-information dialogs.  If a command returned
	   error output, or if the process' exit status indicated failure,
	   present the information to the user. */
	if (cmdData->flags & ERROR_DIALOGS) {
		failure = WIFEXITED(status) && WEXITSTATUS(status) != 0;
		errorReport = !errText.empty();
		
		static const int DF_MAX_MSG_LENGTH = 4096;

		if (failure && errorReport) {
			removeTrailingNewlines(errText);
			truncateString(errText, DF_MAX_MSG_LENGTH);
			
			QMessageBox msgBox;
			msgBox.setWindowTitle(QLatin1String("Warning"));
			msgBox.setText(QString::fromStdString(errText));
			msgBox.setIcon(QMessageBox::Warning);
			msgBox.setStandardButtons(QMessageBox::Cancel);
			auto accept = new QPushButton(QLatin1String("Proceed"));
			msgBox.addButton(accept, QMessageBox::AcceptRole);
			msgBox.setDefaultButton(accept);
			cancel = (msgBox.exec() == QMessageBox::Cancel);
			
		} else if (failure) {
			truncateString(outText, DF_MAX_MSG_LENGTH - 70);
			
			QMessageBox msgBox;
			msgBox.setWindowTitle(QLatin1String("Command Failure"));
			msgBox.setText(QString(QLatin1String("Command reported failed exit status.\nOutput from command:\n%1")).arg(QString::fromStdString(outText)));
			msgBox.setIcon(QMessageBox::Warning);
			msgBox.setStandardButtons(QMessageBox::Cancel);
			auto accept = new QPushButton(QLatin1String("Proceed"));
			msgBox.addButton(accept, QMessageBox::AcceptRole);
			msgBox.setDefaultButton(accept);
			cancel = (msgBox.exec() == QMessageBox::Cancel);				

		} else if (errorReport) {
			removeTrailingNewlines(errText);
			truncateString(errText, DF_MAX_MSG_LENGTH);
			
			QMessageBox msgBox;
			msgBox.setWindowTitle(QLatin1String("Information"));
			msgBox.setText(QString::fromStdString(errText));
			msgBox.setIcon(QMessageBox::Information);
			msgBox.setStandardButtons(QMessageBox::Cancel);
			auto accept = new QPushButton(QLatin1String("Proceed"));
			msgBox.addButton(accept, QMessageBox::AcceptRole);
			msgBox.setDefaultButton(accept);
			cancel = (msgBox.exec() == QMessageBox::Cancel);
		}

		if (cancel) {
			goto cmdDone;
		}
	}
	
	{
		auto textD = reinterpret_cast<TextWidget>(cmdData->textW)->text.textD;

		/* If output is to a dialog, present the dialog.  Otherwise insert the
		   (remaining) output in the text widget as requested, and move the
		   insert point to the end */
		if (cmdData->flags & OUTPUT_TO_DIALOG) {
			removeTrailingNewlines(outText);
			if (!outText.empty()) {
				createOutputDialog(window->shell_, outText);
			}
		} else if (cmdData->flags & OUTPUT_TO_STRING) {
			ReturnShellCommandOutput(window, outText, WEXITSTATUS(status));
		} else {
			buf = textD->TextGetBuffer();
			if (!buf->BufSubstituteNullCharsEx(outText)) {
				fprintf(stderr, "nedit: Too much binary data in shell cmd output\n");
				outText[0] = '\0';
			}
			if (cmdData->flags & REPLACE_SELECTION) {
				reselectStart = buf->primary_.rectangular ? -1 : buf->primary_.start;
				buf->BufReplaceSelectedEx(outText);

				textD->TextSetCursorPos(buf->cursorPosHint_);
				if (reselectStart != -1)
					buf->BufSelect(reselectStart, reselectStart + outText.size());
			} else {
				safeBufReplace(buf, &cmdData->leftPos, &cmdData->rightPos, outText);
				textD->TextSetCursorPos(cmdData->leftPos + outText.size());
			}
		}

		// If the command requires the file to be reloaded afterward, reload it 
		if (cmdData->flags & RELOAD_FILE_AFTER)
			RevertToSaved(window);
	}

cmdDone:
	delete cmdData;
	window->shellCmdData_ = nullptr;
	if (fromMacro)
		ResumeMacroExecution(window);
}

/*
** Fork a subprocess to execute a command, return file descriptors for pipes
** connected to the subprocess' stdin, stdout, and stderr streams.  cmdDir
** sets the default directory for the subprocess.  If stderrFD is passed as
** nullptr, the pipe represented by stdoutFD is connected to both stdin and
** stderr.  The function value returns the pid of the new subprocess, or -1
** if an error occured.
*/
static pid_t forkCommand(Widget parent, const std::string &command, const std::string &cmdDir, int *stdinFD, int *stdoutFD, int *stderrFD) {

	(void)parent;

	int childStdoutFD;
	int childStdinFD;
	int childStderrFD;
	int pipeFDs[2];
	pid_t childPid;

	/* Ignore SIGPIPE signals generated when user attempts to provide
	   input for commands which don't take input */
	signal(SIGPIPE, SIG_IGN);

	/* Create pipes to communicate with the sub process.  One end of each is
	   returned to the caller, the other half is spliced to stdin, stdout
	   and stderr in the child process */
	if (pipe(pipeFDs) != 0) {
		perror("nedit: Internal error (opening stdout pipe)");
		return -1;
	}
	
	*stdoutFD = pipeFDs[0];
	childStdoutFD = pipeFDs[1];
	if (pipe(pipeFDs) != 0) {
		perror("nedit: Internal error (opening stdin pipe)");
		return -1;
	}
	
	*stdinFD = pipeFDs[1];
	childStdinFD = pipeFDs[0];
	if(!stderrFD) {
		childStderrFD = childStdoutFD;
	} else {
		if (pipe(pipeFDs) != 0) {
			perror("nedit: Internal error (opening stdin pipe)");
			return -1;
		}
		*stderrFD = pipeFDs[0];
		childStderrFD = pipeFDs[1];
	}

	// Fork the process 
	childPid = fork();

	/*
	** Child process context (fork returned 0), clean up the
	** child ends of the pipes and execute the command
	*/
	if (childPid == 0) {
		// close the parent end of the pipes in the child process   
		close(*stdinFD);
		close(*stdoutFD);
		if(stderrFD)
			close(*stderrFD);

		/* close current stdin, stdout, and stderr file descriptors before
		   substituting pipes */
		close(fileno(stdin));
		close(fileno(stdout));
		close(fileno(stderr));

		/* duplicate the child ends of the pipes to have the same numbers
		   as stdout & stderr, so it can substitute for stdout & stderr */
		int dupFD = dup2(childStdinFD, fileno(stdin));
		if (dupFD == -1)
			perror("dup of stdin failed");
		dupFD = dup2(childStdoutFD, fileno(stdout));
		if (dupFD == -1)
			perror("dup of stdout failed");
		dupFD = dup2(childStderrFD, fileno(stderr));
		if (dupFD == -1)
			perror("dup of stderr failed");

		/* now close the original child end of the pipes
		   (we now have the 0, 1 and 2 descriptors in their place) */
		close(childStdinFD);
		close(childStdoutFD);
		close(childStderrFD);

		/* make this process the leader of a new process group, so the sub
		   processes can be killed, if necessary, with a killpg call */
		setsid();

		/* change the current working directory to the directory of the
		    current file. */
		if (!cmdDir.empty()) {
			if (chdir(cmdDir.c_str()) == -1) {
				perror("chdir to directory of current file failed");
			}
		}

		// execute the command using the shell specified by preferences 
		execlp(GetPrefShell(), GetPrefShell(), "-c", command.c_str(), nullptr);

		// if we reach here, execlp failed 
		fprintf(stderr, "Error starting shell: %s\n", GetPrefShell());
		exit(EXIT_FAILURE);
	}

	// Parent process context, check if fork succeeded 
	if (childPid == -1) {
		QMessageBox::critical(nullptr /*parent*/, QLatin1String("Shell Command"), QLatin1String("Error starting shell command process\n(fork failed)"));
	}

	// close the child ends of the pipes 
	close(childStdinFD);
	close(childStdoutFD);
	if(stderrFD)
		close(childStderrFD);

	return childPid;
}

/*
** coalesce the contents of a list of buffers into a contiguous memory block,
** freeing the memory occupied by the buffer list.  Returns the memory block
** as the function result, and its length as parameter "length".
*/
static std::string coalesceOutputEx(std::list<bufElem *> &bufList) {

	int length = 0;

	// find the total length of data read 
	for(bufElem *buf : bufList) {
		length += buf->length;
	}
	
	std::string outBuf;
	outBuf.reserve(length);

	// allocate contiguous memory for returning data 

	// copy the buffers into the output bufElem 
	auto outPtr = std::back_inserter(outBuf);
	
	for(auto it = bufList.rbegin(); it != bufList.rend(); ++it) {
		bufElem *buf = *it;
		char *p = buf->contents;
		for (int i = 0; i < buf->length; i++) {
			*outPtr++ = *p++;
		}
	}

	return outBuf;
}

static void freeBufListEx(std::list<bufElem *> &bufList) {

	for(bufElem *buf : bufList) {
		delete buf;
	}
	
	bufList.clear();
}

/*
** Remove trailing newlines from a string by substituting nulls
*/
static void removeTrailingNewlines(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](char ch) {
		return ch != '\n';
	}).base(), s.end());
}

/*
** Create a dialog for the output of a shell command.  The dialog lives until
** the user presses the Dismiss button, and is then destroyed
*/
static void createOutputDialog(Widget parent, const std::string &text) {
	Arg al[50];
	int ac, rows, cols, hasScrollBar, wrapped;
	Widget form, textW, button;
	XmString st1;

	// measure the width and height of the text to determine size for dialog 
	measureText(text.c_str(), MAX_OUT_DIALOG_COLS, &rows, &cols, &wrapped);
	if (rows > MAX_OUT_DIALOG_ROWS) {
		rows = MAX_OUT_DIALOG_ROWS;
		hasScrollBar = True;
	} else
		hasScrollBar = False;
	if (cols > MAX_OUT_DIALOG_COLS)
		cols = MAX_OUT_DIALOG_COLS;
	if (cols == 0)
		cols = 1;
	/* Without completely emulating Motif's wrapping algorithm, we can't
	   be sure that we haven't underestimated the number of lines in case
	   a line has wrapped, so let's assume that some lines could be obscured
	   */
	if (wrapped)
		hasScrollBar = True;
	ac = 0;
	form = CreateFormDialog(parent, "shellOutForm", al, ac);

	ac = 0;
	XtSetArg(al[ac], XmNlabelString, st1 = XmStringCreateLtoREx("OK"));
	ac++;
	XtSetArg(al[ac], XmNmarginWidth, BUTTON_WIDTH_MARGIN);
	ac++;
	XtSetArg(al[ac], XmNhighlightThickness, 2);
	ac++;
	XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_FORM);
	ac++;
	XtSetArg(al[ac], XmNtopAttachment, XmATTACH_NONE);
	ac++;
	button = XmCreatePushButtonGadget(form, (String) "ok", al, ac);
	XtManageChild(button);
	XtVaSetValues(form, XmNdefaultButton, button, nullptr);
	XtVaSetValues(form, XmNcancelButton, button, nullptr);
	XmStringFree(st1);
	XtAddCallback(button, XmNactivateCallback, destroyOutDialogCB, XtParent(form));

	ac = 0;
	XtSetArg(al[ac], XmNrows, rows);
	ac++;
	XtSetArg(al[ac], XmNcolumns, cols);
	ac++;
	XtSetArg(al[ac], XmNresizeHeight, False);
	ac++;
	XtSetArg(al[ac], XmNtraversalOn, True);
	ac++;
	XtSetArg(al[ac], XmNwordWrap, True);
	ac++;
	XtSetArg(al[ac], XmNscrollHorizontal, False);
	ac++;
	XtSetArg(al[ac], XmNscrollVertical, hasScrollBar);
	ac++;
	XtSetArg(al[ac], XmNhighlightThickness, 2);
	ac++;
	XtSetArg(al[ac], XmNspacing, 0);
	ac++;
	XtSetArg(al[ac], XmNeditMode, XmMULTI_LINE_EDIT);
	ac++;
	XtSetArg(al[ac], XmNeditable, False);
	ac++;
	XtSetArg(al[ac], XmNvalue, text.c_str());
	ac++;
	XtSetArg(al[ac], XmNtopAttachment, XmATTACH_FORM);
	ac++;
	XtSetArg(al[ac], XmNleftAttachment, XmATTACH_FORM);
	ac++;
	XtSetArg(al[ac], XmNbottomAttachment, XmATTACH_WIDGET);
	ac++;
	XtSetArg(al[ac], XmNrightAttachment, XmATTACH_FORM);
	ac++;
	XtSetArg(al[ac], XmNbottomWidget, button);
	ac++;
	textW = XmCreateScrolledText(form, (String) "outText", al, ac);
	AddMouseWheelSupport(textW);
	MakeSingleLineTextW(textW); // Binds <Return> to activate() 
	XtManageChild(textW);

	XtVaSetValues(XtParent(form), XmNtitle, "Output from Command", nullptr);
	ManageDialogCenteredOnPointer(form);

	XmProcessTraversal(textW, XmTRAVERSE_CURRENT);
}

/*
** Dispose of the command output dialog when user presses Dismiss button
*/
static void destroyOutDialogCB(Widget w, XtPointer callback, XtPointer closure) {
	(void)w;
	(void)closure;

	XtDestroyWidget((Widget)callback);
}

/*
** Measure the width and height of a string of text.  Assumes 8 character
** tabs.  wrapWidth specifies a number of columns at which text wraps.
*/
static void measureText(view::string_view text, int wrapWidth, int *rows, int *cols, int *wrapped) {
	int maxCols = 0;
	int line = 1;
	int col = 0;
	int wrapCol;

	*wrapped = 0;
	for(char ch : text) {
		if (ch == '\n') {
			line++;
			col = 0;
			continue;
		}

		if (ch == '\t') {
			col += 8 - (col % 8);
			wrapCol = 0; // Tabs at end of line are not drawn when wrapped 
		} else if (ch == ' ') {
			col++;
			wrapCol = 0; // Spaces at end of line are not drawn when wrapped 
		} else {
			col++;
			wrapCol = 1;
		}

		/* Note: there is a small chance that the number of lines is
		   over-estimated when a line ends with a space or a tab (ie, followed
		       by a newline) and that whitespace crosses the boundary, because
		       whitespace at the end of a line does not cause wrapping. Taking
		       this into account is very hard, but an over-estimation is harmless.
		       The worst that can happen is that some extra blank lines are shown
		       at the end of the dialog (in contrast to an under-estimation, which
		       could make the last lines invisible).
		       On the other hand, without emulating Motif's wrapping algorithm
		       completely, we can't be sure that we don't underestimate the number
		       of lines (Motif uses word wrap, and this counting algorithm uses
		       character wrap). Therefore, we remember whether there is a line
		       that has wrapped. In that case we allways install a scroll bar.
		   */
		if (col > wrapWidth) {
			line++;
			*wrapped = 1;
			col = wrapCol;
		} else if (col > maxCols) {
			maxCols = col;
		}
	}
	*rows = line;
	*cols = maxCols;
}

/*
** Truncate a string to a maximum of length characters.  If it shortens the
** string, it appends "..." to show that it has been shortened. It assumes
** that the string that it is passed is writeable.
*/
static void truncateString(std::string &string, int length) {
	if ((int)string.size() > length) {
		string.replace(length, std::string::npos, "...");
	
	}
}

/*
** Substitute the string fileStr in inStr wherever % appears and
** lineStr in inStr wherever # appears, storing the
** result in outStr. If predictOnly is non-zero, the result string length
** is predicted without creating the string. Returns the length of the result
** string or -1 in case of an error.
**
*/
static int shellSubstituter(char *outStr, const char *inStr, const char *fileStr, const char *lineStr, int outLen, int predictOnly) {
	const char *inChar;
	char *outChar = nullptr;
	int outWritten = 0;
	int fileLen, lineLen;

	inChar = inStr;
	if (!predictOnly) {
		outChar = outStr;
	}
	fileLen = strlen(fileStr);
	lineLen = strlen(lineStr);

	while (*inChar != '\0') {

		if (!predictOnly && outWritten >= outLen) {
			return (-1);
		}

		if (*inChar == '%') {
			if (*(inChar + 1) == '%') {
				inChar += 2;
				if (!predictOnly) {
					*outChar++ = '%';
				}
				outWritten++;
			} else {
				if (!predictOnly) {
					if (outWritten + fileLen >= outLen) {
						return (-1);
					}
					strncpy(outChar, fileStr, fileLen);
					outChar += fileLen;
				}
				outWritten += fileLen;
				inChar++;
			}
		} else if (*inChar == '#') {
			if (*(inChar + 1) == '#') {
				inChar += 2;
				if (!predictOnly) {
					*outChar++ = '#';
				}
				outWritten++;
			} else {
				if (!predictOnly) {
					if (outWritten + lineLen >= outLen) {
						return (-1);
					}
					strncpy(outChar, lineStr, lineLen);
					outChar += lineLen;
				}
				outWritten += lineLen;
				inChar++;
			}
		} else {
			if (!predictOnly) {
				*outChar++ = *inChar;
			}
			inChar++;
			outWritten++;
		}
	}

	if (!predictOnly) {
		if (outWritten >= outLen) {
			return (-1);
		}
		*outChar = '\0';
	}
	++outWritten;
	return (outWritten);
}

static char *shellCommandSubstitutes(const char *inStr, const char *fileStr, const char *lineStr) {
	int cmdLen;
	char *subsCmdStr = nullptr;

	cmdLen = shellSubstituter(nullptr, inStr, fileStr, lineStr, 0, 1);
	if (cmdLen >= 0) {
		subsCmdStr = new char[cmdLen];
		if (subsCmdStr) {
			cmdLen = shellSubstituter(subsCmdStr, inStr, fileStr, lineStr, cmdLen, 0);
			if (cmdLen < 0) {
				delete [] subsCmdStr;
				subsCmdStr = nullptr;
			}
		}
	}
	return subsCmdStr;
}
