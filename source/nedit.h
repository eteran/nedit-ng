/*******************************************************************************
*                                                                              *
* nedit.h -- Nirvana Editor Common Header File                                 *
*                                                                              *
* Copyright 2004 The NEdit Developers                                          *
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

#ifndef NEDIT_H_
#define NEDIT_H_

#include "TextSelection.h"
#include "ShowMatchingStyle.h"
#include <QLinkedList>

#define NEDIT_VERSION  5
#define NEDIT_REVISION 6

/* Some default colors */
#define NEDIT_DEFAULT_FG        "#221f1e"
#define NEDIT_DEFAULT_TEXT_BG   "#d6d2d0"
#define NEDIT_DEFAULT_SEL_FG    "#ffffff"
#define NEDIT_DEFAULT_SEL_BG    "#43ace8"
#define NEDIT_DEFAULT_HI_FG     "white"        /* These are colors for flashing */
#define NEDIT_DEFAULT_HI_BG     "red"          /* matching parens. */
#define NEDIT_DEFAULT_LINENO_FG "black"
#define NEDIT_DEFAULT_CURSOR_FG "black"

#define NEDIT_DEFAULT_HELP_FG   "black"
#define NEDIT_DEFAULT_HELP_BG   "#cccccc"

/* Tuning parameters */
#define SEARCHMAX 5119         /* Maximum length of search/replace strings */
#define MAX_PANES 6            /* Max # of ADDITIONAL text editing panes  that can be added to a window */

#define AUTOSAVE_CHAR_LIMIT 80 /* set higher on VMS becaus saving is slower */
#define AUTOSAVE_OP_LIMIT   8  /* number of distinct editing operations user can do before NEdit gens. new backup file */
#define MIN_LINE_NUM_COLS 4 /* Min. # of columns in line number display */
#define APP_NAME "nedit"    /* application name for loading resources */
#define APP_CLASS "NEdit"   /* application class for loading resources */

/*  This enum must be kept in parallel to the array TruncSubstitutionModes[]
    in preferences.c  */
enum TruncSubstitution {
    TRUNCSUBST_SILENT,
    TRUNCSUBST_FAIL,
    TRUNCSUBST_WARN,
    TRUNCSUBST_IGNORE
};

enum virtKeyOverride {
	VIRT_KEY_OVERRIDE_NEVER, 
	VIRT_KEY_OVERRIDE_AUTO,
    VIRT_KEY_OVERRIDE_ALWAYS
};

#define NO_FLASH_STRING      "off"
#define FLASH_DELIMIT_STRING "delimiter"
#define FLASH_RANGE_STRING   "range"



#define SET_ONE_RSRC(widget, name, newValue)                                                                                                                                                                                                   \
	{                                                                                                                                                                                                                                          \
		static Arg args[1] = {{name, (XtArgVal)0}};                                                                                                                                                                                            \
		args[0].value = (XtArgVal)newValue;                                                                                                                                                                                                    \
		XtSetValues(widget, args, 1);                                                                                                                                                                                                          \
	}

#define GET_ONE_RSRC(widget, name, valueAddr)                                                                                                                                                                                                  \
	{                                                                                                                                                                                                                                          \
		static Arg args[1] = {{name, (XtArgVal)0}};                                                                                                                                                                                            \
		args[0].value = (XtArgVal)valueAddr;                                                                                                                                                                                                   \
		XtGetValues(widget, args, 1);                                                                                                                                                                                                          \
	}

/* determine a safe size for a string to hold an integer-like number contained in xType */
#define TYPE_INT_STR_SIZE(xType) ((sizeof(xType) * 3) + 2)

class UndoInfo;


/* Identifiers for the different colors that can be adjusted. */
enum ColorTypes {
	TEXT_FG_COLOR, 
	TEXT_BG_COLOR, 
	SELECT_FG_COLOR, 
	SELECT_BG_COLOR, 
	HILITE_FG_COLOR, 
	HILITE_BG_COLOR, 
	LINENO_FG_COLOR, 
	CURSOR_FG_COLOR, 
	NUM_COLORS
};


//extern Display *TheDisplay;
extern bool IsServer;

#endif
