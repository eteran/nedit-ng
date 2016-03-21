/*******************************************************************************
*                                                                              *
* prefFile.c -- Nirvana utilities for providing application preferences files  *
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
* June 3, 1993                                                                 *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include "prefFile.h"
#include "fileUtils.h"
#include "utils.h"
#include "MotifHelper.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <sys/param.h>
#include <Xm/Xm.h>

namespace {

const int BooleanStringCount = 13;
const char *TrueStrings[BooleanStringCount]  = {"True",  "true",  "TRUE",  "T", "t", "Yes", "yes", "YES", "y", "Y", "on",  "On",  "ON" };
const char *FalseStrings[BooleanStringCount] = {"False", "false", "FALSE", "F", "f", "No",  "no",  "NO",  "n", "N", "off", "Off", "OFF"};

/*
** Remove the white space (blanks and tabs) from a string and return
** the result in a newly allocated string as the function value
*/
std::string removeWhiteSpaceEx(view::string_view string) {

	std::string outString;
	outString.reserve(string.size());
	
	auto outPtr = std::back_inserter(outString);

	for (char ch : string) {
		if (ch != ' ' && ch != '\t') {
			*outPtr++ = ch;
		}
	}
	
	return outString;
}

bool stringToPref(const char *string, PrefDescripRec *rsrcDescrip) {
	
	switch (rsrcDescrip->dataType) {
	case PREF_INT:
		{
			char *endPtr;
			const std::string cleanStr = removeWhiteSpaceEx(string);
			*rsrcDescrip->valueAddr.number = std::strtol(cleanStr.c_str(), &endPtr, 10);

			if (cleanStr.empty()) { /* String is empty */
				*rsrcDescrip->valueAddr.number = 0;
				return false;
			} else if (*endPtr != '\0') { /* Whole string not parsed */
				*rsrcDescrip->valueAddr.number = 0;
				return false;
			}
			return true;
		}
	case PREF_BOOLEAN:
		{
			const std::string cleanStr = removeWhiteSpaceEx(string);
			for (int i = 0; i < BooleanStringCount; i++) {
				if (cleanStr == TrueStrings[i]) {
					*rsrcDescrip->valueAddr.boolean = true;
					return true;
				}

				if (cleanStr == FalseStrings[i]) {
					*rsrcDescrip->valueAddr.boolean = false;
					return true;
				}
			}
			*rsrcDescrip->valueAddr.boolean = false;
			return false;
		}
	case PREF_ENUM:
		{
			const std::string cleanStr = removeWhiteSpaceEx(string);
			const char **enumStrings = rsrcDescrip->arg.str_ptr;
			
			for (int i = 0; enumStrings[i] != nullptr; i++) {
				if (cleanStr == enumStrings[i]) {
					*rsrcDescrip->valueAddr.number = i;
					return true;
				}
			}
			*rsrcDescrip->valueAddr.number = 0;
			return false;
		}
		
	case PREF_STRING:
		{
			if (strlen(string) >= rsrcDescrip->arg.size) {
				return false;
			}
			
			strncpy(rsrcDescrip->valueAddr.str, string, rsrcDescrip->arg.size);
			return true;
		}
		
	case PREF_ALLOC_STRING:
		{
			*rsrcDescrip->valueAddr.str_ptr = XtStringDup(string);
			return true;
		}
	case PREF_STD_STRING:
		{
			*(rsrcDescrip->valueAddr.string) = QLatin1String(string);
			return true;
		}
	default:
		assert(0 && "reading setting of unknown type");		
	}
	
	return false;
}

