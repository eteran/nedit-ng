/*******************************************************************************
*                                                                              *
* windowTitle.c -- Nirvana Editor window title customization                   *
*                                                                              *
* Copyright (C) 2001, Arne Forlie                                              *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version. In addition, you may distribute versions of this program linked to  *
* Motif or Open Motif. See README for details.                                 *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for    *
* more details.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* July 31, 2001                                                                *
*                                                                              *
* Written by Arne Forlie, http://arne.forlie.com                               *
*                                                                              *
*******************************************************************************/

#include "windowTitle.h"
#include "TextBuffer.h"
#include "nedit.h"
#include "preferences.h"
#include "help.h"
#include "Document.h"
#include "MotifHelper.h"
#include "../util/prefFile.h"
#include "../util/misc.h"
#include "../util/DialogF.h"
#include "../util/utils.h"
#include "../util/fileUtils.h"

#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <sys/param.h>
#include "../util/clearcase.h"

#include <Xm/Xm.h>
#include <Xm/SelectioB.h>
#include <Xm/Form.h>
#include <Xm/List.h>
#include <Xm/SeparatoG.h>
#include <Xm/LabelG.h>
#include <Xm/PushBG.h>
#include <Xm/PushB.h>
#include <Xm/ToggleBG.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeBG.h>
#include <Xm/Frame.h>
#include <Xm/Text.h>
#include <Xm/TextF.h>

namespace {

const int LEFT_MARGIN_POS     = 2;
const int RIGHT_MARGIN_POS    = 98;
const int V_MARGIN            = 5;
const int RADIO_INDENT        = 3;
const int WINDOWTITLE_MAX_LEN = 500;

char *removeSequence(char *sourcePtr, char c) {

	while (*sourcePtr == c) {
		sourcePtr++;
	}
	
	return sourcePtr;
}

/*
** Two functions for performing safe insertions into a finite
** size buffer so that we don't get any memory overruns.
*/
char *safeStrCpy(char *dest, char *destEnd, const char *source) {
	int len = (int)strlen(source);
	if (len <= (destEnd - dest)) {
		strcpy(dest, source);
		return (dest + len);
	} else {
		strncpy(dest, source, destEnd - dest);
		*destEnd = '\0';
		return destEnd;
	}
}

char *safeCharAdd(char *dest, char *destEnd, char c) {
	if (destEnd - dest > 0) {
		*dest++ = c;
		*dest = '\0';
	}
	return dest;
}

}



/* Customize window title dialog information */
static struct {
	Widget form;
	Widget shell;
	Document *window;
	Widget previewW;
	Widget formatW;

	Widget ccW;
	Widget fileW;
	Widget hostW;
	Widget dirW;
	Widget statusW;
	Widget shortStatusW;
	Widget serverW;
	Widget nameW;
	Widget mdirW;
	Widget ndirW;

	Widget oDirW;
	Widget oCcViewTagW;
	Widget oServerNameW;
	Widget oFileChangedW;
	Widget oFileLockedW;
	Widget oFileReadOnlyW;
	Widget oServerEqualViewW;

	char filename[MAXPATHLEN];
	char path[MAXPATHLEN];
	char viewTag[MAXPATHLEN];
	char serverName[MAXPATHLEN];
	int isServer;
	int filenameSet;
	int lockReasons;
	int fileChanged;

	int suppressFormatUpdate;
} etDialog = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
              nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, "",      "",      "",      "",      0,       0,       0,       0,       0};





