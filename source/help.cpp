/*******************************************************************************
*                                                                              *
* help.c -- Nirvana Editor help display                                        *
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
* September 10, 1991                                                           *
*                                                                              *
* Written by Mark Edel, mostly rewritten by Steve Haehn for new help system,   *
* December, 2001                                                               *
*                                                                              *
*******************************************************************************/

#include "help.h"
#include "preferences.h"
#include "help_data.h"
#include "../util/misc.h"
#include "../util/system.h"

#include <cstring>

#include <Xm/Xm.h>


#ifdef HAVE__XMVERSIONSTRING
extern char _XmVersionString[];
#else
static char _XmVersionString[] = "unknown";
#endif


static const char *getBuildInfo(void) {
	static char *bldInfoString = nullptr;
	static const char bldFormat[] = "%s\n"
	                               "     Built on: %s, %s, %s\n"
	                               "     Built at: %s, %s\n"
	                               "   With Motif: %s%d.%d.%d [%s]\n"
	                               "Running Motif: %d.%d [%s]\n"
	                               "       Server: %s %d\n"
	                               "       Visual: %s\n"
	                               "       Locale: %s\n";


	if(!bldInfoString) {
		const char *locale;
		char visualStr[500] = "<unknown>";

		if (TheDisplay) {
			
			static const char *visualClass[] = { "StaticGray", "GrayScale", "StaticColor", "PseudoColor", "TrueColor", "DirectColor" };
			
			Visual *visual;
			int depth;
			Colormap map;

			bool usingDefaultVisual = FindBestVisual(TheDisplay, APP_NAME, APP_CLASS, &visual, &depth, &map);
			snprintf(visualStr, sizeof(visualStr), "%d-bit %s (ID %#lx%s)", depth, visualClass[visual->c_class], visual->visualid, usingDefaultVisual ? ", Default" : "");
		}

		bldInfoString = new char[sizeof(bldFormat) + 1024];
		locale = setlocale(LC_MESSAGES, "");

		sprintf(bldInfoString,
			bldFormat, 
			NEditVersion, 
			COMPILE_OS, 
			COMPILE_MACHINE, 
			COMPILE_COMPILER, 
			__DATE__, 
			__TIME__, 
			"", 
			XmVERSION, 
			XmREVISION, 
			XmUPDATE_LEVEL, 
			XmVERSION_STRING, 
			xmUseVersion / 1000, 
			xmUseVersion % 1000,
			_XmVersionString, 
			(!TheDisplay ? "<unknown>" : ServerVendor(TheDisplay)),
			(!TheDisplay ? 0 : VendorRelease(TheDisplay)), 
			visualStr, 
			locale ? locale : "None");


		atexit([](){
			// This keeps memory leak detectors happy 
			delete [] bldInfoString;
		});
	}

	return bldInfoString;
}

/*
** Help fonts are not loaded until they're actually needed.  This function
** checks if the style's font is loaded, and loads it if it's not.
*/
// Print version info to stdout 
void PrintVersion(void) {
	const char *text = getBuildInfo();
	puts(text);
}