void readPrefs(XrmDatabase prefDB, XrmDatabase appDB, const std::string &appName, const std::string &appClass, PrefDescripRec *rsrcDescrip, int nRsrc, bool overlay) {


	/* read each resource, trying first the preferences file database, then
	   the application database, then the default value if neither are found */
	for (int i = 0; i < nRsrc; i++) {

		char rsrcName[256];
		char rsrcClass[256];
		const char *valueString;
		char *type;
		XrmValue rsrcValue;

		snprintf(rsrcName,  sizeof(rsrcName),  "%s.%s", appName.c_str(),  rsrcDescrip[i].name.c_str());
		snprintf(rsrcClass, sizeof(rsrcClass), "%s.%s", appClass.c_str(), rsrcDescrip[i].clazz.c_str());
		
		if (prefDB != nullptr && XrmGetResource(prefDB, rsrcName, rsrcClass, &type, &rsrcValue)) {
			if (strcmp(type, XmRString)) {
				fprintf(stderr, "nedit: Internal Error: Unexpected resource type, %s\n", type);
				return;
			}
			valueString = rsrcValue.addr;
		} else if (XrmGetResource(appDB, rsrcName, rsrcClass, &type, &rsrcValue)) {
			if (strcmp(type, XmRString)) {
				fprintf(stderr, "nedit: Internal Error: Unexpected resource type, %s\n", type);
				return;
			}
			valueString = rsrcValue.addr;
		} else {
			valueString = rsrcDescrip[i].defaultString;
		}
		
		if (overlay && valueString == rsrcDescrip[i].defaultString) {
			continue;
		}
		
		if (!stringToPref(valueString, &rsrcDescrip[i])) {
			fprintf(stderr, "nedit: Could not read value of resource %s\n", rsrcName);
		}
	}
}

}

/*
** Preferences File
**
** An application maintains a preferences file so that users can
** quickly save and restore program options from within a program,
** without being forced to learn the X resource mechanism.
**
** Preference files are the same format as X resource files, and
** are read using the X resource file reader.  X-savvy users are allowed
** to move resources out of a preferences file to their X resource
** files.  They would do so if they wanted to attach server-specific
** preferences (such as fonts and colors) to different X servers, or to
** combine additional preferences served only by X resources with those
** provided by the program's menus.
*/

/*
** Preference description table
**
** A preference description table contains the information necessary
** to read preference resources and store their values in a data
** structure.  The table can, so far, describe four types
** of values (this will probably be expanded in the future to include
** more types): ints, booleans, enumerations, and strings.  Each entry
** includes the name and class for saving and restoring the parameter
** in X database format, the data type, a default value in the form of
** a character string, and the address where the parameter value is
** be stored.  Strings and enumerations take an additional argument.
** For strings, it is the maximum length string that can safely be
** stored or nullptr to indicate that new space should be allocated and a
** pointer to it stored in the value address.  For enums, it is an array
** of string pointers to the names of each of its possible values. The
** last value in a preference record is a flag for determining whether
** the value should be written to the save file by SavePreferences.
*/

/*
** CreatePreferencesDatabase
**
** Process a preferences file and the command line options pertaining to
** the X resources used to set those preferences.  Create an X database
** of the results.  The reason for this odd set of functionality is
** to process command line options before XtDisplayInitialize reads them
** into the application database that the toolkit attaches to the display.
** This allows command line arguments to properly override values specified
** in the preferences file.
**
** 	fileName	Name only of the preferences file to be found
**			in the user's home directory
**	appName		Application name to use in reading the preference
**			resources
**	opTable		Xrm command line option table for the resources
**			used in the preferences file ONLY.  Command line
**			options for other X resources should be processed
**			by XtDisplayInitialize.
**	nOptions	Number of items in opTable
**	argcInOut	Address of argument count.  This will be altered
**			to remove the command line options that are
**			recognized in the option table.
**	argvInOut	Argument vector.  Will be altered as argcInOut.
*/
XrmDatabase CreatePreferencesDatabase(const char *fullName, const char *appName, XrmOptionDescList opTable, int nOptions, unsigned int *argcInOut, char **argvInOut) {
	XrmDatabase db;
	int argcCopy;
	static XrmOptionDescRec xrmOnlyTable[] = {{(char *)"-xrm", nullptr, XrmoptionResArg, nullptr}};

	/* read the preferences file into an X database.
	   On failure prefDB will be nullptr. */
	if(!fullName) {
		db = nullptr;
	} else {
		QString fileString = ReadAnyTextFileEx(fullName, False);
		if(fileString.isNull()) {
			db = nullptr;
		} else {
			db = XrmGetStringDatabase(fileString.toLatin1().data());

			/*  Add a resource to the database which remembers that
			    the file is read, so that NEdit will know it.  */
			auto rsrcName = new char[strlen(appName) + 14];
			sprintf(rsrcName, "%s.prefFileRead", appName);
			XrmPutStringResource(&db, rsrcName, "True");
			delete [] rsrcName;
		}
	}

	/* parse the command line, storing results in the preferences database */
	XrmParseCommand(&db, opTable, nOptions, appName, (int *)argcInOut, argvInOut);

	/* process -xrm (resource setting by resource name) arguments so those
	   pertaining to preference resources will be included in the database.
	   Don't remove -xrm arguments from the argument vector, however, so
	   XtDisplayInitialize can still read the non-preference resources */
	auto  argvCopy = new char *[*argcInOut];
	memcpy(argvCopy, argvInOut, sizeof(char *) * *argcInOut);
	argcCopy = *argcInOut;
	XrmParseCommand(&db, xrmOnlyTable, XtNumber(xrmOnlyTable), appName, &argcCopy, argvCopy);
	delete [] argvCopy;
	return db;
}

