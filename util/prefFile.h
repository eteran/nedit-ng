/*******************************************************************************
*                                                                              *
* prefFile.h -- Nirvana Editor Preference File Header File                     *
*                                                                              *
* Copyright 2002 The NEdit Developers                                          *
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
*******************************************************************************/

#ifndef PREFFILE_H_
#define PREFFILE_H_

#include <X11/Intrinsic.h>
#include <string>
#include <cassert>

enum PrefDataTypes {
	PREF_INT,          
	PREF_BOOLEAN,      
	PREF_ENUM,         
	PREF_STRING,       
	PREF_ALLOC_STRING,
	PREF_STD_STRING
};

struct PrefDescripRec {
	std::string   name;
	std::string   clazz;
	PrefDataTypes dataType;
	const char *  defaultString;
	
	// where shall we write the value?
	union Value {
	
		Value(char *s)        : str(s)      { assert(s);  } // reading will read into this string
		Value(char **sp)      : str_ptr(sp) { assert(sp); } // reading will allocate a new string and assign it to *sp
		Value(int  *n)        : number(n)   { assert(n);  } // reading will read into this int
		Value(bool *b)        : boolean(b)  { assert(b);  } // reading will read into this bool
		Value(std::string *s) : string(s)   { assert(s);  } // reading will read into this string
	
		char * str;
		char **str_ptr;
		int  * number;
		bool * boolean;
		std::string *string;
	} valueAddr;
	
	// a parameter for the value
	// so far:
	// nullptr      = None
	// size_t       = size of the buffer supplies as the value
	// const char** = null-terminated array of strings representing an enum
	union Arg {
		Arg(size_t n)        : size(n) {}
		Arg(const char **sp) : str_ptr(sp) {}
		Arg(std::nullptr_t) {}
		
		const size_t size;
		const char **str_ptr;
	} arg;

	bool          save;
};


// name		"fileVersion", 
// class	"FileVersion", 
// type		PREF_STRING, 
// default	"", 
// addr		PrefData.fileVersion, 
// arg		(void *)sizeof(PrefData.fileVersion), 
// save		true



XrmDatabase CreatePreferencesDatabase(const char *fileName, const char *appName, XrmOptionDescList opTable, int nOptions, unsigned int *argcInOut, char **argvInOut);
void RestorePreferences(XrmDatabase prefDB, XrmDatabase appDB, const std::string &appName, const std::string &appClass, PrefDescripRec *rsrcDescrip, int nRsrc);
void OverlayPreferences(XrmDatabase prefDB, const std::string &appName, const std::string &appClass, PrefDescripRec *rsrcDescrip, int nRsrc);
void RestoreDefaultPreferences(PrefDescripRec *rsrcDescrip, int nRsrc);
bool SavePreferences(Display *display, const char *fileName, const char *fileHeader, const PrefDescripRec *rsrcDescrip, int nRsrc);

#endif