/*
** Remove empty paranthesis pairs and multiple spaces in a row
** with one space.
** Also remove leading and trailing spaces and dashes.
*/
static void compressWindowTitle(char *title) {
	/* Compress the title */
	int modified;
	do {
		char *sourcePtr = title;
		char *destPtr = sourcePtr;
		char c = *sourcePtr++;

		modified = False;

		/* Remove leading spaces and dashes */
		while (c == ' ' || c == '-') {
			c = *sourcePtr++;
		}

		/* Remove empty constructs */
		while (c != '\0') {
			switch (c) {
			/* remove sequences */
			case ' ':
			case '-':
				sourcePtr = removeSequence(sourcePtr, c);
				*destPtr++ = c; /* leave one */
				break;

			/* remove empty paranthesis pairs */
			case '(':
				if (*sourcePtr == ')') {
					modified = True;
					sourcePtr++;
				} else
					*destPtr++ = c;
				sourcePtr = removeSequence(sourcePtr, ' ');
				break;

			case '[':
				if (*sourcePtr == ']') {
					modified = True;
					sourcePtr++;
				} else
					*destPtr++ = c;
				sourcePtr = removeSequence(sourcePtr, ' ');
				break;

			case '{':
				if (*sourcePtr == '}') {
					modified = True;
					sourcePtr++;
				} else
					*destPtr++ = c;
				sourcePtr = removeSequence(sourcePtr, ' ');
				break;

			default:
				*destPtr++ = c;
				break;
			}
			c = *sourcePtr++;
			*destPtr = '\0';
		}

		/* Remove trailing spaces and dashes */
		while (destPtr-- > title) {
			if (*destPtr != ' ' && *destPtr != '-')
				break;
			*destPtr = '\0';
		}
	} while (modified == True);
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
char *FormatWindowTitle(const char *filename, const char *path, const char *clearCaseViewTag, const char *serverName, int isServer, int filenameSet, int lockReasons, int fileChanged, const char *titleFormat) {
	static char title[WINDOWTITLE_MAX_LEN];
	char *titlePtr = title;
	char *titleEnd = title + WINDOWTITLE_MAX_LEN - 1;

	/* Flags to supress one of these if both are specified and they are identical */
	int serverNameSeen = False;
	int clearCaseViewTagSeen = False;

	int fileNamePresent = False;
	int hostNamePresent = False;
	int userNamePresent = False;
	int serverNamePresent = False;
	int clearCasePresent = False;
	int fileStatusPresent = False;
	int dirNamePresent = False;
	int noOfComponents = -1;
	int shortStatus = False;

	*titlePtr = '\0'; /* always start with an empty string */

	while (*titleFormat != '\0' && titlePtr < titleEnd) {
		char c = *titleFormat++;
		if (c == '%') {
			c = *titleFormat++;
			if (c == '\0') {
				titlePtr = safeCharAdd(titlePtr, titleEnd, '%');
				break;
			}
			switch (c) {
			case 'c': /* ClearCase view tag */
				clearCasePresent = True;
				if (clearCaseViewTag) {
					if (serverNameSeen == False || strcmp(serverName, clearCaseViewTag) != 0) {
						titlePtr = safeStrCpy(titlePtr, titleEnd, clearCaseViewTag);
						clearCaseViewTagSeen = True;
					}
				}
				break;

			case 's': /* server name */
				serverNamePresent = True;
				if (isServer && serverName[0] != '\0') { /* only applicable for servers */
					if (clearCaseViewTagSeen == False || strcmp(serverName, clearCaseViewTag) != 0) {
						titlePtr = safeStrCpy(titlePtr, titleEnd, serverName);
						serverNameSeen = True;
					}
				}
				break;

			case 'd': /* directory without any limit to no. of components */
				dirNamePresent = True;
				if (filenameSet) {
					titlePtr = safeStrCpy(titlePtr, titleEnd, path);
				}
				break;

			case '0': /* directory with limited no. of components */
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (*titleFormat == 'd') {
					dirNamePresent = True;
					noOfComponents = c - '0';
					titleFormat++; /* delete the argument */

					if (filenameSet) {
						const char *trailingPath = GetTrailingPathComponents(path, noOfComponents);

						/* prefix with ellipsis if components were skipped */
						if (trailingPath > path) {
							titlePtr = safeStrCpy(titlePtr, titleEnd, "...");
						}
						titlePtr = safeStrCpy(titlePtr, titleEnd, trailingPath);
					}
				}
				break;

			case 'f': /* file name */
				fileNamePresent = True;
				titlePtr = safeStrCpy(titlePtr, titleEnd, filename);
				break;

			case 'h': /* host name */
				hostNamePresent = True;
				titlePtr = safeStrCpy(titlePtr, titleEnd, GetNameOfHostEx().c_str());
				break;

			case 'S': /* file status */
				fileStatusPresent = True;
				if (IS_ANY_LOCKED_IGNORING_USER(lockReasons) && fileChanged)
					titlePtr = safeStrCpy(titlePtr, titleEnd, "read only, modified");
				else if (IS_ANY_LOCKED_IGNORING_USER(lockReasons))
					titlePtr = safeStrCpy(titlePtr, titleEnd, "read only");
				else if (IS_USER_LOCKED(lockReasons) && fileChanged)
					titlePtr = safeStrCpy(titlePtr, titleEnd, "locked, modified");
				else if (IS_USER_LOCKED(lockReasons))
					titlePtr = safeStrCpy(titlePtr, titleEnd, "locked");
				else if (fileChanged)
					titlePtr = safeStrCpy(titlePtr, titleEnd, "modified");
				break;

			case 'u': /* user name */
				userNamePresent = True;
				titlePtr = safeStrCpy(titlePtr, titleEnd, GetUserNameEx().c_str());
				break;

			case '%': /* escaped % */
				titlePtr = safeCharAdd(titlePtr, titleEnd, '%');
				break;

			case '*': /* short file status ? */
				fileStatusPresent = True;
				if (*titleFormat && *titleFormat == 'S') {
					++titleFormat;
					shortStatus = True;
					if (IS_ANY_LOCKED_IGNORING_USER(lockReasons) && fileChanged)
						titlePtr = safeStrCpy(titlePtr, titleEnd, "RO*");
					else if (IS_ANY_LOCKED_IGNORING_USER(lockReasons))
						titlePtr = safeStrCpy(titlePtr, titleEnd, "RO");
					else if (IS_USER_LOCKED(lockReasons) && fileChanged)
						titlePtr = safeStrCpy(titlePtr, titleEnd, "LO*");
					else if (IS_USER_LOCKED(lockReasons))
						titlePtr = safeStrCpy(titlePtr, titleEnd, "LO");
					else if (fileChanged)
						titlePtr = safeStrCpy(titlePtr, titleEnd, "*");
					break;
				}
			/* fall-through */
			default:
				titlePtr = safeCharAdd(titlePtr, titleEnd, c);
				break;
			}
		} else {
			titlePtr = safeCharAdd(titlePtr, titleEnd, c);
		}
	}

	compressWindowTitle(title);

	if (title[0] == 0) {
		sprintf(&title[0], "<empty>"); /* For preview purposes only */
	}

	if (etDialog.form) {
		/* Prevent recursive callback loop */
		etDialog.suppressFormatUpdate = True;

		/* Sync radio buttons with format string (in case the user entered
		   the format manually) */
		XmToggleButtonSetState(etDialog.fileW, fileNamePresent, False);
		XmToggleButtonSetState(etDialog.statusW, fileStatusPresent, False);
		XmToggleButtonSetState(etDialog.serverW, serverNamePresent, False);

		XmToggleButtonSetState(etDialog.ccW, clearCasePresent, False);

		XmToggleButtonSetState(etDialog.dirW, dirNamePresent, False);
		XmToggleButtonSetState(etDialog.hostW, hostNamePresent, False);
		XmToggleButtonSetState(etDialog.nameW, userNamePresent, False);

		XtSetSensitive(etDialog.shortStatusW, fileStatusPresent);
		if (fileStatusPresent) {
			XmToggleButtonSetState(etDialog.shortStatusW, shortStatus, False);
		}

		/* Directory components are also sensitive to presence of dir */
		XtSetSensitive(etDialog.ndirW, dirNamePresent);
		XtSetSensitive(etDialog.mdirW, dirNamePresent);

		if (dirNamePresent) /* Avoid erasing number when not active */
		{
			if (noOfComponents >= 0) {
				std::string value = XmTextGetStringEx(etDialog.ndirW);
				char buf[2];
				sprintf(&buf[0], "%d", noOfComponents);
				if (strcmp(&buf[0], value.c_str())) /* Don't overwrite unless diff. */
					SetIntText(etDialog.ndirW, noOfComponents);
			} else {
				XmTextSetStringEx(etDialog.ndirW, "");
			}
		}

		/* Enable/disable test buttons, depending on presence of codes */
		XtSetSensitive(etDialog.oFileChangedW, fileStatusPresent);
		XtSetSensitive(etDialog.oFileReadOnlyW, fileStatusPresent);
		XtSetSensitive(etDialog.oFileLockedW, fileStatusPresent && !IS_PERM_LOCKED(etDialog.lockReasons));

		XtSetSensitive(etDialog.oServerNameW, serverNamePresent);

		XtSetSensitive(etDialog.oCcViewTagW, clearCasePresent);
		XtSetSensitive(etDialog.oServerEqualViewW, clearCasePresent && serverNamePresent);

		XtSetSensitive(etDialog.oDirW, dirNamePresent);

		etDialog.suppressFormatUpdate = False;
	}

	return (title);
}

/* a utility that sets the values of all toggle buttons */
static void setToggleButtons(void) {
	XmToggleButtonSetState(etDialog.oDirW, etDialog.filenameSet == True, False);
	XmToggleButtonSetState(etDialog.oFileChangedW, etDialog.fileChanged == True, False);
	XmToggleButtonSetState(etDialog.oFileReadOnlyW, IS_PERM_LOCKED(etDialog.lockReasons), False);
	XmToggleButtonSetState(etDialog.oFileLockedW, IS_USER_LOCKED(etDialog.lockReasons), False);
	/* Read-only takes precedence on locked */
	XtSetSensitive(etDialog.oFileLockedW, !IS_PERM_LOCKED(etDialog.lockReasons));

	XmToggleButtonSetState(etDialog.oCcViewTagW, GetClearCaseViewTag() != nullptr, False);
	XmToggleButtonSetState(etDialog.oServerNameW, etDialog.isServer, False);

	if (GetClearCaseViewTag() != nullptr && etDialog.isServer && GetPrefServerName()[0] != '\0' && strcmp(GetClearCaseViewTag(), GetPrefServerName()) == 0) {
		XmToggleButtonSetState(etDialog.oServerEqualViewW, True, False);
	} else {
		XmToggleButtonSetState(etDialog.oServerEqualViewW, False, False);
	}
}

static void formatChangedCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	char *format;
	int filenameSet = XmToggleButtonGetState(etDialog.oDirW);
	char *title;
	const char *serverName;

	if (etDialog.suppressFormatUpdate) {
		return; /* Prevent recursive feedback */
	}

	format = XmTextGetString(etDialog.formatW);

	if (XmToggleButtonGetState(etDialog.oServerEqualViewW) && XmToggleButtonGetState(etDialog.ccW)) {
		serverName = etDialog.viewTag;
	} else {
		serverName = XmToggleButtonGetState(etDialog.oServerNameW) ? etDialog.serverName : "";
	}

	title = FormatWindowTitle(etDialog.filename, etDialog.filenameSet == True ? etDialog.path : "/a/very/long/path/used/as/example/",

	                          XmToggleButtonGetState(etDialog.oCcViewTagW) ? etDialog.viewTag : nullptr, serverName, etDialog.isServer, filenameSet, etDialog.lockReasons, XmToggleButtonGetState(etDialog.oFileChangedW), format);
	XtFree(format);
	XmTextFieldSetString(etDialog.previewW, title);
}