/*
** RestorePreferences
**
** Fill in preferences data from two X databases, values in prefDB taking
** precidence over those in appDB.
*/
void RestorePreferences(XrmDatabase prefDB, XrmDatabase appDB, const std::string &appName, const std::string &appClass, PrefDescripRec *rsrcDescrip, int nRsrc) {
	readPrefs(prefDB, appDB, appName, appClass, rsrcDescrip, nRsrc, false);
}

/*
** OverlayPreferences
**
** Incorporate preference specified in database "prefDB", preserving (not
** restoring to default) existing preferences, not mentioned in "prefDB"
*/
void OverlayPreferences(XrmDatabase prefDB, const std::string &appName, const std::string &appClass, PrefDescripRec *rsrcDescrip, int nRsrc) {
	readPrefs(nullptr, prefDB, appName, appClass, rsrcDescrip, nRsrc, true);
}

/*
** RestoreDefaultPreferences
**
** Restore preferences to their default values as stored in rsrcDesrcip
*/
void RestoreDefaultPreferences(PrefDescripRec *rsrcDescrip, int nRsrc) {

	for (int i = 0; i < nRsrc; i++) {
		stringToPref(rsrcDescrip[i].defaultString, &rsrcDescrip[i]);
	}
}

/*
** SavePreferences
**
** Create or replace an application preference file according to
** the resource descriptions in rsrcDesrcip.
*/
bool SavePreferences(Display *display, const char *fullName, const char *fileHeader, const PrefDescripRec *rsrcDescrip, int nRsrc) {
	char *appName;
	char *appClass;
	FILE *fp;

	/* open the file */
	if ((fp = fopen(fullName, "w")) == nullptr) {
		return false;
	}

	/* write the file header text out to the file */
	fprintf(fp, "%s\n", fileHeader);

	/* write out the resources so they can be read by XrmGetFileDatabase */
	XtGetApplicationNameAndClass(display, &appName, &appClass);
	
	for (int i = 0; i < nRsrc; i++) {
		if (rsrcDescrip[i].save) {
			const int type = rsrcDescrip[i].dataType;
			fprintf(fp, "%s.%s: ", appName, rsrcDescrip[i].name.c_str());
						
			switch(type) {
			case PREF_STRING:
				fprintf(fp, "%s", rsrcDescrip[i].valueAddr.str);
				break;
			case PREF_ALLOC_STRING:
				fprintf(fp, "%s", *rsrcDescrip[i].valueAddr.str_ptr);
				break;
			case PREF_STD_STRING:
				fprintf(fp, "%s", (*rsrcDescrip[i].valueAddr.string).toLatin1().data());
				break;				
			case PREF_ENUM:
				{
					const char **enumStrings = rsrcDescrip[i].arg.str_ptr;
					fprintf(fp, "%s", enumStrings[*rsrcDescrip[i].valueAddr.number]);
				}
				break;
			case PREF_INT:
				fprintf(fp, "%d", *rsrcDescrip[i].valueAddr.number);
				break;
			case PREF_BOOLEAN:
				if (*rsrcDescrip[i].valueAddr.boolean) {
					fprintf(fp, "True");
				} else {
					fprintf(fp, "False");
				}
				break;
			default:
				assert(0 && "writing setting of unknown type");
			}
			

			fprintf(fp, "\n");
		}
	}
	
	fclose(fp);
	return true;
}

/*******************
Implementation Note:
Q: Why aren't you using the Xt type conversion services?
A: 1) To create a save file, you also need to convert values back to text form,
and there are no converters for that direction.  2) XtGetApplicationResources
can only be used on the resource database created by the X toolkit at
initialization time, and there is no way to intervene in the creation of
that database or store new resources in it reliably after it is created.
3) The alternative, XtConvertAndStore is not adequately documented.  The
toolkit mauual does not explain why it overwrites its input value structure.
4) XtGetApplicationResources and XtConvertAndStore do not work well together
because they use different storage strategies for certain data types.
*******************/
