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

#include "TextBuffer.h"
#include "UserMenuListElement.h"
#include <sys/types.h>
#include <list>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/XmStrDefs.h>
#include <sys/param.h>

#define NEDIT_VERSION 5
#define NEDIT_REVISION 6

/* Some default colors */
#define NEDIT_DEFAULT_FG "black"
#define NEDIT_DEFAULT_TEXT_BG "rgb:e5/e5/e5"
#define NEDIT_DEFAULT_SEL_FG "black"
#define NEDIT_DEFAULT_SEL_BG "rgb:cc/cc/cc"
#define NEDIT_DEFAULT_HI_FG "white" /* These are colors for flashing */
#define NEDIT_DEFAULT_HI_BG "red"   /*   matching parens. */
#define NEDIT_DEFAULT_LINENO_FG "black"
#define NEDIT_DEFAULT_CURSOR_FG "black"
#define NEDIT_DEFAULT_HELP_FG "black"
#define NEDIT_DEFAULT_HELP_BG "rgb:cc/cc/cc"

/* Tuning parameters */
#define SEARCHMAX 5119         /* Maximum length of search/replace strings */
#define MAX_SEARCH_HISTORY 100 /* Maximum length of search string history */
#define MAX_PANES 6            /* Max # of ADDITIONAL text editing panes  that can be added to a window */

#define AUTOSAVE_CHAR_LIMIT 80 /* set higher on VMS becaus saving is slower */
#define AUTOSAVE_OP_LIMIT   8  /* number of distinct editing operations user can do before NEdit gens. new backup file */
#define MAX_FONT_LEN 100    /* maximum length for a font name */
#define MAX_COLOR_LEN 30    /* maximum length for a color name */
#define MAX_MARKS 36        /* max. # of bookmarks (one per letter & #) */
#define MIN_LINE_NUM_COLS 4 /* Min. # of columns in line number display */
#define APP_NAME "nedit"    /* application name for loading resources */
#define APP_CLASS "NEdit"   /* application class for loading resources */



enum IndentStyle       { NO_AUTO_INDENT, AUTO_INDENT, SMART_INDENT };
enum WrapStyle         { NO_WRAP, NEWLINE_WRAP, CONTINUOUS_WRAP };
enum ShowMatchingStyle { NO_FLASH, FLASH_DELIMIT, FLASH_RANGE };
enum VirtKeyOverride   { VIRT_KEY_OVERRIDE_NEVER, VIRT_KEY_OVERRIDE_AUTO, VIRT_KEY_OVERRIDE_ALWAYS };

/*  This enum must be kept in parallel to the array TruncSubstitutionModes[]
    in preferences.c  */
enum TruncSubstitution { TRUNCSUBST_SILENT, TRUNCSUBST_FAIL, TRUNCSUBST_WARN, TRUNCSUBST_IGNORE };

#define NO_FLASH_STRING "off"
#define FLASH_DELIMIT_STRING "delimiter"
#define FLASH_RANGE_STRING "range"

#define CHARSET (XmStringCharSet) XmSTRING_DEFAULT_CHARSET

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

/* This handles all the different reasons files can be locked */
#define USER_LOCKED_BIT 0
#define PERM_LOCKED_BIT 1
#define TOO_MUCH_BINARY_DATA_LOCKED_BIT 2

#define LOCKED_BIT_TO_MASK(bitNum) (1 << (bitNum))
#define SET_LOCKED_BY_REASON(reasons, onOrOff, reasonBit) ((onOrOff) ? ((reasons) |= LOCKED_BIT_TO_MASK(reasonBit)) : ((reasons) &= ~LOCKED_BIT_TO_MASK(reasonBit)))

#define IS_USER_LOCKED(reasons) (((reasons)&LOCKED_BIT_TO_MASK(USER_LOCKED_BIT)) != 0)
#define SET_USER_LOCKED(reasons, onOrOff) SET_LOCKED_BY_REASON(reasons, onOrOff, USER_LOCKED_BIT)
#define IS_PERM_LOCKED(reasons) (((reasons)&LOCKED_BIT_TO_MASK(PERM_LOCKED_BIT)) != 0)
#define SET_PERM_LOCKED(reasons, onOrOff) SET_LOCKED_BY_REASON(reasons, onOrOff, PERM_LOCKED_BIT)
#define IS_TMBD_LOCKED(reasons) (((reasons)&LOCKED_BIT_TO_MASK(TOO_MUCH_BINARY_DATA_LOCKED_BIT)) != 0)
#define SET_TMBD_LOCKED(reasons, onOrOff) SET_LOCKED_BY_REASON(reasons, onOrOff, TOO_MUCH_BINARY_DATA_LOCKED_BIT)

#define IS_ANY_LOCKED_IGNORING_USER(reasons) (((reasons) & ~LOCKED_BIT_TO_MASK(USER_LOCKED_BIT)) != 0)
#define IS_ANY_LOCKED_IGNORING_PERM(reasons) (((reasons) & ~LOCKED_BIT_TO_MASK(PERM_LOCKED_BIT)) != 0)
#define IS_ANY_LOCKED(reasons) ((reasons) != 0)
#define CLEAR_ALL_LOCKS(reasons) ((reasons) = 0)

/* determine a safe size for a string to hold an integer-like number contained in xType */
#define TYPE_INT_STR_SIZE(xType) ((sizeof(xType) * 3) + 2)

struct UndoInfo;
struct Document;
struct UserMenuListElement;

/* Element in bookmark table */
struct Bookmark {
	char label;
	int cursorPos;
	TextSelection sel;
};

/* Identifiers for the different colors that can be adjusted. */
enum ColorTypes { TEXT_FG_COLOR, TEXT_BG_COLOR, SELECT_FG_COLOR, SELECT_BG_COLOR, HILITE_FG_COLOR, HILITE_BG_COLOR, LINENO_FG_COLOR, CURSOR_FG_COLOR, NUM_COLORS };


/* structure holding cache info about Shell and Macro menus, which are
   shared over all "tabbed" documents (needed to manage/unmanage this
   user definable menus when language mode changes) */
struct UserMenuCache {
	int umcLanguageMode;           /* language mode applied for shared user menus */
	Boolean umcShellMenuCreated;   /* indicating, if all shell menu items were created */
	Boolean umcMacroMenuCreated;   /* indicating, if all macro menu items were created */
	
	UserMenuList umcShellMenuList; /* list of all shell menu items */
	UserMenuList umcMacroMenuList; /* list of all macro menu items */
};

/* structure holding cache info about Background menu, which is
   owned by each document individually (needed to manage/unmanage this
   user definable menu when language mode changes) */
struct UserBGMenuCache {
	int ubmcLanguageMode;      /* language mode applied for background user menu */
	Boolean ubmcMenuCreated;   /* indicating, if all background menu items were created */
	UserMenuList ubmcMenuList; /* list of all background menu items */
};



extern Document *WindowList;
extern Display *TheDisplay;
extern Widget TheAppShell;
extern bool IsServer;

#endif