static void ccViewTagCB(Widget w, XtPointer clientData, XtPointer callData) {
	if (XmToggleButtonGetState(w) == False) {
		XmToggleButtonSetState(etDialog.oServerEqualViewW, False, False);
	}
	formatChangedCB(w, clientData, callData);
}

static void serverNameCB(Widget w, XtPointer clientData, XtPointer callData) {
	if (XmToggleButtonGetState(w) == False) {
		XmToggleButtonSetState(etDialog.oServerEqualViewW, False, False);
	}
	etDialog.isServer = XmToggleButtonGetState(w);
	formatChangedCB(w, clientData, callData);
}

static void fileChangedCB(Widget w, XtPointer clientData, XtPointer callData) {
	etDialog.fileChanged = XmToggleButtonGetState(w);
	formatChangedCB(w, clientData, callData);
}

static void fileLockedCB(Widget w, XtPointer clientData, XtPointer callData) {
	SET_USER_LOCKED(etDialog.lockReasons, XmToggleButtonGetState(w));
	formatChangedCB(w, clientData, callData);
}

static void fileReadOnlyCB(Widget w, XtPointer clientData, XtPointer callData) {
	SET_PERM_LOCKED(etDialog.lockReasons, XmToggleButtonGetState(w));
	formatChangedCB(w, clientData, callData);
}

static void serverEqualViewCB(Widget w, XtPointer clientData, XtPointer callData) {
	if (XmToggleButtonGetState(w) == True) {
		XmToggleButtonSetState(etDialog.oCcViewTagW, True, False);
		XmToggleButtonSetState(etDialog.oServerNameW, True, False);
		etDialog.isServer = True;
	}
	formatChangedCB(w, clientData, callData);
}

static void applyCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	char *format = XmTextGetString(etDialog.formatW);

	/* pop down the dialog */
	/*    XtUnmanageChild(etDialog.form); */

	if (strcmp(format, GetPrefTitleFormat()) != 0) {
		SetPrefTitleFormat(format);
	}
	XtFree(format);
}

static void closeCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	/* pop down the dialog */
	XtUnmanageChild(etDialog.form);
}

static void restoreCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	XmTextSetStringEx(etDialog.formatW, "{%c} [%s] %f (%S) - %d");
}

static void helpCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)w;
	(void)clientData;
	(void)callData;

	Help(HELP_CUSTOM_TITLE_DIALOG);
}

static void wtDestroyCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)clientData;
	(void)callData;

	if (w == etDialog.form) /* Prevent disconnecting the replacing dialog */
		etDialog.form = nullptr;
}

static void wtUnmapCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)clientData;
	(void)callData;

	if (etDialog.form == w) /* Prevent destroying the replacing dialog */
		XtDestroyWidget(etDialog.form);
}

static void appendToFormat(view::string_view string) {

	std::string format = XmTextGetStringEx(etDialog.formatW);

	std::string buf;
	buf.reserve(string.size() + format.size());
	
	buf.append(format);
	buf.append(string.begin(), string.end());
	
	XmTextSetStringEx(etDialog.formatW, buf);
}

static void removeFromFormat(const char *string) {
	char *format = XmTextGetString(etDialog.formatW);
	char *pos;

	/* There can be multiple occurences */
	while ((pos = strstr(format, string))) {
		/* If the string is preceded or followed by a brace, include
		   the brace(s) for removal */
		char *start = pos;
		char *end = pos + strlen(string);
		char post = *end;

		if (post == '}' || post == ')' || post == ']' || post == '>') {
			end += 1;
			post = *end;
		}

		if (start > format) {
			char pre = *(start - 1);
			if (pre == '{' || pre == '(' || pre == '[' || pre == '<')
				start -= 1;
		}
		if (start > format) {
			char pre = *(start - 1);
			/* If there is a space in front and behind, remove one space
			   (there can be more spaces, but in that case it is likely
			   that the user entered them manually); also remove trailing
			   space */
			if (pre == ' ' && post == ' ') {
				end += 1;
			} else if (pre == ' ' && post == (char)0) {
				/* Remove (1) trailing space */
				start -= 1;
			}
		}

		/* Contract the string: move end to start */
		strcpy(start, end);
	}

	/* Remove leading and trailing space */
	pos = format;
	while (*pos == ' ')
		++pos;
	strcpy(format, pos);

	pos = format + strlen(format) - 1;
	while (pos >= format && *pos == ' ') {
		--pos;
	}
	*(pos + 1) = (char)0;

	XmTextSetStringEx(etDialog.formatW, format);
	XtFree(format);
}

static void toggleFileCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)w;
	(void)clientData;
	(void)callData;

	if (XmToggleButtonGetState(etDialog.fileW))
		appendToFormat(" %f");
	else
		removeFromFormat("%f");
}

static void toggleServerCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)w;
	(void)clientData;
	(void)callData;

	if (XmToggleButtonGetState(etDialog.serverW))
		appendToFormat(" [%s]");
	else
		removeFromFormat("%s");
}

static void toggleHostCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)w;
	(void)clientData;
	(void)callData;

	if (XmToggleButtonGetState(etDialog.hostW))
		appendToFormat(" [%h]");
	else
		removeFromFormat("%h");
}

static void toggleClearCaseCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	if (XmToggleButtonGetState(etDialog.ccW))
		appendToFormat(" {%c}");
	else
		removeFromFormat("%c");
}

static void toggleStatusCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)w;
	(void)clientData;
	(void)callData;

	if (XmToggleButtonGetState(etDialog.statusW)) {
		if (XmToggleButtonGetState(etDialog.shortStatusW))
			appendToFormat(" (%*S)");
		else
			appendToFormat(" (%S)");
	} else {
		removeFromFormat("%S");
		removeFromFormat("%*S");
	}
}

static void toggleShortStatusCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	char *format, *pos;

	if (etDialog.suppressFormatUpdate) {
		return;
	}

	format = XmTextGetString(etDialog.formatW);

	if (XmToggleButtonGetState(etDialog.shortStatusW)) {
		/* Find all %S occurrences and replace them by %*S */
		do {
			pos = strstr(format, "%S");
			if (pos) {
				char *tmp = XtMalloc((strlen(format) + 2));
				strncpy(tmp, format, (size_t)(pos - format + 1));
				tmp[pos - format + 1] = 0;
				strcat(tmp, "*");
				strcat(tmp, pos + 1);
				XtFree(format);
				format = tmp;
			}
		} while (pos);
	} else {
		/* Replace all %*S occurences by %S */
		do {
			pos = strstr(format, "%*S");
			if (pos) {
				strcpy(pos + 1, pos + 2);
			}
		} while (pos);
	}

	XmTextSetStringEx(etDialog.formatW, format);
	XtFree(format);
}

static void toggleUserCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	if (XmToggleButtonGetState(etDialog.nameW))
		appendToFormat(" %u");
	else
		removeFromFormat("%u");
}

static void toggleDirectoryCB(Widget w, XtPointer clientData, XtPointer callData) {
	(void)w;
	(void)clientData;
	(void)callData;

	if (XmToggleButtonGetState(etDialog.dirW)) {
		char buf[20];
		int maxComp;
		char *value = XmTextGetString(etDialog.ndirW);
		if (*value) {
			if (sscanf(value, "%d", &maxComp) > 0) {
				sprintf(&buf[0], " %%%dd ", maxComp);
			} else {
				sprintf(&buf[0], " %%d "); /* Should not be necessary */
			}
		} else {
			sprintf(&buf[0], " %%d ");
		}
		XtFree(value);
		appendToFormat(buf);
	} else {
		int i;
		removeFromFormat("%d");
		for (i = 0; i <= 9; ++i) {
			char buf[20];
			sprintf(&buf[0], "%%%dd", i);
			removeFromFormat(buf);
		}
	}
}

static void enterMaxDirCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)clientData;
	(void)callData;

	int maxComp = -1;
	char *format;
	char *value;

	if (etDialog.suppressFormatUpdate) {
		return;
	}

	format = XmTextGetString(etDialog.formatW);
	value = XmTextGetString(etDialog.ndirW);

	if (*value) {
		if (sscanf(value, "%d", &maxComp) <= 0) {
			/* Don't allow non-digits to be entered */
			XBell(XtDisplay(w), 0);
			XmTextSetStringEx(etDialog.ndirW, "");
		}
	}

	if (maxComp >= 0) {
		char *pos;
		int found = False;
		char insert[2];
		insert[0] = (char)('0' + maxComp);
		insert[1] = (char)0; /* '0' digit and 0 char ! */

		/* Find all %d and %nd occurrences and replace them by the new value */
		do {
			int i;
			found = False;
			pos = strstr(format, "%d");
			if (pos) {
				char *tmp = XtMalloc((strlen(format) + 2));
				strncpy(tmp, format, (size_t)(pos - format + 1));
				tmp[pos - format + 1] = 0;
				strcat(tmp, &insert[0]);
				strcat(tmp, pos + 1);
				XtFree(format);
				format = tmp;
				found = True;
			}

			for (i = 0; i <= 9; ++i) {
				char buf[20];
				sprintf(&buf[0], "%%%dd", i);
				if (i != maxComp) {
					pos = strstr(format, &buf[0]);
					if (pos) {
						*(pos + 1) = insert[0];
						found = True;
					}
				}
			}
		} while (found);
	} else {
		int found = True;

		/* Replace all %nd occurences by %d */
		do {
			int i;
			found = False;
			for (i = 0; i <= 9; ++i) {
				char buf[20];
				char *pos;
				sprintf(&buf[0], "%%%dd", i);
				pos = strstr(format, &buf[0]);
				if (pos) {
					strcpy(pos + 1, pos + 2);
					found = True;
				}
			}
		} while (found);
	}

	XmTextSetStringEx(etDialog.formatW, format);
	XtFree(format);
	XtFree(value);
}

static void createEditTitleDialog(Widget parent) {

	Widget buttonForm, formatLbl, previewFrame;
	Widget previewForm, previewBox, selectFrame, selectBox, selectForm;
	Widget testLbl, selectLbl;
	Widget applyBtn, closeBtn, restoreBtn, helpBtn;
	XmString s1;
	XmFontList fontList;
	Arg args[20];
	int defaultBtnOffset;
	Dimension shadowThickness;
	Dimension radioHeight, textHeight;
	Pixel background;

	int ac = 0;
	XtSetArg(args[ac], XmNautoUnmanage, False);
	ac++;
	XtSetArg(args[ac], XmNtitle, "Customize Window Title");
	ac++;
	etDialog.form = CreateFormDialog(parent, "customizeTitle", args, ac);

	/*
	 * Destroy the dialog every time it is unmapped (otherwise it 'sticks'
	 * to the window for which it was created originally).
	 */
	XtAddCallback(etDialog.form, XmNunmapCallback, wtUnmapCB, nullptr);
	XtAddCallback(etDialog.form, XmNdestroyCallback, wtDestroyCB, nullptr);

	etDialog.shell = XtParent(etDialog.form);

	/* Definition form */
	selectFrame = XtVaCreateManagedWidget("selectionFrame", xmFrameWidgetClass, etDialog.form, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LEFT_MARGIN_POS, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, V_MARGIN,
	                                      XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, nullptr);

	XtVaCreateManagedWidget("titleLabel", xmLabelGadgetClass, selectFrame, XmNlabelString, s1 = XmStringCreateSimpleEx("Title definition"), XmNchildType, XmFRAME_TITLE_CHILD, XmNchildHorizontalAlignment, XmALIGNMENT_BEGINNING, nullptr);
	XmStringFree(s1);

	selectForm = XtVaCreateManagedWidget("selectForm", xmFormWidgetClass, selectFrame, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LEFT_MARGIN_POS, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, V_MARGIN, XmNrightAttachment,
	                                     XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, nullptr);

	selectLbl = XtVaCreateManagedWidget("selectLabel", xmLabelGadgetClass, selectForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Select title components to include:  "), XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition,
	                                    LEFT_MARGIN_POS, XmNtopOffset, 5, XmNbottomOffset, 5, XmNtopAttachment, XmATTACH_FORM, nullptr);
	XmStringFree(s1);

	selectBox = XtVaCreateManagedWidget("selectBox", xmFormWidgetClass, selectForm, XmNorientation, XmHORIZONTAL, XmNpacking, XmPACK_TIGHT, XmNradioBehavior, False, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM,
	                                    XmNtopOffset, 5, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, selectLbl, nullptr);

	etDialog.fileW = XtVaCreateManagedWidget("file", xmToggleButtonWidgetClass, selectBox, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, RADIO_INDENT, XmNtopAttachment, XmATTACH_FORM, XmNlabelString,
	                                         s1 = XmStringCreateSimpleEx("File name (%f)"), XmNmnemonic, 'F', nullptr);
	XtAddCallback(etDialog.fileW, XmNvalueChangedCallback, toggleFileCB, nullptr);
	XmStringFree(s1);

	etDialog.statusW = XtVaCreateManagedWidget("status", xmToggleButtonWidgetClass, selectBox, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, RADIO_INDENT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, etDialog.fileW,
	                                           XmNlabelString, s1 = XmStringCreateSimpleEx("File status (%S) "), XmNmnemonic, 't', nullptr);
	XtAddCallback(etDialog.statusW, XmNvalueChangedCallback, toggleStatusCB, nullptr);
	XmStringFree(s1);

	etDialog.shortStatusW = XtVaCreateManagedWidget("shortStatus", xmToggleButtonWidgetClass, selectBox, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, etDialog.statusW, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, etDialog.fileW,
	                                                XmNlabelString, s1 = XmStringCreateSimpleEx("brief"), XmNmnemonic, 'b', nullptr);
	XtAddCallback(etDialog.shortStatusW, XmNvalueChangedCallback, toggleShortStatusCB, nullptr);
	XmStringFree(s1);

	etDialog.ccW = XtVaCreateManagedWidget("ccView", xmToggleButtonWidgetClass, selectBox, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, RADIO_INDENT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, etDialog.statusW,
	                                       XmNlabelString, s1 = XmStringCreateSimpleEx("ClearCase view tag (%c) "), XmNmnemonic, 'C', nullptr);

	XtAddCallback(etDialog.ccW, XmNvalueChangedCallback, toggleClearCaseCB, nullptr);

	XmStringFree(s1);

	etDialog.dirW = XtVaCreateManagedWidget("directory", xmToggleButtonWidgetClass, selectBox, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, RADIO_INDENT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, etDialog.ccW,
	                                        XmNlabelString, s1 = XmStringCreateSimpleEx("Directory (%d),"), XmNmnemonic, 'D', nullptr);
	XtAddCallback(etDialog.dirW, XmNvalueChangedCallback, toggleDirectoryCB, nullptr);
	XmStringFree(s1);

	XtVaGetValues(etDialog.fileW, XmNheight, &radioHeight, nullptr);
	etDialog.mdirW = XtVaCreateManagedWidget("componentLab", xmLabelGadgetClass, selectBox, XmNheight, radioHeight, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, etDialog.dirW, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget,
	                                         etDialog.ccW, XmNlabelString, s1 = XmStringCreateSimpleEx("max. components: "), XmNmnemonic, 'x', nullptr);
	XmStringFree(s1);

	etDialog.ndirW = XtVaCreateManagedWidget("dircomp", xmTextWidgetClass, selectBox, XmNcolumns, 1, XmNmaxLength, 1, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, etDialog.mdirW, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget,
	                                         etDialog.ccW, nullptr);
	XtAddCallback(etDialog.ndirW, XmNvalueChangedCallback, enterMaxDirCB, nullptr);
	RemapDeleteKey(etDialog.ndirW);
	XtVaSetValues(etDialog.mdirW, XmNuserData, etDialog.ndirW, nullptr); /* mnemonic processing */

	XtVaGetValues(etDialog.ndirW, XmNheight, &textHeight, nullptr);
	XtVaSetValues(etDialog.dirW, XmNheight, textHeight, nullptr);
	XtVaSetValues(etDialog.mdirW, XmNheight, textHeight, nullptr);

	etDialog.hostW = XtVaCreateManagedWidget("host", xmToggleButtonWidgetClass, selectBox, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 50 + RADIO_INDENT, XmNtopAttachment, XmATTACH_FORM, XmNlabelString,
	                                         s1 = XmStringCreateSimpleEx("Host name (%h)"), XmNmnemonic, 'H', nullptr);
	XtAddCallback(etDialog.hostW, XmNvalueChangedCallback, toggleHostCB, nullptr);
	XmStringFree(s1);

	etDialog.nameW = XtVaCreateManagedWidget("name", xmToggleButtonWidgetClass, selectBox, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 50 + RADIO_INDENT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, etDialog.hostW,
	                                         XmNlabelString, s1 = XmStringCreateSimpleEx("User name (%u)"), XmNmnemonic, 'U', nullptr);
	XtAddCallback(etDialog.nameW, XmNvalueChangedCallback, toggleUserCB, nullptr);
	XmStringFree(s1);

	etDialog.serverW = XtVaCreateManagedWidget("server", xmToggleButtonWidgetClass, selectBox, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 50 + RADIO_INDENT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, etDialog.nameW,
	                                           XmNlabelString, s1 = XmStringCreateSimpleEx("NEdit server name (%s)"), XmNmnemonic, 's', nullptr);
	XtAddCallback(etDialog.serverW, XmNvalueChangedCallback, toggleServerCB, nullptr);
	XmStringFree(s1);

	formatLbl = XtVaCreateManagedWidget("formatLbl", xmLabelGadgetClass, selectForm, XmNlabelString, s1 = XmStringCreateSimpleEx("Format:  "), XmNmnemonic, 'r', XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LEFT_MARGIN_POS,
	                                    XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, selectBox, XmNbottomAttachment, XmATTACH_FORM, nullptr);
	XmStringFree(s1);
	etDialog.formatW = XtVaCreateManagedWidget("format", xmTextWidgetClass, selectForm, XmNmaxLength, WINDOWTITLE_MAX_LEN, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, selectBox, XmNtopOffset, 5, XmNleftAttachment, XmATTACH_WIDGET,
	                                           XmNleftWidget, formatLbl, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, 5, nullptr);
	RemapDeleteKey(etDialog.formatW);
	XtVaSetValues(formatLbl, XmNuserData, etDialog.formatW, nullptr);
	XtAddCallback(etDialog.formatW, XmNvalueChangedCallback, formatChangedCB, nullptr);

	XtVaGetValues(etDialog.formatW, XmNheight, &textHeight, nullptr);
	XtVaSetValues(formatLbl, XmNheight, textHeight, nullptr);

	previewFrame = XtVaCreateManagedWidget("previewFrame", xmFrameWidgetClass, etDialog.form, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, selectFrame, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LEFT_MARGIN_POS,
	                                       XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, nullptr);

	XtVaCreateManagedWidget("previewLabel", xmLabelGadgetClass, previewFrame, XmNlabelString, s1 = XmStringCreateSimpleEx("Preview"), XmNchildType, XmFRAME_TITLE_CHILD, XmNchildHorizontalAlignment, XmALIGNMENT_BEGINNING, nullptr);
	XmStringFree(s1);

	previewForm = XtVaCreateManagedWidget("previewForm", xmFormWidgetClass, previewFrame, XmNleftAttachment, XmATTACH_FORM, XmNleftPosition, LEFT_MARGIN_POS, XmNtopAttachment, XmATTACH_FORM, XmNtopOffset, V_MARGIN, XmNrightAttachment,
	                                      XmATTACH_FORM, XmNrightPosition, RIGHT_MARGIN_POS, nullptr);

	/* Copy a variable width font from one of the labels to use for the
	   preview (no editing is allowed, and with a fixed size font the
	   preview easily gets partially obscured). Also copy the form background
	   color to make it clear that this field is not editable */
	XtVaGetValues(formatLbl, XmNfontList, &fontList, nullptr);
	XtVaGetValues(previewForm, XmNbackground, &background, nullptr);

	etDialog.previewW = XtVaCreateManagedWidget("sample", xmTextFieldWidgetClass, previewForm, XmNeditable, False, XmNcursorPositionVisible, False, XmNtopAttachment, XmATTACH_FORM, XmNleftAttachment, XmATTACH_FORM, XmNleftOffset, V_MARGIN,
	                                            XmNrightAttachment, XmATTACH_FORM, XmNrightOffset, V_MARGIN, XmNfontList, fontList, XmNbackground, background, nullptr);

	previewBox = XtVaCreateManagedWidget("previewBox", xmFormWidgetClass, previewForm, XmNorientation, XmHORIZONTAL, XmNpacking, XmPACK_TIGHT, XmNradioBehavior, False, XmNleftAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_FORM,
	                                     XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, etDialog.previewW, nullptr);

	testLbl = XtVaCreateManagedWidget("testLabel", xmLabelGadgetClass, previewBox, XmNlabelString, s1 = XmStringCreateSimpleEx("Test settings:  "), XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LEFT_MARGIN_POS, XmNtopOffset, 5,
	                                  XmNbottomOffset, 5, XmNtopAttachment, XmATTACH_FORM, nullptr);
	XmStringFree(s1);

	etDialog.oFileChangedW = XtVaCreateManagedWidget("fileChanged", xmToggleButtonWidgetClass, previewBox, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, RADIO_INDENT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, testLbl,
	                                                 XmNlabelString, s1 = XmStringCreateSimpleEx("File modified"), XmNmnemonic, 'o', nullptr);
	XtAddCallback(etDialog.oFileChangedW, XmNvalueChangedCallback, fileChangedCB, nullptr);
	XmStringFree(s1);

	etDialog.oFileReadOnlyW = XtVaCreateManagedWidget("fileReadOnly", xmToggleButtonWidgetClass, previewBox, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, etDialog.oFileChangedW, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget,
	                                                  testLbl, XmNlabelString, s1 = XmStringCreateSimpleEx("File read only"), XmNmnemonic, 'n', nullptr);
	XtAddCallback(etDialog.oFileReadOnlyW, XmNvalueChangedCallback, fileReadOnlyCB, nullptr);
	XmStringFree(s1);

	etDialog.oFileLockedW = XtVaCreateManagedWidget("fileLocked", xmToggleButtonWidgetClass, previewBox, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, etDialog.oFileReadOnlyW, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, testLbl,
	                                                XmNlabelString, s1 = XmStringCreateSimpleEx("File locked"), XmNmnemonic, 'l', nullptr);
	XtAddCallback(etDialog.oFileLockedW, XmNvalueChangedCallback, fileLockedCB, nullptr);
	XmStringFree(s1);

	etDialog.oServerNameW = XtVaCreateManagedWidget("servernameSet", xmToggleButtonWidgetClass, previewBox, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, RADIO_INDENT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget,
	                                                etDialog.oFileChangedW, XmNlabelString, s1 = XmStringCreateSimpleEx("Server name present"), XmNmnemonic, 'v', nullptr);
	XtAddCallback(etDialog.oServerNameW, XmNvalueChangedCallback, serverNameCB, nullptr);
	XmStringFree(s1);

	etDialog.oCcViewTagW = XtVaCreateManagedWidget("ccViewTagSet", xmToggleButtonWidgetClass, previewBox, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, RADIO_INDENT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget,
	                                               etDialog.oServerNameW, XmNlabelString, s1 = XmStringCreateSimpleEx("CC view tag present"),

	                                               XmNset, GetClearCaseViewTag() != nullptr,

	                                               XmNmnemonic, 'w', nullptr);

	XtAddCallback(etDialog.oCcViewTagW, XmNvalueChangedCallback, ccViewTagCB, nullptr);

	XmStringFree(s1);

	etDialog.oServerEqualViewW = XtVaCreateManagedWidget("serverEqualView", xmToggleButtonWidgetClass, previewBox, XmNleftAttachment, XmATTACH_WIDGET, XmNleftWidget, etDialog.oCcViewTagW, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget,
	                                                     etDialog.oServerNameW, XmNlabelString, s1 = XmStringCreateSimpleEx("Server name equals CC view tag  "), XmNmnemonic, 'q', nullptr);

	XtAddCallback(etDialog.oServerEqualViewW, XmNvalueChangedCallback, serverEqualViewCB, nullptr);

	XmStringFree(s1);

	etDialog.oDirW = XtVaCreateManagedWidget("pathSet", xmToggleButtonWidgetClass, previewBox, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, RADIO_INDENT, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, etDialog.oCcViewTagW,
	                                         XmNlabelString, s1 = XmStringCreateSimpleEx("Directory present"), XmNmnemonic, 'i', nullptr);
	XtAddCallback(etDialog.oDirW, XmNvalueChangedCallback, formatChangedCB, nullptr);
	XmStringFree(s1);

	/* Button box */
	buttonForm = XtVaCreateManagedWidget("buttonForm", xmFormWidgetClass, etDialog.form, XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, LEFT_MARGIN_POS, XmNtopAttachment, XmATTACH_WIDGET, XmNtopWidget, previewFrame, XmNtopOffset,
	                                     V_MARGIN, XmNbottomOffset, V_MARGIN, XmNbottomAttachment, XmATTACH_FORM, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, RIGHT_MARGIN_POS, nullptr);

	applyBtn = XtVaCreateManagedWidget("apply", xmPushButtonWidgetClass, buttonForm, XmNhighlightThickness, 2, XmNlabelString, s1 = XmStringCreateSimpleEx("Apply"), XmNshowAsDefault, (short)1, XmNleftAttachment, XmATTACH_POSITION,
	                                   XmNleftPosition, 6, XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, 25, XmNbottomAttachment, XmATTACH_FORM, nullptr);
	XtAddCallback(applyBtn, XmNactivateCallback, applyCB, nullptr);
	XmStringFree(s1);
	XtVaGetValues(applyBtn, XmNshadowThickness, &shadowThickness, nullptr);
	defaultBtnOffset = shadowThickness + 4;

	closeBtn = XtVaCreateManagedWidget("close", xmPushButtonWidgetClass, buttonForm, XmNhighlightThickness, 2, XmNlabelString, s1 = XmStringCreateSimpleEx("Close"), XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 52,
	                                   XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, 71, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, defaultBtnOffset, nullptr);
	XtAddCallback(closeBtn, XmNactivateCallback, closeCB, nullptr);
	XmStringFree(s1);

	restoreBtn = XtVaCreateManagedWidget("restore", xmPushButtonWidgetClass, buttonForm, XmNhighlightThickness, 2, XmNlabelString, s1 = XmStringCreateSimpleEx("Default"), XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 29,
	                                     XmNrightAttachment, XmATTACH_POSITION, XmNrightPosition, 48, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, defaultBtnOffset, XmNmnemonic, 'e', nullptr);
	XtAddCallback(restoreBtn, XmNactivateCallback, restoreCB, nullptr);
	XmStringFree(s1);

	helpBtn = XtVaCreateManagedWidget("help", xmPushButtonWidgetClass, buttonForm, XmNhighlightThickness, 2, XmNlabelString, s1 = XmStringCreateSimpleEx("Help"), XmNleftAttachment, XmATTACH_POSITION, XmNleftPosition, 75, XmNrightAttachment,
	                                  XmATTACH_POSITION, XmNrightPosition, 94, XmNbottomAttachment, XmATTACH_FORM, XmNbottomOffset, defaultBtnOffset, XmNmnemonic, 'p', nullptr);
	XtAddCallback(helpBtn, XmNactivateCallback, helpCB, nullptr);
	XmStringFree(s1);

	/* Set initial default button */
	XtVaSetValues(etDialog.form, XmNdefaultButton, applyBtn, nullptr);
	XtVaSetValues(etDialog.form, XmNcancelButton, closeBtn, nullptr);

	/* Handle mnemonic selection of buttons and focus to dialog */
	AddDialogMnemonicHandler(etDialog.form, FALSE);

	etDialog.suppressFormatUpdate = FALSE;
}

void Document::EditCustomTitleFormat() {
	/* copy attributes from current this so that we can use as many
	 * 'real world' defaults as possible when testing the effect
	 * of different formatting strings.
	 */
	strcpy(etDialog.path, this->path_);
	strcpy(etDialog.filename, this->filename_);
	strcpy(etDialog.viewTag, GetClearCaseViewTag() != nullptr ? GetClearCaseViewTag() : "viewtag");
	strcpy(etDialog.serverName, IsServer ? GetPrefServerName() : "servername");
	etDialog.isServer = IsServer;
	etDialog.filenameSet = this->filenameSet_;
	etDialog.lockReasons = this->lockReasons_;
	etDialog.fileChanged = this->fileChanged_;

	if (etDialog.window != this && etDialog.form) {
		/* Destroy the dialog owned by the other this.
		   Note: don't rely on the destroy event handler to reset the
		         form. Events are handled asynchronously, so the old dialog
		         may continue to live for a while. */
		XtDestroyWidget(etDialog.form);
		etDialog.form = nullptr;
	}

	etDialog.window = this;

	/* Create the dialog if it doesn't already exist */
	if(!etDialog.form) {
		createEditTitleDialog(this->shell_);
	} else {
		/* If the this is already up, just pop it to the top */
		if (XtIsManaged(etDialog.form)) {

			RaiseDialogWindow(XtParent(etDialog.form));

			/* force update of the dialog */
			setToggleButtons();
			formatChangedCB(nullptr, nullptr, nullptr);
			return;
		}
	}

	/* set initial value of format field */
	XmTextSetStringEx(etDialog.formatW, GetPrefTitleFormat());

	/* force update of the dialog */
	setToggleButtons();
	formatChangedCB(nullptr, nullptr, nullptr);

	/* put up dialog and wait for user to press ok or cancel */
	ManageDialogCenteredOnPointer(etDialog.form);
}
