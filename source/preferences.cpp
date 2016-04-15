/*******************************************************************************
*                                                                              *
* preferences.c -- Nirvana Editor preferences processing                       *
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
* April 20, 1993                                                               *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

#include <QMessageBox>
#include <QString>
#include <QPushButton>
#include <QtDebug>
#include "ui/DialogFontSelector.h"
#include "ui/DialogWrapMargin.h"
#include "ui/DialogLanguageModes.h"
#include "ui/DialogColors.h"
#include "LanguageMode.h"

#include "preferences.h"
#include "TextBuffer.h"
#include "nedit.h"
#include "menu.h"
#include "text.h"
#include "search.h"
#include "Document.h"
#include "window.h"
#include "userCmds.h"
#include "highlight.h"
#include "highlightData.h"
#include "help.h"
#include "regularExp.h"
#include "smartIndent.h"
#include "windowTitle.h"
#include "server.h"
#include "tags.h"
#include "MotifHelper.h"
#include "prefFile.h"
#include "misc.h"
#include "managedList.h"
#include "fileUtils.h"
#include "utils.h"

#include <cctype>
#include <pwd.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>
#include <sys/stat.h>

#include <sys/param.h>
#include "clearcase.h"

#include <Xm/Xm.h>
#include <Xm/SelectioB.h>
#include <Xm/Form.h>
#include <Xm/List.h>
#include <Xm/SeparatoG.h>
#include <Xm/LabelG.h>
#include <Xm/Label.h>
#include <Xm/PushBG.h>
#include <Xm/PushB.h>
#include <Xm/ToggleBG.h>
#include <Xm/ToggleB.h>
#include <Xm/RowColumn.h>
#include <Xm/CascadeBG.h>
#include <Xm/Frame.h>
#include <Xm/Text.h>

#define MENU_WIDGET(w) (XmGetPostedFromWidget(XtParent(w)))

#define PREF_FILE_VERSION "5.6"

// New styles added in 5.2 for auto-upgrade 
#define ADD_5_2_STYLES " Pointer:#660000:Bold\nRegex:#009944:Bold\nWarning:brown2:Italic"

// maximum number of word delimiters allowed (256 allows whole character set) 
#define MAX_WORD_DELIMITERS 256

// maximum number of file extensions allowed in a language mode 
#define MAX_FILE_EXTENSIONS 20

// Return values for checkFontStatus 
enum fontStatus { GOOD_FONT, BAD_PRIMARY, BAD_FONT, BAD_SIZE, BAD_SPACING };

/* enumerated type preference strings
** The order of the elements in this array must be exactly the same
** as the order of the corresponding integers of the enum SearchType
** defined in search.h (!!)
*/
static const char *SearchMethodStrings[] = {"Literal", "CaseSense", "RegExp", "LiteralWord", "CaseSenseWord", "RegExpNoCase", nullptr};

#ifdef REPLACE_SCOPE
/* enumerated default scope for replace dialog if a selection exists when
** the dialog is popped up.
*/
static const char *ReplaceDefScopeStrings[] = {"Window", "Selection", "Smart", nullptr};
#endif

#define N_WRAP_STYLES 3
static const char *AutoWrapTypes[N_WRAP_STYLES + 3] = {"None", "Newline", "Continuous", "True", "False", nullptr};
#define N_INDENT_STYLES 3
static const char *AutoIndentTypes[N_INDENT_STYLES + 3] = {"None", "Auto", "Smart", "True", "False", nullptr};
#define N_VIRTKEY_OVERRIDE_MODES 3
static const char *VirtKeyOverrideModes[N_VIRTKEY_OVERRIDE_MODES + 1] = {"Never", "Auto", "Always", nullptr};

#define N_SHOW_MATCHING_STYLES 3
/* For backward compatibility, "False" and "True" are still accepted.
   They are internally converted to "Off" and "Delimiter" respectively.
   NOTE: N_SHOW_MATCHING_STYLES must correspond to the number of
         _real_ matching styles, not counting False & True.
         False and True should also be the last ones in the list. */
static const char *ShowMatchingTypes[] = {"Off", "Delimiter", "Range", "False", "True", nullptr};

/*  This array must be kept in parallel to the enum truncSubstitution
    in nedit.h  */
static const char *TruncSubstitutionModes[] = {"Silent", "Fail", "Warn", "Ignore", nullptr};

/* suplement wrap and indent styles w/ a value meaning "use default" for
   the override fields in the language modes dialog */
#define DEFAULT_TAB_DIST -1
#define DEFAULT_EM_TAB_DIST -1

// list of available language modes and language specific preferences 
int NLanguageModes = 0;
LanguageMode *LanguageModes[MAX_LANGUAGE_MODES];

// Repository for simple preferences settings 
static struct prefData {
	int openInTab;         // open files in new tabs  
	int wrapStyle;         // what kind of wrapping to do 
	int wrapMargin;        // 0=wrap at window width, other=wrap margin 
	int autoIndent;        // style for auto-indent 
	int autoSave;          // whether automatic backup feature is on 
	int saveOldVersion;    // whether to preserve a copy of last version 
	int searchDlogs;       // whether to show explanatory search dialogs 
	int searchWrapBeep;    // 1=beep when search restarts at begin/end 
	int keepSearchDlogs;   // whether to retain find and replace dialogs 
	int searchWraps;       // whether to attempt search again if reach bof or eof 
	int statsLine;         // whether to show the statistics line 
	int iSearchLine;       // whether to show the incremental search line
	int tabBar;            // whether to show the tab bar 
	int tabBarHideOne;     // hide tab bar if only one document in window 
	int globalTabNavigate; // prev/next document across windows 
	int toolTips;          // whether to show the tooltips 
	int lineNums;          // whether to show line numbers 
	int pathInWindowsMenu; // whether to show path in windows menu 
	int warnFileMods;      // warn user if files externally modified 
	int warnRealFileMods;  // only warn if file contents modified 
	int warnExit;          // whether to warn on exit 
	int searchMethod;      // initial search method as a text string 
#ifdef REPLACE_SCOPE
	int replaceDefScope; // default replace scope if selection exists 
#endif
	int textRows;                     // initial window height in characters 
	int textCols;                     // initial window width in characters 
	int tabDist;                      // number of characters between tab stops 
	int emTabDist;                    // non-zero tab dist. if emulated tabs are on 
	int insertTabs;                   // whether to use tabs for padding 
	int showMatchingStyle;            // how to flash matching parenthesis 
	int matchSyntaxBased;             // use syntax info to match parenthesis 
	int highlightSyntax;              // whether to highlight syntax by default 
	int smartTags;                    // look for tag in current window first 
	int alwaysCheckRelativeTagsSpecs; // for every new opened file of session 
	int stickyCaseSenseBtn;           // whether Case Word Btn is sticky to Regex Btn 
	int prefFileRead;                 // detects whether a .nedit existed 
	int backlightChars;               // whether to apply character "backlighting" 
	char *backlightCharTypes;         // the backlighting color definitions 
	char fontString[MAX_FONT_LEN];    // names of fonts for text widget 
	char boldFontString[MAX_FONT_LEN];
	char italicFontString[MAX_FONT_LEN];
	char boldItalicFontString[MAX_FONT_LEN];
	XmFontList fontList; // XmFontLists corresp. to above named fonts 
	XFontStruct *boldFontStruct;
	XFontStruct *italicFontStruct;
	XFontStruct *boldItalicFontStruct;
	int sortTabs;                         // sort tabs alphabetically 
	int repositionDialogs;                // w. to reposition dialogs under the pointer 
	int autoScroll;                       // w. to autoscroll near top/bottom of screen 
	int autoScrollVPadding;               // how close to get before autoscrolling 
	int sortOpenPrevMenu;                 // whether to sort the "Open Previous" menu 
	int appendLF;                         // Whether to append LF at the end of each file 
	int mapDelete;                        // whether to map delete to backspace 
	int stdOpenDialog;                    // w. to retain redundant text field in Open 
	char tagFile[MAXPATHLEN];             // name of tags file to look for at startup 
	int maxPrevOpenFiles;                 // limit to size of Open Previous menu 
	int typingHidesPointer;               // hide mouse pointer when typing 
	char delimiters[MAX_WORD_DELIMITERS]; // punctuation characters 
	char shell[MAXPATHLEN + 1];           // shell to use for executing commands 
	char geometry[MAX_GEOM_STRING_LEN];   /* per-application geometry string,
	                                         only for the clueless */
	char serverName[MAXPATHLEN];          // server name for multiple servers per disp. 
	char bgMenuBtn[MAX_ACCEL_LEN];        /* X event description for triggering
	                                         posting of background menu */
	char fileVersion[6];                  /* Version of nedit which wrote the .nedit
	                                 file we're reading */
	int findReplaceUsesSelection;         /* whether the find replace dialog is automatically
	                                         loaded with the primary selection */
	int virtKeyOverride;                  /* Override Motif default virtual key bindings
	                             never, if invalid, or always */
	char titleFormat[MAX_TITLE_FORMAT_LEN];
	char helpFontNames[NUM_HELP_FONTS][MAX_FONT_LEN]; // fonts for help system 
	char helpLinkColor[MAX_COLOR_LEN];                // Color for hyperlinks in the help system 
	char colorNames[NUM_COLORS][MAX_COLOR_LEN];
	char tooltipBgColor[MAX_COLOR_LEN];
	int undoModifiesSelection;
	int focusOnRaise;
	Boolean honorSymlinks;
	int truncSubstitution;
	Boolean forceOSConversion;
} PrefData;

/* Temporary storage for preferences strings which are discarded after being
   read */
static struct {
	QString shellCmds;
	QString macroCmds;
	QString bgMenuCmds;
	QString highlight;
	QString language;
	QString styles;
	QString smartIndent;
	QString smartIndentCommon;
} TempStringPrefs;

// preference descriptions for SavePreferences and RestorePreferences. 
static PrefDescripRec PrefDescrip[] = {
    {"fileVersion", "FileVersion", PREF_STRING, "", PrefData.fileVersion, sizeof(PrefData.fileVersion), true},

#ifdef linux
    {"shellCommands", "ShellCommands", PREF_STD_STRING, "spell:Alt+B:s:EX:\n\
    cat>spellTmp; xterm -e ispell -x spellTmp; cat spellTmp; rm spellTmp\n\
    wc::w:ED:\nwc | awk '{print $1 \" lines, \" $2 \" words, \" $3 \" characters\"}'\n\
    sort::o:EX:\nsort\nnumber lines::n:AW:\nnl -ba\nmake:Alt+Z:m:W:\nmake\n\
    expand::p:EX:\nexpand\nunexpand::u:EX:\nunexpand\n",
     &TempStringPrefs.shellCmds, nullptr, true},
#elif __FreeBSD__
    {"shellCommands", "ShellCommands", PREF_STD_STRING, "spell:Alt+B:s:EX:\n\
    cat>spellTmp; xterm -e ispell -x spellTmp; cat spellTmp; rm spellTmp\n\
    wc::w:ED:\nwc | awk '{print $2 \" lines, \" $1 \" words, \" $3 \" characters\"}'\n\
    sort::o:EX:\nsort\nnumber lines::n:AW:\npr -tn\nmake:Alt+Z:m:W:\nmake\n\
    expand::p:EX:\nexpand\nunexpand::u:EX:\nunexpand\n",
     &TempStringPrefs.shellCmds, nullptr, true},
#else
    {"shellCommands", "ShellCommands", PREF_STD_STRING, "spell:Alt+B:s:ED:\n\
    (cat;echo \"\") | spell\nwc::w:ED:\nwc | awk '{print $1 \" lines, \" $2 \" words, \" $3 \" characters\"}'\n\
    \nsort::o:EX:\nsort\nnumber lines::n:AW:\nnl -ba\nmake:Alt+Z:m:W:\nmake\n\
    expand::p:EX:\nexpand\nunexpand::u:EX:\nunexpand\n",
     &TempStringPrefs.shellCmds, nullptr, true},
#endif // linux, __FreeBSD__ 

    {"macroCommands", "MacroCommands", PREF_STD_STRING, "Complete Word:Alt+D::: {\n\
		# This macro attempts to complete the current word by\n\
		# finding another word in the same document that has\n\
		# the same prefix; repeated invocations of the macro\n\
		# (by repeated typing of its accelerator, say) cycles\n\
		# through the alternatives found.\n\
		# \n\
		# Make sure $compWord contains something (a dummy index)\n\
		$compWord[\"\"] = \"\"\n\
		\n\
		# Test whether the rest of $compWord has been initialized:\n\
		# this avoids having to initialize the global variable\n\
		# $compWord in an external macro file\n\
		if (!(\"wordEnd\" in $compWord)) {\n\
		    # we need to initialize it\n\
		    $compWord[\"wordEnd\"] = 0\n\
		    $compWord[\"repeat\"] = 0\n\
		    $compWord[\"init\"] = 0\n\
		    $compWord[\"wordStart\"] = 0\n\
		}\n\
		\n\
		if ($compWord[\"wordEnd\"] == $cursor) {\n\
		        $compWord[\"repeat\"] += 1\n\
		}\n\
		else {\n\
		   $compWord[\"repeat\"] = 1\n\
		   $compWord[\"init\"] = $cursor\n\
		\n\
		   # search back to a word boundary to find the word to complete\n\
		   # (we use \\w here to allow for programming \"words\" that can include\n\
		   # digits and underscores; use \\l for letters only)\n\
		   $compWord[\"wordStart\"] = search(\"<\\\\w+\", $cursor, \"backward\", \"regex\", \"wrap\")\n\
		\n\
		   if ($compWord[\"wordStart\"] == -1)\n\
		      return\n\
		\n\
		    if ($search_end == $cursor)\n\
		       $compWord[\"word\"] = get_range($compWord[\"wordStart\"], $cursor)\n\
		    else\n\
		        return\n\
		}\n\
		s = $cursor\n\
		for (i=0; i <= $compWord[\"repeat\"]; i++)\n\
		    s = search($compWord[\"word\"], s - 1, \"backward\", \"regex\", \"wrap\")\n\
		\n\
		if (s == $compWord[\"wordStart\"]) {\n\
		   beep()\n\
		   $compWord[\"repeat\"] = 0\n\
		   s = $compWord[\"wordStart\"]\n\
		   se = $compWord[\"init\"]\n\
		}\n\
		else\n\
		   se = search(\">\", s, \"regex\")\n\
		\n\
		replace_range($compWord[\"wordStart\"], $cursor, get_range(s, se))\n\
		\n\
		$compWord[\"wordEnd\"] = $cursor\n\
	}\n\
	Fill Sel. w/Char:::R: {\n\
		# This macro replaces each character position in\n\
		# the selection with the string typed into the dialog\n\
		# it displays.\n\
		if ($selection_start == -1) {\n\
		    beep()\n\
		    return\n\
		}\n\
		\n\
		# Ask the user what character to fill with\n\
		fillChar = string_dialog(\"Fill selection with what character?\", \\\n\
		                         \"OK\", \"Cancel\")\n\
		if ($string_dialog_button == 2 || $string_dialog_button == 0)\n\
		    return\n\
		\n\
		# Count the number of lines (NL characters) in the selection\n\
		# (by removing all non-NLs in selection and counting the remainder)\n\
		nLines = length(replace_in_string(get_selection(), \\\n\
		                                  \"^.*$\", \"\", \"regex\"))\n\
		\n\
		rectangular = $selection_left != -1\n\
		\n\
		# work out the pieces of required of the replacement text\n\
		# this will be top mid bot where top is empty or ends in NL,\n\
		# mid is 0 or more lines of repeats ending with NL, and\n\
		# bot is 0 or more repeats of the fillChar\n\
		\n\
		toplen = -1 # top piece by default empty (no NL)\n\
		midlen = 0\n\
		botlen = 0\n\
		\n\
		if (rectangular) {\n\
		    # just fill the rectangle:  mid\\n \\ nLines\n\
		    #                           mid\\n /\n\
		    #                           bot   - last line with no nl\n\
		    midlen = $selection_right -  $selection_left\n\
		    botlen = $selection_right -  $selection_left\n\
		} else {\n\
		    #                  |col[0]\n\
		    #         .........toptoptop\\n                      |col[0]\n\
		    # either  midmidmidmidmidmid\\n \\ nLines - 1   or ...botbot...\n\
		    #         midmidmidmidmidmid\\n /                          |col[1]\n\
		    #         botbot...         |\n\
		    #                 |col[1]   |wrap margin\n\
		    # we need column positions col[0], col[1] of selection start and\n\
		    # end (use a loop and arrays to do the two positions)\n\
		    sel[0] = $selection_start\n\
		    sel[1] = $selection_end\n\
		\n\
		    # col[0] = pos_to_column($selection_start)\n\
		    # col[1] = pos_to_column($selection_end)\n\
		\n\
		    for (i = 0; i < 2; ++i) {\n\
		        end = sel[i]\n\
		        pos = search(\"^\", end, \"regex\", \"backward\")\n\
		        thisCol = 0\n\
		        while (pos < end) {\n\
		            nexttab = search(\"\\t\", pos)\n\
		            if (nexttab < 0 || nexttab >= end) {\n\
		                thisCol += end - pos # count remaining non-tabs\n\
		                nexttab = end\n\
		            } else {\n\
		                thisCol += nexttab - pos + $tab_dist\n\
		                thisCol -= (thisCol % $tab_dist)\n\
		            }\n\
		            pos = nexttab + 1 # skip past the tab or end\n\
		        }\n\
		        col[i] = thisCol\n\
		    }\n\
		    toplen = max($wrap_margin - col[0], 0)\n\
		    botlen = min(col[1], $wrap_margin)\n\
		\n\
		    if (nLines == 0) {\n\
		        toplen = -1\n\
		        botlen = max(botlen - col[0], 0)\n\
		    } else {\n\
		        midlen = $wrap_margin\n\
		        if (toplen < 0)\n\
		            toplen = 0\n\
		        nLines-- # top piece will end in a NL\n\
		    }\n\
		}\n\
		\n\
		# Create the fill text\n\
		# which is the longest piece? make a line of that length\n\
		# (use string doubling - this allows the piece to be\n\
		# appended to double in size at each iteration)\n\
		\n\
		len = max(toplen, midlen, botlen)\n\
		charlen = length(fillChar) # maybe more than one char given!\n\
		\n\
		line = \"\"\n\
		while (len > 0) {\n\
		    if (len % 2)\n\
		        line = line fillChar\n\
		    len /= 2\n\
		    if (len > 0)\n\
		        fillChar = fillChar fillChar\n\
		}\n\
		# assemble our pieces\n\
		toppiece = \"\"\n\
		midpiece = \"\"\n\
		botpiece = \"\"\n\
		if (toplen >= 0)\n\
		    toppiece = substring(line, 0, toplen * charlen) \"\\n\"\n\
		if (botlen > 0)\n\
		    botpiece = substring(line, 0, botlen * charlen)\n\
		\n\
		# assemble midpiece (use doubling again)\n\
		line = substring(line, 0, midlen * charlen) \"\\n\"\n\
		while (nLines > 0) {\n\
		    if (nLines % 2)\n\
		        midpiece = midpiece line\n\
		    nLines /= 2\n\
		    if (nLines > 0)\n\
		        line = line line\n\
		}\n\
		# Replace the selection with the complete fill text\n\
		replace_selection(toppiece midpiece botpiece)\n\
	}\n\
	Quote Mail Reply:::: {\n\
		if ($selection_start == -1)\n\
		    replace_all(\"^.*$\", \"\\\\> &\", \"regex\")\n\
		else\n\
		    replace_in_selection(\"^.*$\", \"\\\\> &\", \"regex\")\n\
	}\n\
	Unquote Mail Reply:::: {\n\
		if ($selection_start == -1)\n\
		    replace_all(\"(^\\\\> )(.*)$\", \"\\\\2\", \"regex\")\n\
		else\n\
		    replace_in_selection(\"(^\\\\> )(.*)$\", \"\\\\2\", \"regex\")\n\
	}\n\
	Comments>/* Comment */@C@C++@Java@CSS@JavaScript@Lex:::R: {\n\
		selStart = $selection_start\n\
		selEnd = $selection_end\n\
		replace_range(selStart, selEnd, \"/* \" get_selection() \" */\")\n\
		select(selStart, selEnd + 6)\n\
	}\n\
	Comments>/* Uncomment */@C@C++@Java@CSS@JavaScript@Lex:::R: {\n\
		pos = search(\"(?n\\\\s*/\\\\*\\\\s*)\", $selection_start, \"regex\")\n\
		start = $search_end\n\
		end = search(\"(?n\\\\*/\\\\s*)\", $selection_end, \"regex\", \"backward\")\n\
		if (pos != $selection_start || end == -1 )\n\
		    return\n\
		replace_selection(get_range(start, end))\n\
		select(pos, $cursor)\n\
	}\n\
	Comments>// Comment@C@C++@Java@JavaScript:::R: {\n\
		replace_in_selection(\"^.*$\", \"// &\", \"regex\")\n\
	}\n\
	Comments>// Uncomment@C@C++@Java@JavaScript:::R: {\n\
		replace_in_selection(\"(^[ \\\\t]*// ?)(.*)$\", \"\\\\2\", \"regex\")\n\
	}\n\
	Comments># Comment@Perl@Sh Ksh Bash@NEdit Macro@Makefile@Awk@Csh@Python@Tcl:::R: {\n\
		replace_in_selection(\"^.*$\", \"#&\", \"regex\")\n\
	}\n\
	Comments># Uncomment@Perl@Sh Ksh Bash@NEdit Macro@Makefile@Awk@Csh@Python@Tcl:::R: {\n\
		replace_in_selection(\"(^[ \\\\t]*#)(.*)$\", \"\\\\2\", \"regex\")\n\
	}\n\
	Comments>-- Comment@SQL:::R: {\n\
		replace_in_selection(\"^.*$\", \"--&\", \"regex\")\n\
	}\n\
	Comments>-- Uncomment@SQL:::R: {\n\
		replace_in_selection(\"(^[ \\\\t]*--)(.*)$\", \"\\\\2\", \"regex\")\n\
	}\n\
	Comments>! Comment@X Resources:::R: {\n\
		replace_in_selection(\"^.*$\", \"!&\", \"regex\")\n\
	}\n\
	Comments>! Uncomment@X Resources:::R: {\n\
		replace_in_selection(\"(^[ \\\\t]*!)(.*)$\", \"\\\\2\", \"regex\")\n\
	}\n\
	Comments>% Comment@LaTeX:::R: {\n\
		replace_in_selection(\"^.*$\", \"%&\", \"regex\")\n\
		}\n\
	Comments>% Uncomment@LaTeX:::R: {\n\
		replace_in_selection(\"(^[ \\\\t]*%)(.*)$\", \"\\\\2\", \"regex\")\n\
		}\n\
	Comments>Bar Comment@C:::R: {\n\
		if ($selection_left != -1) {\n\
		    dialog(\"Selection must not be rectangular\")\n\
		    return\n\
		}\n\
		start = $selection_start\n\
		end = $selection_end-1\n\
		origText = get_range($selection_start, $selection_end-1)\n\
		newText = \"/*\\n\" replace_in_string(get_range(start, end), \\\n\
		    \"^\", \" * \", \"regex\") \"\\n */\\n\"\n\
		replace_selection(newText)\n\
		select(start, start + length(newText))\n\
	}\n\
	Comments>Bar Uncomment@C:::R: {\n\
		selStart = $selection_start\n\
		selEnd = $selection_end\n\
		pos = search(\"/\\\\*\\\\s*\\\\n\", selStart, \"regex\")\n\
		if (pos != selStart) return\n\
		start = $search_end\n\
		end = search(\"\\\\n\\\\s*\\\\*/\\\\s*\\\\n?\", selEnd, \"regex\", \"backward\")\n\
		if (end == -1 || $search_end < selEnd) return\n\
		newText = get_range(start, end)\n\
		newText = replace_in_string(newText,\"^ *\\\\* ?\", \"\", \"regex\", \"copy\")\n\
		if (get_range(selEnd, selEnd - 1) == \"\\n\") selEnd -= 1\n\
		replace_range(selStart, selEnd, newText)\n\
		select(selStart, selStart + length(newText))\n\
	}\n\
	Make C Prototypes@C@C++:::: {\n\
		# simplistic extraction of C function prototypes, usually good enough\n\
		if ($selection_start == -1) {\n\
		    start = 0\n\
		    end = $text_length\n\
		} else {\n\
		    start = $selection_start\n\
		    end = $selection_end\n\
		}\n\
		string = get_range(start, end)\n\
		# remove all C++ and C comments, then all blank lines in the extracted range\n\
		string = replace_in_string(string, \"//.*$\", \"\", \"regex\", \"copy\")\n\
		string = replace_in_string(string, \"(?n/\\\\*.*?\\\\*/)\", \"\", \"regex\", \"copy\")\n\
		string = replace_in_string(string, \"^\\\\s*\\n\", \"\", \"regex\", \"copy\")\n\
		nDefs = 0\n\
		searchPos = 0\n\
		prototypes = \"\"\n\
		staticPrototypes = \"\"\n\
		for (;;) {\n\
		    headerStart = search_string(string, \\\n\
		        \"^[a-zA-Z]([^;#\\\"'{}=><!/]|\\n)*\\\\)[ \\t]*\\n?[ \\t]*\\\\{\", \\\n\
		        searchPos, \"regex\")\n\
		    if (headerStart == -1)\n\
		        break\n\
		    headerEnd = search_string(string, \")\", $search_end,\"backward\") + 1\n\
		    prototype = substring(string, headerStart, headerEnd) \";\\n\"\n\
		    if (substring(string, headerStart, headerStart+6) == \"static\")\n\
		        staticPrototypes = staticPrototypes prototype\n\
		    else\n\
		        prototypes = prototypes prototype\n\
		    searchPos = headerEnd\n\
		    nDefs++\n\
		}\n\
		if (nDefs == 0) {\n\
		    dialog(\"No function declarations found\")\n\
		    return\n\
		}\n\
		new()\n\
		focus_window(\"last\")\n\
		replace_range(0, 0, prototypes staticPrototypes)\n\
	}",
     &TempStringPrefs.macroCmds, nullptr, true},
    {"bgMenuCommands", "BGMenuCommands", PREF_STD_STRING, "Undo:::: {\nundo()\n}\n\
	Redo:::: {\nredo()\n}\n\
	Cut:::R: {\ncut_clipboard()\n}\n\
	Copy:::R: {\ncopy_clipboard()\n}\n\
	Paste:::: {\npaste_clipboard()\n}",
     &TempStringPrefs.bgMenuCmds, nullptr, true},

    {"highlightPatterns", "HighlightPatterns", PREF_STD_STRING, "Ada:Default\n\
        Awk:Default\n\
        C++:Default\n\
        C:Default\n\
        CSS:Default\n\
        Csh:Default\n\
        Fortran:Default\n\
        Java:Default\n\
        JavaScript:Default\n\
        LaTeX:Default\n\
        Lex:Default\n\
        Makefile:Default\n\
        Matlab:Default\n\
        NEdit Macro:Default\n\
        Pascal:Default\n\
        Perl:Default\n\
        PostScript:Default\n\
        Python:Default\n\
        Regex:Default\n\
        SGML HTML:Default\n\
        SQL:Default\n\
        Sh Ksh Bash:Default\n\
        Tcl:Default\n\
        VHDL:Default\n\
        Verilog:Default\n\
        XML:Default\n\
        X Resources:Default\n\
        Yacc:Default",
     &TempStringPrefs.highlight, nullptr, true},
    {"languageModes", "LanguageModes", PREF_STD_STRING,

     "Ada:.ada .ad .ads .adb .a:::::::\n\
        Awk:.awk:::::::\n\
        C++:.cc .hh .C .H .i .cxx .hxx .cpp .c++::::::\".,/\\`'!|@#%^&*()-=+{}[]\"\":;<>?~\":\n\
        C:.c .h::::::\".,/\\`'!|@#%^&*()-=+{}[]\"\":;<>?~\":\n\
        CSS:css::Auto:None:::\".,/\\`'!|@#%^&*()=+{}[]\"\":;<>?~\":\n\
        Csh:.csh .cshrc .tcshrc .login .logout:\"^[ \\t]*#[ \\t]*![ \\t]*/bin/t?csh\"::::::\n\
        Fortran:.f .f77 .for:::::::\n\
        Java:.java:::::::\n\
        JavaScript:.js:::::::\n\
        LaTeX:.tex .sty .cls .ltx .ins .clo .fd:::::::\n\
        Lex:.lex:::::::\n\
        Makefile:Makefile makefile .gmk:::None:8:8::\n\
        Matlab:.m .oct .sci:::::::\n\
        NEdit Macro:.nm .neditmacro:::::::\n\
        Pascal:.pas .p .int:::::::\n\
        Perl:.pl .pm .p5 .PL:\"^[ \\t]*#[ \\t]*!.*perl\":Auto:None:::\".,/\\\\`'!$@#%^&*()-=+{}[]\"\":;<>?~|\":\n\
        PostScript:.ps .eps .epsf .epsi:\"^%!\":::::\"/%(){}[]<>\":\n\
        Python:.py:\"^#!.*python\":Auto:None:::\"!\"\"#$%&'()*+,-./:;<=>?@[\\\\]^`{|}~\":\n\
        Regex:.reg .regex:\"\\(\\?[:#=!iInN].+\\)\":None:Continuous::::\n\
        SGML HTML:.sgml .sgm .html .htm:\"\\<[Hh][Tt][Mm][Ll]\\>\"::::::\n\
        SQL:.sql:::::::\n\
        Sh Ksh Bash:.sh .bash .ksh .profile .bashrc .bash_logout .bash_login .bash_profile:\"^[ \\t]*#[ \\t]*![ \\t]*/.*bin/(bash|ksh|sh|zsh)\"::::::\n\
        Tcl:.tcl .tk .itcl .itk::Smart:None::::\n\
        VHDL:.vhd .vhdl .vdl:::::::\n\
        Verilog:.v:::::::\n\
        XML:.xml .xsl .dtd:\"\\<(?i\\?xml|!doctype)\"::None:::\"<>/=\"\"'()+*?|\":\n\
        X Resources:.Xresources .Xdefaults .nedit .pats nedit.rc:\"^[!#].*([Aa]pp|[Xx]).*[Dd]efaults\"::::::\n\
        Yacc:.y::::::\".,/\\`'!|@#%^&*()-=+{}[]\"\":;<>?~\":",

     &TempStringPrefs.language, nullptr, true},
    {"styles", "Styles", PREF_STD_STRING, "Plain:black:Plain\n\
    	Comment:gray20:Italic\n\
    	Keyword:black:Bold\n\
        Operator:dark blue:Bold\n\
        Bracket:dark blue:Bold\n\
    	Storage Type:brown:Bold\n\
    	Storage Type1:saddle brown:Bold\n\
    	String:darkGreen:Plain\n\
    	String1:SeaGreen:Plain\n\
    	String2:darkGreen:Bold\n\
    	Preprocessor:RoyalBlue4:Plain\n\
    	Preprocessor1:blue:Plain\n\
    	Character Const:darkGreen:Plain\n\
    	Numeric Const:darkGreen:Plain\n\
    	Identifier:brown:Plain\n\
    	Identifier1:RoyalBlue4:Plain\n\
        Identifier2:SteelBlue:Plain\n\
 	Subroutine:brown:Plain\n\
	Subroutine1:chocolate:Plain\n\
   	Ada Attributes:plum:Bold\n\
	Label:red:Italic\n\
	Flag:red:Bold\n\
    	Text Comment:SteelBlue4:Italic\n\
    	Text Key:VioletRed4:Bold\n\
	Text Key1:VioletRed4:Plain\n\
    	Text Arg:RoyalBlue4:Bold\n\
    	Text Arg1:SteelBlue4:Bold\n\
	Text Arg2:RoyalBlue4:Plain\n\
    	Text Escape:gray30:Bold\n\
	LaTeX Math:darkGreen:Plain\n" ADD_5_2_STYLES,
     &TempStringPrefs.styles, nullptr, true},
    {"smartIndentInit", "SmartIndentInit", PREF_STD_STRING, "C:Default\n\
	C++:Default\n\
	Python:Default\n\
	Matlab:Default",
     &TempStringPrefs.smartIndent, nullptr, true},
    {"smartIndentInitCommon", "SmartIndentInitCommon", PREF_STD_STRING, "Default", &TempStringPrefs.smartIndentCommon, nullptr, true},
    {"autoWrap", "AutoWrap", PREF_ENUM, "Continuous", &PrefData.wrapStyle, AutoWrapTypes, true},
    {"wrapMargin", "WrapMargin", PREF_INT, "0", &PrefData.wrapMargin, nullptr, true},
    {"autoIndent", "AutoIndent", PREF_ENUM, "Auto", &PrefData.autoIndent, AutoIndentTypes, true},
    {"autoSave", "AutoSave", PREF_BOOLEAN, "True", &PrefData.autoSave, nullptr, true},
    {"openInTab", "OpenInTab", PREF_BOOLEAN, "True", &PrefData.openInTab, nullptr, true},
    {"saveOldVersion", "SaveOldVersion", PREF_BOOLEAN, "False", &PrefData.saveOldVersion, nullptr, true},
    {"showMatching", "ShowMatching", PREF_ENUM, "Delimiter", &PrefData.showMatchingStyle, ShowMatchingTypes, true},
    {"matchSyntaxBased", "MatchSyntaxBased", PREF_BOOLEAN, "True", &PrefData.matchSyntaxBased, nullptr, true},
    {"highlightSyntax", "HighlightSyntax", PREF_BOOLEAN, "True", &PrefData.highlightSyntax, nullptr, true},
    {"backlightChars", "BacklightChars", PREF_BOOLEAN, "False", &PrefData.backlightChars, nullptr, true},
    {"backlightCharTypes", "BacklightCharTypes", PREF_ALLOC_STRING, "0-8,10-31,127:red;9:#dedede;32,160-255:#f0f0f0;128-159:orange",
     //                     gray87                 gray94                 
     &PrefData.backlightCharTypes, nullptr, false},
    {"searchDialogs", "SearchDialogs", PREF_BOOLEAN, "False", &PrefData.searchDlogs, nullptr, true},
    {"beepOnSearchWrap", "BeepOnSearchWrap", PREF_BOOLEAN, "False", &PrefData.searchWrapBeep, nullptr, true},
    {"retainSearchDialogs", "RetainSearchDialogs", PREF_BOOLEAN, "False", &PrefData.keepSearchDlogs, nullptr, true},
    {"searchWraps", "SearchWraps", PREF_BOOLEAN, "True", &PrefData.searchWraps, nullptr, true},
    {"stickyCaseSenseButton", "StickyCaseSenseButton", PREF_BOOLEAN, "True", &PrefData.stickyCaseSenseBtn, nullptr, true},
    {"repositionDialogs", "RepositionDialogs", PREF_BOOLEAN, "True", &PrefData.repositionDialogs, nullptr, true},
    {"autoScroll", "AutoScroll", PREF_BOOLEAN, "False", &PrefData.autoScroll, nullptr, true},
    {"autoScrollVPadding", "AutoScrollVPadding", PREF_INT, "4", &PrefData.autoScrollVPadding, nullptr, false},
    {"appendLF", "AppendLF", PREF_BOOLEAN, "True", &PrefData.appendLF, nullptr, true},
    {"sortOpenPrevMenu", "SortOpenPrevMenu", PREF_BOOLEAN, "True", &PrefData.sortOpenPrevMenu, nullptr, true},
    {"statisticsLine", "StatisticsLine", PREF_BOOLEAN, "False", &PrefData.statsLine, nullptr, true},
    {"iSearchLine", "ISearchLine", PREF_BOOLEAN, "False", &PrefData.iSearchLine, nullptr, true},
    {"sortTabs", "SortTabs", PREF_BOOLEAN, "False", &PrefData.sortTabs, nullptr, true},
    {"tabBar", "TabBar", PREF_BOOLEAN, "True", &PrefData.tabBar, nullptr, true},
    {"tabBarHideOne", "TabBarHideOne", PREF_BOOLEAN, "True", &PrefData.tabBarHideOne, nullptr, true},
    {"toolTips", "ToolTips", PREF_BOOLEAN, "True", &PrefData.toolTips, nullptr, true},
    {"globalTabNavigate", "GlobalTabNavigate", PREF_BOOLEAN, "False", &PrefData.globalTabNavigate, nullptr, true},
    {"lineNumbers", "LineNumbers", PREF_BOOLEAN, "False", &PrefData.lineNums, nullptr, true},
    {"pathInWindowsMenu", "PathInWindowsMenu", PREF_BOOLEAN, "True", &PrefData.pathInWindowsMenu, nullptr, true},
    {"warnFileMods", "WarnFileMods", PREF_BOOLEAN, "True", &PrefData.warnFileMods, nullptr, true},
    {"warnRealFileMods", "WarnRealFileMods", PREF_BOOLEAN, "True", &PrefData.warnRealFileMods, nullptr, true},
    {"warnExit", "WarnExit", PREF_BOOLEAN, "True", &PrefData.warnExit, nullptr, true},
    {"searchMethod", "SearchMethod", PREF_ENUM, "Literal", &PrefData.searchMethod, SearchMethodStrings, true},
#ifdef REPLACE_SCOPE
    {"replaceDefaultScope", "ReplaceDefaultScope", PREF_ENUM, "Smart", &PrefData.replaceDefScope, ReplaceDefScopeStrings, true},
#endif
    {"textRows", "TextRows", PREF_INT, "24", &PrefData.textRows, nullptr, true},
    {"textCols", "TextCols", PREF_INT, "80", &PrefData.textCols, nullptr, true},
    {"tabDistance", "TabDistance", PREF_INT, "8", &PrefData.tabDist, nullptr, true},
    {"emulateTabs", "EmulateTabs", PREF_INT, "0", &PrefData.emTabDist, nullptr, true},
    {"insertTabs", "InsertTabs", PREF_BOOLEAN, "True", &PrefData.insertTabs, nullptr, true},
    {"textFont", "TextFont", PREF_STRING, "-*-courier-medium-r-normal--*-120-*-*-*-iso8859-1", PrefData.fontString, sizeof(PrefData.fontString), true},
    {"boldHighlightFont", "BoldHighlightFont", PREF_STRING, "-*-courier-bold-r-normal--*-120-*-*-*-iso8859-1", PrefData.boldFontString, sizeof(PrefData.boldFontString), true},
    {"italicHighlightFont", "ItalicHighlightFont", PREF_STRING, "-*-courier-medium-o-normal--*-120-*-*-*-iso8859-1", PrefData.italicFontString, sizeof(PrefData.italicFontString), true},
    {"boldItalicHighlightFont", "BoldItalicHighlightFont", PREF_STRING, "-*-courier-bold-o-normal--*-120-*-*-*-iso8859-1", PrefData.boldItalicFontString, sizeof(PrefData.boldItalicFontString), true},
    {"helpFont", "HelpFont", PREF_STRING, "-*-helvetica-medium-r-normal--*-120-*-*-*-iso8859-1", PrefData.helpFontNames[HELP_FONT], sizeof(PrefData.helpFontNames[HELP_FONT]), false},
    {"boldHelpFont", "BoldHelpFont", PREF_STRING, "-*-helvetica-bold-r-normal--*-120-*-*-*-iso8859-1", PrefData.helpFontNames[BOLD_HELP_FONT], sizeof(PrefData.helpFontNames[BOLD_HELP_FONT]), false},
    {"italicHelpFont", "ItalicHelpFont", PREF_STRING, "-*-helvetica-medium-o-normal--*-120-*-*-*-iso8859-1", PrefData.helpFontNames[ITALIC_HELP_FONT], sizeof(PrefData.helpFontNames[ITALIC_HELP_FONT]), false},
    {"boldItalicHelpFont", "BoldItalicHelpFont", PREF_STRING, "-*-helvetica-bold-o-normal--*-120-*-*-*-iso8859-1", PrefData.helpFontNames[BOLD_ITALIC_HELP_FONT], sizeof(PrefData.helpFontNames[BOLD_ITALIC_HELP_FONT]), false},
    {"fixedHelpFont", "FixedHelpFont", PREF_STRING, "-*-courier-medium-r-normal--*-120-*-*-*-iso8859-1", PrefData.helpFontNames[FIXED_HELP_FONT], sizeof(PrefData.helpFontNames[FIXED_HELP_FONT]), false},
    {"boldFixedHelpFont", "BoldFixedHelpFont", PREF_STRING, "-*-courier-bold-r-normal--*-120-*-*-*-iso8859-1", PrefData.helpFontNames[BOLD_FIXED_HELP_FONT], sizeof(PrefData.helpFontNames[BOLD_FIXED_HELP_FONT]), false},
    {"italicFixedHelpFont", "ItalicFixedHelpFont", PREF_STRING, "-*-courier-medium-o-normal--*-120-*-*-*-iso8859-1", PrefData.helpFontNames[ITALIC_FIXED_HELP_FONT], sizeof(PrefData.helpFontNames[ITALIC_FIXED_HELP_FONT]), false},
    {"boldItalicFixedHelpFont", "BoldItalicFixedHelpFont", PREF_STRING, "-*-courier-bold-o-normal--*-120-*-*-*-iso8859-1", PrefData.helpFontNames[BOLD_ITALIC_FIXED_HELP_FONT], sizeof(PrefData.helpFontNames[BOLD_ITALIC_FIXED_HELP_FONT]), false},
    {"helpLinkFont", "HelpLinkFont", PREF_STRING, "-*-helvetica-medium-r-normal--*-120-*-*-*-iso8859-1", PrefData.helpFontNames[HELP_LINK_FONT], sizeof(PrefData.helpFontNames[HELP_LINK_FONT]), false},
    {"h1HelpFont", "H1HelpFont", PREF_STRING, "-*-helvetica-bold-r-normal--*-140-*-*-*-iso8859-1", PrefData.helpFontNames[H1_HELP_FONT], sizeof(PrefData.helpFontNames[H1_HELP_FONT]), false},
    {"h2HelpFont", "H2HelpFont", PREF_STRING, "-*-helvetica-bold-o-normal--*-120-*-*-*-iso8859-1", PrefData.helpFontNames[H2_HELP_FONT], sizeof(PrefData.helpFontNames[H2_HELP_FONT]), false},
    {"h3HelpFont", "H3HelpFont", PREF_STRING, "-*-courier-bold-r-normal--*-120-*-*-*-iso8859-1", PrefData.helpFontNames[H3_HELP_FONT], sizeof(PrefData.helpFontNames[H3_HELP_FONT]), false},
    {"helpLinkColor", "HelpLinkColor", PREF_STRING, "#009900", PrefData.helpLinkColor, sizeof(PrefData.helpLinkColor), false},

    {"textFgColor", "TextFgColor", PREF_STRING, NEDIT_DEFAULT_FG, PrefData.colorNames[TEXT_FG_COLOR], sizeof(PrefData.colorNames[TEXT_FG_COLOR]), true},
    {"textBgColor", "TextBgColor", PREF_STRING, NEDIT_DEFAULT_TEXT_BG, PrefData.colorNames[TEXT_BG_COLOR], sizeof(PrefData.colorNames[TEXT_BG_COLOR]), true},
    {"selectFgColor", "SelectFgColor", PREF_STRING, NEDIT_DEFAULT_SEL_FG, PrefData.colorNames[SELECT_FG_COLOR], sizeof(PrefData.colorNames[SELECT_FG_COLOR]), true},
    {"selectBgColor", "SelectBgColor", PREF_STRING, NEDIT_DEFAULT_SEL_BG, PrefData.colorNames[SELECT_BG_COLOR], sizeof(PrefData.colorNames[SELECT_BG_COLOR]), true},
    {"hiliteFgColor", "HiliteFgColor", PREF_STRING, NEDIT_DEFAULT_HI_FG, PrefData.colorNames[HILITE_FG_COLOR], sizeof(PrefData.colorNames[HILITE_FG_COLOR]), true},
    {"hiliteBgColor", "HiliteBgColor", PREF_STRING, NEDIT_DEFAULT_HI_BG, PrefData.colorNames[HILITE_BG_COLOR], sizeof(PrefData.colorNames[HILITE_BG_COLOR]), true},
    {"lineNoFgColor", "LineNoFgColor", PREF_STRING, NEDIT_DEFAULT_LINENO_FG, PrefData.colorNames[LINENO_FG_COLOR], sizeof(PrefData.colorNames[LINENO_FG_COLOR]), true},
    {"cursorFgColor", "CursorFgColor", PREF_STRING, NEDIT_DEFAULT_CURSOR_FG, PrefData.colorNames[CURSOR_FG_COLOR], sizeof(PrefData.colorNames[CURSOR_FG_COLOR]), true},
    {"tooltipBgColor", "TooltipBgColor", PREF_STRING, "LemonChiffon1", PrefData.tooltipBgColor, sizeof(PrefData.tooltipBgColor), false},
    {"shell", "Shell", PREF_STRING, "DEFAULT", PrefData.shell, sizeof(PrefData.shell), true},
    {"geometry", "Geometry", PREF_STRING, "", PrefData.geometry, sizeof(PrefData.geometry), false},
    {"remapDeleteKey", "RemapDeleteKey", PREF_BOOLEAN, "False", &PrefData.mapDelete, nullptr, false},
    {"stdOpenDialog", "StdOpenDialog", PREF_BOOLEAN, "False", &PrefData.stdOpenDialog, nullptr, false},
    {"tagFile", "TagFile", PREF_STRING, "", PrefData.tagFile, sizeof(PrefData.tagFile), false},
    {"wordDelimiters", "WordDelimiters", PREF_STRING, ".,/\\`'!|@#%^&*()-=+{}[]\":;<>?", PrefData.delimiters, sizeof(PrefData.delimiters), false},
    {"serverName", "ServerName", PREF_STRING, "", PrefData.serverName, sizeof(PrefData.serverName), false},
    {"maxPrevOpenFiles", "MaxPrevOpenFiles", PREF_INT, "30", &PrefData.maxPrevOpenFiles, nullptr, false},
    {"bgMenuButton", "BGMenuButton", PREF_STRING, "~Shift~Ctrl~Meta~Alt<Btn3Down>", PrefData.bgMenuBtn, sizeof(PrefData.bgMenuBtn), false},
    {"smartTags", "SmartTags", PREF_BOOLEAN, "True", &PrefData.smartTags, nullptr, true},
    {"typingHidesPointer", "TypingHidesPointer", PREF_BOOLEAN, "False", &PrefData.typingHidesPointer, nullptr, false},
    {"alwaysCheckRelativeTagsSpecs", "AlwaysCheckRelativeTagsSpecs", PREF_BOOLEAN, "True", &PrefData.alwaysCheckRelativeTagsSpecs, nullptr, false},
    {"prefFileRead", "PrefFileRead", PREF_BOOLEAN, "False", &PrefData.prefFileRead, nullptr, true},
    {"findReplaceUsesSelection", "FindReplaceUsesSelection", PREF_BOOLEAN, "False", &PrefData.findReplaceUsesSelection, nullptr, false},
    {"overrideDefaultVirtualKeyBindings", "OverrideDefaultVirtualKeyBindings", PREF_ENUM, "Auto", &PrefData.virtKeyOverride, VirtKeyOverrideModes, false},
    {"titleFormat", "TitleFormat", PREF_STRING, "{%c} [%s] %f (%S) - %d", PrefData.titleFormat, sizeof(PrefData.titleFormat), true},
    {"undoModifiesSelection", "UndoModifiesSelection", PREF_BOOLEAN, "True", &PrefData.undoModifiesSelection, nullptr, false},
    {"focusOnRaise", "FocusOnRaise", PREF_BOOLEAN, "False", &PrefData.focusOnRaise, nullptr, false},
    {"forceOSConversion", "ForceOSConversion", PREF_BOOLEAN, "True", &PrefData.forceOSConversion, nullptr, false},
    {"truncSubstitution", "TruncSubstitution", PREF_ENUM, "Fail", &PrefData.truncSubstitution, TruncSubstitutionModes, false},
    {"honorSymlinks", "HonorSymlinks", PREF_BOOLEAN, "True", &PrefData.honorSymlinks, nullptr, false}};

static XrmOptionDescRec OpTable[] = {
    {(String) "-wrap", (String) ".autoWrap", XrmoptionNoArg, (XPointer) "Continuous"},
    {(String) "-nowrap", (String) ".autoWrap", XrmoptionNoArg, (XPointer) "None"},
    {(String) "-autowrap", (String) ".autoWrap", XrmoptionNoArg, (XPointer) "Newline"},
    {(String) "-noautowrap", (String) ".autoWrap", XrmoptionNoArg, (XPointer) "None"},
    {(String) "-autoindent", (String) ".autoIndent", XrmoptionNoArg, (XPointer) "Auto"},
    {(String) "-noautoindent", (String) ".autoIndent", XrmoptionNoArg, (XPointer) "False"},
    {(String) "-autosave", (String) ".autoSave", XrmoptionNoArg, (XPointer) "True"},
    {(String) "-noautosave", (String) ".autoSave", XrmoptionNoArg, (XPointer) "False"},
    {(String) "-rows", (String) ".textRows", XrmoptionSepArg, nullptr},
    {(String) "-columns", (String) ".textCols", XrmoptionSepArg, nullptr},
    {(String) "-tabs", (String) ".tabDistance", XrmoptionSepArg, nullptr},
    {(String) "-font", (String) ".textFont", XrmoptionSepArg, nullptr},
    {(String) "-fn", (String) ".textFont", XrmoptionSepArg, nullptr},
    {(String) "-svrname", (String) ".serverName", XrmoptionSepArg, nullptr},
};

static const char HeaderText[] = "\
! Preferences file for NEdit\n\
! (User settings in X \"application defaults\" format)\n\
!\n\
! This file is overwritten by the \"Save Defaults...\" command in NEdit\n\
! and serves only the interactively settable options presented in the NEdit\n\
! \"Preferences\" menu.  To modify other options, such as key bindings, use\n\
! the .Xdefaults file in your home directory (or the X resource\n\
! specification method appropriate to your system).  The contents of this\n\
! file can be moved into an X resource file, but since resources in this file\n\
! override their corresponding X resources, either this file should be \n\
! deleted or individual resource lines in the file should be deleted for the\n\
! moved lines to take effect.\n";

/* Module-global variable set when any preference changes (for asking the
   user about re-saving on exit) */
static bool PrefsHaveChanged = false;

/* Module-global variable set when user uses -import to load additional
   preferences on top of the defaults.  Contains name of file loaded */
static char *ImportedFile = nullptr;

//  Module-global variables for shell selection dialog  
static int DoneWithShellSelDialog = False;

static void translatePrefFormats(int convertOld, int fileVer);
static void setIntPref(int *prefDataField, int newValue);
static void setStringPref(char *prefDataField, const char *newValue);
static void shellSelOKCB(Widget widget, XtPointer clientData, XtPointer callData);
static void shellSelCancelCB(Widget widgget, XtPointer clientData, XtPointer callData);
static void reapplyLanguageMode(Document *window, int mode, int forceDefaults);

static bool stringReplaceEx(std::string *inString, const char *expr, const char *replaceWith, int searchType, int replaceLen);
static char *createExtString(char **extensions, int nExtensions);
static char **readExtensionList(const char **inPtr, int *nExtensions);
static const char *getDefaultShell(void);
static int caseFind(view::string_view inString, const char *expr);
static int caseReplaceEx(std::string *inString, const char *expr, const char *replaceWith, int replaceLen);
static int loadLanguageModesString(const char *inString, int fileVer);
static int loadLanguageModesStringEx(const std::string &string, int fileVer);
static int matchLanguageMode(Document *window);
static int modeError(LanguageMode *lm, const char *stringStart, const char *stoppedAt, const char *message);
static int regexFind(view::string_view inString, const char *expr);
static int regexReplaceEx(std::string *inString, const char *expr, const char *replaceWith);
static int replaceMacroIfUnchanged(const char *oldText, const char *newStart, const char *newEnd);
static std::string spliceStringEx(const std::string &intoString, view::string_view insertString, const char *atExpr);
static QString WriteLanguageModesStringEx(void);
static void migrateColorResources(XrmDatabase prefDB, XrmDatabase appDB);
static void setLangModeCB(Widget w, XtPointer clientData, XtPointer callData);
void updateLanguageModeSubmenu(Document *window);
static void updateMacroCmdsTo5dot5(void);
static void updateMacroCmdsTo5dot6(void);
static void updatePatternsTo5dot1(void);
static void updatePatternsTo5dot2(void);
static void updatePatternsTo5dot3(void);
static void updatePatternsTo5dot4(void);
static void updatePatternsTo5dot6(void);
static void updateShellCmdsTo5dot3(void);
static void updateShellCmdsTo5dot4(void);

XrmDatabase CreateNEditPrefDB(int *argcInOut, char **argvInOut) {

	// NOTE(eteran): mimic previous bahavior here of passing null when nedit.rc lookup fails
	try {
		const QString filename = GetRCFileNameEx(NEDIT_RC);
		return CreatePreferencesDatabase(filename.toLatin1().data(), APP_NAME, OpTable, XtNumber(OpTable), (unsigned int *)argcInOut, argvInOut);
	} catch(const path_error &e) {
		return CreatePreferencesDatabase(nullptr, APP_NAME, OpTable, XtNumber(OpTable), (unsigned int *)argcInOut, argvInOut);
	}
	
}

void RestoreNEditPrefs(XrmDatabase prefDB, XrmDatabase appDB) {
	int requiresConversion;
	int major;       // The integral part of version number 
	int minor;       // fractional part of version number 
	int fileVer = 0; // Both combined into an integer 
	int nparsed;

	// Load preferences 
	RestorePreferences(prefDB, appDB, APP_NAME, APP_CLASS, PrefDescrip, XtNumber(PrefDescrip));

	/* If the preferences file was written by an older version of NEdit,
	   warn the user that it will be converted. */
	requiresConversion = PrefData.prefFileRead && PrefData.fileVersion[0] == '\0';
	if (requiresConversion) {
		updatePatternsTo5dot1();
	}

	if (PrefData.prefFileRead) {
		if (PrefData.fileVersion[0] == '\0') {
			fileVer = 0; // Pre-5.1 
		} else {
			/* Note: do not change the format of this.  Older executables
			   need to read this field for forward compatability. */
			nparsed = sscanf(PrefData.fileVersion, "%d.%d", &major, &minor);
			if (nparsed >= 2) {
				// Use OSF-style numbering scheme 
				fileVer = major * 1000 + minor;
			}
		}
	}

	if (PrefData.prefFileRead && fileVer < 5002) {
		updatePatternsTo5dot2();
	}

	if (PrefData.prefFileRead && fileVer < 5003) {
		updateShellCmdsTo5dot3();
		updatePatternsTo5dot3();
	}

	/* Note that we don't care about unreleased file versions.  Anyone
	   who is running a CVS or alpha version of NEdit is resposnbile
	   for managing the preferences file themselves.  Otherwise, it
	   gets impossible to track the number of "in-between" file formats.
	   We only do auto-upgrading for a real release. */

	if (PrefData.prefFileRead && (fileVer < 5004)) {
		migrateColorResources(prefDB, appDB);
		updateShellCmdsTo5dot4();
		updatePatternsTo5dot4();
	}
	if (PrefData.prefFileRead && (fileVer < 5005)) {
		updateMacroCmdsTo5dot5();
	}
	if (PrefData.prefFileRead && (fileVer < 5006)) {
		fprintf(stderr, "NEdit: Converting .nedit file to 5.6 version.\n"
		                "    To keep, use Preferences -> Save Defaults\n");
		updateMacroCmdsTo5dot6();
		updatePatternsTo5dot6();
	}
	// Migrate colors if there's no config file yet 
	if (!PrefData.prefFileRead) {
		migrateColorResources(prefDB, appDB);
	}

	/* Do further parsing on resource types which RestorePreferences does
	   not understand and reads as strings, to put them in the final form
	   in which nedit stores and uses.  If the preferences file was
	   written by an older version of NEdit, update regular expressions in
	   highlight patterns to quote braces and use & instead of \0 */
	translatePrefFormats(requiresConversion, fileVer);
}

/*
** Many of of NEdit's preferences are much more complicated than just simple
** integers or strings.  These are read as strings, but must be parsed and
** translated into something meaningful.  This routine does the translation,
** and, in most cases, frees the original string, which is no longer useful.
**
** In addition this function covers settings that, while simple, require
** additional steps before they can be published.
**
** The argument convertOld attempts a conversion from pre 5.1 format .nedit
** files (which means patterns and macros may contain regular expressions
** which are of the older syntax where braces were not quoted, and \0 was a
** legal substitution character).  Macros, so far can not be automatically
** converted, unfortunately.
*/
static void translatePrefFormats(int convertOld, int fileVer) {
	XFontStruct *font;

	/* Parse the strings which represent types which are not decoded by
	   the standard resource manager routines */

	if (!TempStringPrefs.shellCmds.isNull()) {
		LoadShellCmdsStringEx(TempStringPrefs.shellCmds.toStdString());
		TempStringPrefs.shellCmds = QString();
	}
	if (!TempStringPrefs.macroCmds.isNull()) {
		LoadMacroCmdsStringEx(TempStringPrefs.macroCmds.toStdString());
		TempStringPrefs.macroCmds = QString();
	}
	if (!TempStringPrefs.bgMenuCmds.isNull()) {
		LoadBGMenuCmdsStringEx(TempStringPrefs.bgMenuCmds.toStdString());
		TempStringPrefs.bgMenuCmds = QString();
	}
	if (!TempStringPrefs.highlight.isNull()) {
		LoadHighlightStringEx(TempStringPrefs.highlight.toStdString(), convertOld);
		TempStringPrefs.highlight = QString();
	}
	if (!TempStringPrefs.styles.isNull()) {
		LoadStylesStringEx(TempStringPrefs.styles.toStdString());
		TempStringPrefs.styles = QString();
	}
	if (!TempStringPrefs.language.isNull()) {
		loadLanguageModesStringEx(TempStringPrefs.language.toStdString(), fileVer);
		TempStringPrefs.language = QString();
	}
	if (!TempStringPrefs.smartIndent.isNull()) {
		LoadSmartIndentStringEx(TempStringPrefs.smartIndent);
		TempStringPrefs.smartIndent = QString();
	}
	if (!TempStringPrefs.smartIndentCommon.isNull()) {
		LoadSmartIndentCommonStringEx(TempStringPrefs.smartIndentCommon.toStdString());
		TempStringPrefs.smartIndentCommon = QString();
	}

	// translate the font names into fontLists suitable for the text widget 
	font = XLoadQueryFont(TheDisplay, PrefData.fontString);
	PrefData.fontList = font == nullptr ? nullptr : XmFontListCreate(font, XmSTRING_DEFAULT_CHARSET);
	PrefData.boldFontStruct = XLoadQueryFont(TheDisplay, PrefData.boldFontString);
	PrefData.italicFontStruct = XLoadQueryFont(TheDisplay, PrefData.italicFontString);
	PrefData.boldItalicFontStruct = XLoadQueryFont(TheDisplay, PrefData.boldItalicFontString);

	/*
	**  The default set for the comand shell in PrefDescrip ("DEFAULT") is
	**  only a place-holder, the actual default is the user's login shell
	**  (or whatever is implemented in getDefaultShell()). We put the login
	**  shell's name in PrefData here.
	*/
	if (strcmp(PrefData.shell, "DEFAULT") == 0) {
		strncpy(PrefData.shell, getDefaultShell(), MAXPATHLEN);
		PrefData.shell[MAXPATHLEN] = '\0';
	}

	/* For compatability with older (4.0.3 and before) versions, the autoWrap
	   and autoIndent resources can accept values of True and False.  Translate
	   them into acceptable wrap and indent styles */
	if (PrefData.wrapStyle == 3)
		PrefData.wrapStyle = CONTINUOUS_WRAP;
	if (PrefData.wrapStyle == 4)
		PrefData.wrapStyle = NO_WRAP;
	if (PrefData.autoIndent == 3)
		PrefData.autoIndent = AUTO_INDENT;
	if (PrefData.autoIndent == 4)
		PrefData.autoIndent = NO_AUTO_INDENT;

	/* setup language mode dependent info of user menus (to increase
	   performance when switching between documents of different
	   language modes) */
	SetupUserMenuInfo();
}

void SaveNEditPrefs(Widget parent, int quietly) {
	try {
		QString prefFileName = GetRCFileNameEx(NEDIT_RC);

		if (!quietly) {
		
		
			int resp = QMessageBox::information(nullptr /*parent*/, QLatin1String("Save Preferences"), 
				ImportedFile == nullptr ? QString(QLatin1String("Default preferences will be saved in the file:\n%1\nNEdit automatically loads this file\neach time it is started.")).arg(prefFileName)
				                        : QString(QLatin1String("Default preferences will be saved in the file:\n%1\nSAVING WILL INCORPORATE SETTINGS\nFROM FILE: %2")).arg(prefFileName).arg(QLatin1String(ImportedFile)), 
					QMessageBox::Ok | QMessageBox::Cancel);
		
		
		
			if(resp == QMessageBox::Cancel) {
				return;
			}
		}

		/*  Write the more dynamic settings into TempStringPrefs.
	    	These locations are set in PrefDescrip, so this is where
	    	SavePreferences() will look for them.  */

		TempStringPrefs.shellCmds         = WriteShellCmdsStringEx();
		TempStringPrefs.macroCmds         = WriteMacroCmdsStringEx();
		TempStringPrefs.bgMenuCmds        = WriteBGMenuCmdsStringEx();
		TempStringPrefs.highlight         = WriteHighlightStringEx();
		TempStringPrefs.language          = WriteLanguageModesStringEx();
		TempStringPrefs.styles            = WriteStylesStringEx();
		TempStringPrefs.smartIndent       = WriteSmartIndentStringEx();
		TempStringPrefs.smartIndentCommon = WriteSmartIndentCommonStringEx();
		strcpy(PrefData.fileVersion, PREF_FILE_VERSION);

		if (!SavePreferences(XtDisplay(parent), prefFileName.toLatin1().data(), HeaderText, PrefDescrip, XtNumber(PrefDescrip))) {
			QMessageBox::warning(nullptr /*parent*/, QLatin1String("Save Preferences"), QString(QLatin1String("Unable to save preferences in %1")).arg(prefFileName));
		}

		PrefsHaveChanged = false;
	} catch(const path_error &e) {
		QMessageBox::warning(nullptr /*parent*/, QLatin1String("Error saving Preferences"), QLatin1String("Unable to save preferences: Cannot determine filename."));
	}
}

/*
** Load an additional preferences file on top of the existing preferences
** derived from defaults, the .nedit file, and X resources.
*/
void ImportPrefFile(const char *filename, int convertOld) {
	XrmDatabase db;

	QString fileString = ReadAnyTextFileEx(filename, False);
	if (!fileString.isNull()) {
		db = XrmGetStringDatabase(fileString.toLatin1().data());
		OverlayPreferences(db, APP_NAME, APP_CLASS, PrefDescrip, XtNumber(PrefDescrip));
		translatePrefFormats(convertOld, -1);
		ImportedFile = XtNewStringEx(filename);
	} else {
		fprintf(stderr, "Could not read additional preferences file: %s\n", filename);
	}
}

void SetPrefOpenInTab(int state) {
	setIntPref(&PrefData.openInTab, state);
	
	for(Document *w: WindowList) {
		w->UpdateNewOppositeMenu(state);
	}
}

int GetPrefOpenInTab(void) {
	return PrefData.openInTab;
}

void SetPrefWrap(int state) {
	setIntPref(&PrefData.wrapStyle, state);
}

int GetPrefWrap(int langMode) {
	if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode]->wrapStyle == DEFAULT_WRAP)
		return PrefData.wrapStyle;
	return LanguageModes[langMode]->wrapStyle;
}

void SetPrefWrapMargin(int margin) {
	setIntPref(&PrefData.wrapMargin, margin);
}

int GetPrefWrapMargin(void) {
	return PrefData.wrapMargin;
}

void SetPrefSearch(int searchType) {
	setIntPref(&PrefData.searchMethod, searchType);
}

int GetPrefSearch(void) {
	return PrefData.searchMethod;
}

#ifdef REPLACE_SCOPE
void SetPrefReplaceDefScope(int scope) {
	setIntPref(&PrefData.replaceDefScope, scope);
}

int GetPrefReplaceDefScope(void) {
	return PrefData.replaceDefScope;
}
#endif

void SetPrefAutoIndent(int state) {
	setIntPref(&PrefData.autoIndent, state);
}

int GetPrefAutoIndent(int langMode) {
	if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode]->indentStyle == DEFAULT_INDENT)
		return PrefData.autoIndent;
	return LanguageModes[langMode]->indentStyle;
}

void SetPrefAutoSave(int state) {
	setIntPref(&PrefData.autoSave, state);
}

int GetPrefAutoSave(void) {
	return PrefData.autoSave;
}

void SetPrefSaveOldVersion(int state) {
	setIntPref(&PrefData.saveOldVersion, state);
}

int GetPrefSaveOldVersion(void) {
	return PrefData.saveOldVersion;
}

void SetPrefSearchDlogs(int state) {
	setIntPref(&PrefData.searchDlogs, state);
}

int GetPrefSearchDlogs(void) {
	return PrefData.searchDlogs;
}

void SetPrefBeepOnSearchWrap(int state) {
	setIntPref(&PrefData.searchWrapBeep, state);
}

int GetPrefBeepOnSearchWrap(void) {
	return PrefData.searchWrapBeep;
}

void SetPrefKeepSearchDlogs(int state) {
	setIntPref(&PrefData.keepSearchDlogs, state);
}

int GetPrefKeepSearchDlogs(void) {
	return PrefData.keepSearchDlogs;
}

void SetPrefSearchWraps(int state) {
	setIntPref(&PrefData.searchWraps, state);
}

int GetPrefStickyCaseSenseBtn(void) {
	return PrefData.stickyCaseSenseBtn;
}

int GetPrefSearchWraps(void) {
	return PrefData.searchWraps;
}

void SetPrefStatsLine(int state) {
	setIntPref(&PrefData.statsLine, state);
}

int GetPrefStatsLine(void) {
	return PrefData.statsLine;
}

void SetPrefISearchLine(int state) {
	setIntPref(&PrefData.iSearchLine, state);
}

int GetPrefISearchLine(void) {
	return PrefData.iSearchLine;
}

void SetPrefSortTabs(int state) {
	setIntPref(&PrefData.sortTabs, state);
}

int GetPrefSortTabs(void) {
	return PrefData.sortTabs;
}

void SetPrefTabBar(int state) {
	setIntPref(&PrefData.tabBar, state);
}

int GetPrefTabBar(void) {
	return PrefData.tabBar;
}

void SetPrefTabBarHideOne(int state) {
	setIntPref(&PrefData.tabBarHideOne, state);
}

int GetPrefTabBarHideOne(void) {
	return PrefData.tabBarHideOne;
}

void SetPrefGlobalTabNavigate(int state) {
	setIntPref(&PrefData.globalTabNavigate, state);
}

int GetPrefGlobalTabNavigate(void) {
	return PrefData.globalTabNavigate;
}

void SetPrefToolTips(int state) {
	setIntPref(&PrefData.toolTips, state);
}

int GetPrefToolTips(void) {
	return PrefData.toolTips;
}

void SetPrefLineNums(int state) {
	setIntPref(&PrefData.lineNums, state);
}

int GetPrefLineNums(void) {
	return PrefData.lineNums;
}

void SetPrefShowPathInWindowsMenu(int state) {
	setIntPref(&PrefData.pathInWindowsMenu, state);
}

int GetPrefShowPathInWindowsMenu(void) {
	return PrefData.pathInWindowsMenu;
}

void SetPrefWarnFileMods(int state) {
	setIntPref(&PrefData.warnFileMods, state);
}

int GetPrefWarnFileMods(void) {
	return PrefData.warnFileMods;
}

void SetPrefWarnRealFileMods(int state) {
	setIntPref(&PrefData.warnRealFileMods, state);
}

int GetPrefWarnRealFileMods(void) {
	return PrefData.warnRealFileMods;
}

void SetPrefWarnExit(int state) {
	setIntPref(&PrefData.warnExit, state);
}

int GetPrefWarnExit(void) {
	return PrefData.warnExit;
}

void SetPrefv(int state) {
	setIntPref(&PrefData.findReplaceUsesSelection, state);
}

int GetPrefFindReplaceUsesSelection(void) {
	return PrefData.findReplaceUsesSelection;
}

int GetPrefMapDelete(void) {
	return PrefData.mapDelete;
}

int GetPrefStdOpenDialog(void) {
	return PrefData.stdOpenDialog;
}

void SetPrefRows(int nRows) {
	setIntPref(&PrefData.textRows, nRows);
}

int GetPrefRows(void) {
	return PrefData.textRows;
}

void SetPrefCols(int nCols) {
	setIntPref(&PrefData.textCols, nCols);
}

int GetPrefCols(void) {
	return PrefData.textCols;
}

void SetPrefTabDist(int tabDist) {
	setIntPref(&PrefData.tabDist, tabDist);
}

int GetPrefTabDist(int langMode) {
	int tabDist;
	if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode]->tabDist == DEFAULT_TAB_DIST) {
		tabDist = PrefData.tabDist;
	} else {
		tabDist = LanguageModes[langMode]->tabDist;
	}
	/* Make sure that the tab distance is in range (garbage may have
	   been entered via the command line or the X resources, causing
	   errors later on, like division by zero). */
	if (tabDist <= 0)
		return 1;
	if (tabDist > MAX_EXP_CHAR_LEN)
		return MAX_EXP_CHAR_LEN;
	return tabDist;
}

void SetPrefEmTabDist(int tabDist) {
	setIntPref(&PrefData.emTabDist, tabDist);
}

int GetPrefEmTabDist(int langMode) {
	if (langMode == PLAIN_LANGUAGE_MODE || LanguageModes[langMode]->emTabDist == DEFAULT_EM_TAB_DIST)
		return PrefData.emTabDist;
	return LanguageModes[langMode]->emTabDist;
}

void SetPrefInsertTabs(int state) {
	setIntPref(&PrefData.insertTabs, state);
}

int GetPrefInsertTabs(void) {
	return PrefData.insertTabs;
}

void SetPrefShowMatching(int state) {
	setIntPref(&PrefData.showMatchingStyle, state);
}

int GetPrefShowMatching(void) {
	/*
	 * For backwards compatibility with pre-5.2 versions, the boolean
	 * False/True matching behavior is converted to NO_FLASH/FLASH_DELIMIT.
	 */
	if (PrefData.showMatchingStyle >= N_SHOW_MATCHING_STYLES)
		PrefData.showMatchingStyle -= N_SHOW_MATCHING_STYLES;
	return PrefData.showMatchingStyle;
}

void SetPrefMatchSyntaxBased(int state) {
	setIntPref(&PrefData.matchSyntaxBased, state);
}

int GetPrefMatchSyntaxBased(void) {
	return PrefData.matchSyntaxBased;
}

void SetPrefHighlightSyntax(Boolean state) {
	setIntPref(&PrefData.highlightSyntax, state);
}

Boolean GetPrefHighlightSyntax(void) {
	return PrefData.highlightSyntax;
}

void SetPrefBacklightChars(int state) {
	setIntPref(&PrefData.backlightChars, state);
}

int GetPrefBacklightChars(void) {
	return PrefData.backlightChars;
}

char *GetPrefBacklightCharTypes(void) {
	return PrefData.backlightCharTypes;
}

void SetPrefRepositionDialogs(int state) {
	setIntPref(&PrefData.repositionDialogs, state);
}

int GetPrefRepositionDialogs(void) {
	return PrefData.repositionDialogs;
}

void SetPrefAutoScroll(int state) {
	int margin = state ? PrefData.autoScrollVPadding : 0;

	setIntPref(&PrefData.autoScroll, state);
	
	for(Document *w: WindowList) {
		w->SetAutoScroll(margin);
	}
}

int GetPrefAutoScroll(void) {
	return PrefData.autoScroll;
}

int GetVerticalAutoScroll(void) {
	return PrefData.autoScroll ? PrefData.autoScrollVPadding : 0;
}

void SetPrefAppendLF(int state) {
	setIntPref(&PrefData.appendLF, state);
}

int GetPrefAppendLF(void) {
	return PrefData.appendLF;
}

void SetPrefSortOpenPrevMenu(int state) {
	setIntPref(&PrefData.sortOpenPrevMenu, state);
}

int GetPrefSortOpenPrevMenu(void) {
	return PrefData.sortOpenPrevMenu;
}

char *GetPrefTagFile(void) {
	return PrefData.tagFile;
}

void SetPrefSmartTags(int state) {
	setIntPref(&PrefData.smartTags, state);
}

int GetPrefSmartTags(void) {
	return PrefData.smartTags;
}

int GetPrefAlwaysCheckRelTagsSpecs(void) {
	return PrefData.alwaysCheckRelativeTagsSpecs;
}

char *GetPrefDelimiters(void) {
	return PrefData.delimiters;
}

char *GetPrefColorName(int index) {
	return PrefData.colorNames[index];
}

void SetPrefColorName(int index, const char *name) {
	setStringPref(PrefData.colorNames[index], name);
}

/*
** Set the font preferences using the font name (the fontList is generated
** in this call).  Note that this leaks memory and server resources each
** time the default font is re-set.  See note on SetFontByName in window.c
** for more information.
*/
void SetPrefFont(char *fontName) {
	XFontStruct *font;

	setStringPref(PrefData.fontString, fontName);
	font = XLoadQueryFont(TheDisplay, fontName);
	PrefData.fontList = font == nullptr ? nullptr : XmFontListCreate(font, XmSTRING_DEFAULT_CHARSET);
}

void SetPrefBoldFont(char *fontName) {
	setStringPref(PrefData.boldFontString, fontName);
	PrefData.boldFontStruct = XLoadQueryFont(TheDisplay, fontName);
}

void SetPrefItalicFont(char *fontName) {
	setStringPref(PrefData.italicFontString, fontName);
	PrefData.italicFontStruct = XLoadQueryFont(TheDisplay, fontName);
}
void SetPrefBoldItalicFont(char *fontName) {
	setStringPref(PrefData.boldItalicFontString, fontName);
	PrefData.boldItalicFontStruct = XLoadQueryFont(TheDisplay, fontName);
}

char *GetPrefFontName(void) {
	return PrefData.fontString;
}

char *GetPrefBoldFontName(void) {
	return PrefData.boldFontString;
}

char *GetPrefItalicFontName(void) {
	return PrefData.italicFontString;
}

char *GetPrefBoldItalicFontName(void) {
	return PrefData.boldItalicFontString;
}

XmFontList GetPrefFontList(void) {
	return PrefData.fontList;
}

XFontStruct *GetPrefBoldFont(void) {
	return PrefData.boldFontStruct;
}

XFontStruct *GetPrefItalicFont(void) {
	return PrefData.italicFontStruct;
}

XFontStruct *GetPrefBoldItalicFont(void) {
	return PrefData.boldItalicFontStruct;
}

char *GetPrefHelpFontName(int index) {
	return PrefData.helpFontNames[index];
}

char *GetPrefHelpLinkColor(void) {
	return PrefData.helpLinkColor;
}

char *GetPrefTooltipBgColor(void) {
	return PrefData.tooltipBgColor;
}

void SetPrefShell(const char *shell) {
	setStringPref(PrefData.shell, shell);
}

const char *GetPrefShell(void) {
	return PrefData.shell;
}

char *GetPrefGeometry(void) {
	return PrefData.geometry;
}

char *GetPrefServerName(void) {
	return PrefData.serverName;
}

char *GetPrefBGMenuBtn(void) {
	return PrefData.bgMenuBtn;
}

int GetPrefMaxPrevOpenFiles(void) {
	return PrefData.maxPrevOpenFiles;
}

int GetPrefTypingHidesPointer(void) {
	return (PrefData.typingHidesPointer);
}

void SetPrefTitleFormat(const char *format) {
	setStringPref(PrefData.titleFormat, format);

	// update all windows 
	for(Document *window: WindowList) {
		window->UpdateWindowTitle();
	}
}
const char *GetPrefTitleFormat(void) {
	return PrefData.titleFormat;
}

Boolean GetPrefUndoModifiesSelection(void) {
	return (Boolean)PrefData.undoModifiesSelection;
}

Boolean GetPrefFocusOnRaise(void) {
	return (Boolean)PrefData.focusOnRaise;
}

Boolean GetPrefForceOSConversion(void) {
	return PrefData.forceOSConversion;
}

Boolean GetPrefHonorSymlinks(void) {
	return PrefData.honorSymlinks;
}

int GetPrefOverrideVirtKeyBindings(void) {
	return PrefData.virtKeyOverride;
}

int GetPrefTruncSubstitution(void) {
	return PrefData.truncSubstitution;
}

/*
** If preferences don't get saved, ask the user on exit whether to save
*/
void MarkPrefsChanged(void) {
	PrefsHaveChanged = true;
}

/*
** Check if preferences have changed, and if so, ask the user if he wants
** to re-save.  Returns False if user requests cancelation of Exit (or whatever
** operation triggered this call to be made).
*/
bool CheckPrefsChangesSaved(Widget dialogParent) {

	if (!PrefsHaveChanged)
		return True;


	QMessageBox messageBox(nullptr /*dialogParent*/);
	messageBox.setWindowTitle(QLatin1String("Default Preferences"));
	messageBox.setIcon(QMessageBox::Question);
	
	
	messageBox.setText((ImportedFile == nullptr)
		? QString(QLatin1String("Default Preferences have changed.\nSave changes to NEdit preference file?"))
		: QString(QLatin1String("Default Preferences have changed.  SAVING \nCHANGES WILL INCORPORATE ADDITIONAL\nSETTINGS FROM FILE: %s")).arg(QLatin1String(ImportedFile)));
	
	
	QPushButton *buttonSave     = messageBox.addButton(QLatin1String("Save"), QMessageBox::AcceptRole);
	QPushButton *buttonDontSave = messageBox.addButton(QLatin1String("Don't Save"), QMessageBox::AcceptRole);
	QPushButton *buttonCancel   = messageBox.addButton(QMessageBox::Cancel);
	Q_UNUSED(buttonCancel);

	messageBox.exec();
	if(messageBox.clickedButton() == buttonSave) {
		SaveNEditPrefs(dialogParent, True);
		return true;
	} else if(messageBox.clickedButton() == buttonDontSave) {
		return true;
	} else {
		return false;
	}

}

/*
** set *prefDataField to newValue, but first check if they're different
** and update PrefsHaveChanged if a preference setting has now changed.
*/
static void setIntPref(int *prefDataField, int newValue) {
	if (newValue != *prefDataField)
		PrefsHaveChanged = true;
	*prefDataField = newValue;
}

static void setStringPref(char *prefDataField, const char *newValue) {
	if (strcmp(prefDataField, newValue))
		PrefsHaveChanged = true;
	strcpy(prefDataField, newValue);
}

/*
** Set the language mode for the window, update the menu and trigger language
** mode specific actions (turn on/off highlighting).  If forceNewDefaults is
** true, re-establish default settings for language-specific preferences
** regardless of whether they were previously set by the user.
*/
void SetLanguageMode(Document *window, int mode, int forceNewDefaults) {
	Widget menu;
	WidgetList items;
	int n;
	Cardinal nItems;
	void *userData;

	// Do mode-specific actions 
	reapplyLanguageMode(window, mode, forceNewDefaults);

	// Select the correct language mode in the sub-menu 
	if (window->IsTopDocument()) {
		XtVaGetValues(window->langModeCascade_, XmNsubMenuId, &menu, nullptr);
		XtVaGetValues(menu, XmNchildren, &items, XmNnumChildren, &nItems, nullptr);
		for (n = 0; n < (int)nItems; n++) {
			XtVaGetValues(items[n], XmNuserData, &userData, nullptr);
			XmToggleButtonSetState(items[n], (long)userData == mode, False);
		}
	}
}

/*
** Lookup a language mode by name, returning the index of the language
** mode or PLAIN_LANGUAGE_MODE if the name is not found
*/
int FindLanguageMode(const char *languageName) {
	int i;

	// Compare each language mode to the one we were presented 
	for (i = 0; i < NLanguageModes; i++)
		if (!strcmp(languageName, LanguageModes[i]->name))
			return i;

	return PLAIN_LANGUAGE_MODE;
}

/*
** Apply language mode matching criteria and set window->languageMode_ to
** the appropriate mode for the current file, trigger language mode
** specific actions (turn on/off highlighting), and update the language
** mode menu item.  If forceNewDefaults is true, re-establish default
** settings for language-specific preferences regardless of whether
** they were previously set by the user.
*/
void DetermineLanguageMode(Document *window, int forceNewDefaults) {
	SetLanguageMode(window, matchLanguageMode(window), forceNewDefaults);
}

/*
** Return the name of the current language mode set in "window", or nullptr
** if the current mode is "Plain".
*/
char *LanguageModeName(int mode) {
	if (mode == PLAIN_LANGUAGE_MODE)
		return nullptr;
	else
		return LanguageModes[mode]->name;
}

/*
** Get the set of word delimiters for the language mode set in the current
** window.  Returns nullptr when no language mode is set (it would be easy to
** return the default delimiter set when the current language mode is "Plain",
** or the mode doesn't have its own delimiters, but this is usually used
** to supply delimiters for RE searching, and ExecRE can skip compiling a
** delimiter table when delimiters is nullptr).
*/
char *GetWindowDelimiters(const Document *window) {
	if (window->languageMode_ == PLAIN_LANGUAGE_MODE)
		return nullptr;
	else
		return LanguageModes[window->languageMode_]->delimiters;
}

/*
** Present the user a dialog for setting wrap margin.
*/
void WrapMarginDialog(Widget parent, Document *forWindow) {

	Q_UNUSED(parent);

	auto dialog = new DialogWrapMargin(forWindow, nullptr /*parent*/);
	
	int margin;
	// Set default value 
	if(!forWindow) {
		margin = GetPrefWrapMargin();
	} else {
		XtVaGetValues(forWindow->textArea_, textNwrapMargin, &margin, nullptr);
	}
	
	dialog->ui.checkWrapAndFill->setChecked(margin == 0);
	dialog->ui.spinWrapAndFill->setValue(margin);
	
	dialog->exec();
	delete dialog;
}

/*
**  Create and show a dialog for selecting the shell
*/
void SelectShellDialog(Widget parent, Document *forWindow) {

	(void)forWindow;

	Widget shellSelDialog;
	Arg shellSelDialogArgs[2];
	XmString label;

	//  Set up the dialog.  
	XtSetArg(shellSelDialogArgs[0], XmNdialogStyle, XmDIALOG_FULL_APPLICATION_MODAL);
	XtSetArg(shellSelDialogArgs[1], XmNautoUnmanage, False);
	shellSelDialog = CreatePromptDialog(parent, "shellSelDialog", shellSelDialogArgs, 2);

	//  Fix dialog to our liking.  
	XtVaSetValues(XtParent(shellSelDialog), XmNtitle, "Command Shell", nullptr);
	XtAddCallback(shellSelDialog, XmNokCallback, shellSelOKCB, shellSelDialog);
	XtAddCallback(shellSelDialog, XmNcancelCallback, shellSelCancelCB, nullptr);
	XtUnmanageChild(XmSelectionBoxGetChild(shellSelDialog, XmDIALOG_HELP_BUTTON));
	label = XmStringCreateLocalizedEx("Enter shell path:");
	XtVaSetValues(shellSelDialog, XmNselectionLabelString, label, nullptr);
	XmStringFree(label);

	//  Set dialog's text to the current setting.  
	XmTextSetStringEx(XmSelectionBoxGetChild(shellSelDialog, XmDIALOG_TEXT), GetPrefShell());

	DoneWithShellSelDialog = False;

	//  Show dialog and wait until the user made her choice.  
	ManageDialogCenteredOnPointer(shellSelDialog);
	while (!DoneWithShellSelDialog) {
		XEvent event;
		XtAppNextEvent(XtWidgetToApplicationContext(parent), &event);
		ServerDispatchEvent(&event);
	}

	XtDestroyWidget(shellSelDialog);
}

static void shellSelOKCB(Widget widget, XtPointer clientData, XtPointer callData) {

	(void)widget;
	(void)callData;

	Widget shellSelDialog = static_cast<Widget>(clientData);
	String shellName = XtMalloc(MAXPATHLEN);
	struct stat attribute;

	//  Leave with a warning if the dialog is not up.  
	if (!XtIsRealized(shellSelDialog)) {
		fprintf(stderr, "nedit: Callback shellSelOKCB() illegally called.\n");
		return;
	}

	//  Get the string that the user entered and make sure it's ok.  
	shellName = XmTextGetString(XmSelectionBoxGetChild(shellSelDialog, XmDIALOG_TEXT));

	if (stat(shellName, &attribute) == -1) {
		int resp = QMessageBox::warning(nullptr /*shellSelDialog*/, QLatin1String("Command Shell"), QLatin1String("The selected shell is not available.\nDo you want to use it anyway?"), QMessageBox::Ok | QMessageBox::Cancel);
		if(resp == QMessageBox::Cancel) {
			return;
		}
	}

	SetPrefShell(shellName);
	XtFree(shellName);

	DoneWithShellSelDialog = True;
}

static void shellSelCancelCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)clientData;
	(void)callData;

	DoneWithShellSelDialog = True;
}

void freeLanguageModeRec(LanguageMode *lm) {

	XtFree(lm->name);
	XtFree(lm->recognitionExpr);
	XtFree(lm->defTipsFile);
	XtFree(lm->delimiters);
	for (int i = 0; i < lm->nExtensions; i++) {
		XtFree(lm->extensions[i]);
	}

	delete [] lm->extensions;
	delete lm;
}

/*
** Copy a LanguageMode data structure and all of the allocated data it contains
*/
LanguageMode *copyLanguageModeRec(LanguageMode *lm) {

	auto newLM = new LanguageMode;
	
	newLM->name        = XtStringDup(lm->name);
	newLM->nExtensions = lm->nExtensions;
	newLM->extensions  = new char *[lm->nExtensions];
	
	for (int i = 0; i < lm->nExtensions; i++) {
		newLM->extensions[i] = XtStringDup(lm->extensions[i]);
	}
	
	newLM->recognitionExpr = lm->recognitionExpr ? XtStringDup(lm->recognitionExpr) : nullptr;
	newLM->defTipsFile     = lm->defTipsFile     ? XtStringDup(lm->defTipsFile)     : nullptr;
	newLM->delimiters      = lm->delimiters      ? XtStringDup(lm->delimiters)      : nullptr;	
	newLM->wrapStyle       = lm->wrapStyle;
	newLM->indentStyle     = lm->indentStyle;
	newLM->tabDist         = lm->tabDist;
	newLM->emTabDist       = lm->emTabDist;
	return newLM;
}

/*
** Change the language mode to the one indexed by "mode", reseting word
** delimiters, syntax highlighting and other mode specific parameters
*/
static void reapplyLanguageMode(Document *window, int mode, int forceDefaults) {
	char *delimiters;
	int i, wrapMode, indentStyle, tabDist, emTabDist, highlight, oldEmTabDist;
	int wrapModeIsDef, tabDistIsDef, emTabDistIsDef, indentStyleIsDef;
	int highlightIsDef, haveHighlightPatterns, haveSmartIndentMacros;
	int oldMode = window->languageMode_;

	/* If the mode is the same, and changes aren't being forced (as might
	   happen with Save As...), don't mess with already correct settings */
	if (window->languageMode_ == mode && !forceDefaults)
		return;

	// Change the mode name stored in the window 
	window->languageMode_ = mode;

	// Decref oldMode's default calltips file if needed 
	if (oldMode != PLAIN_LANGUAGE_MODE && LanguageModes[oldMode]->defTipsFile) {
		DeleteTagsFile(LanguageModes[oldMode]->defTipsFile, TIP, False);
	}

	// Set delimiters for all text widgets 
	if (mode == PLAIN_LANGUAGE_MODE || LanguageModes[mode]->delimiters == nullptr)
		delimiters = GetPrefDelimiters();
	else
		delimiters = LanguageModes[mode]->delimiters;
	XtVaSetValues(window->textArea_, textNwordDelimiters, delimiters, nullptr);
	for (i = 0; i < window->nPanes_; i++)
		XtVaSetValues(window->textPanes_[i], textNautoIndent, delimiters, nullptr);

	/* Decide on desired values for language-specific parameters.  If a
	   parameter was set to its default value, set it to the new default,
	   otherwise, leave it alone */
	wrapModeIsDef = window->wrapMode_ == GetPrefWrap(oldMode);
	tabDistIsDef = window->buffer_->BufGetTabDistance() == GetPrefTabDist(oldMode);
	XtVaGetValues(window->textArea_, textNemulateTabs, &oldEmTabDist, nullptr);
	
	const char *oldlanguageModeName = LanguageModeName(oldMode);
	
	emTabDistIsDef   = oldEmTabDist == GetPrefEmTabDist(oldMode);
	indentStyleIsDef = window->indentStyle_ == GetPrefAutoIndent(oldMode)   || (GetPrefAutoIndent(oldMode) == SMART_INDENT && window->indentStyle_ == AUTO_INDENT && !SmartIndentMacrosAvailable(LanguageModeName(oldMode)));
	highlightIsDef   = window->highlightSyntax_ == GetPrefHighlightSyntax() || (GetPrefHighlightSyntax() && FindPatternSet(oldlanguageModeName ? oldlanguageModeName : "") == nullptr);
	wrapMode         = wrapModeIsDef                                       || forceDefaults ? GetPrefWrap(mode)        : window->wrapMode_;
	tabDist          = tabDistIsDef                                        || forceDefaults ? GetPrefTabDist(mode)     : window->buffer_->BufGetTabDistance();
	emTabDist        = emTabDistIsDef                                      || forceDefaults ? GetPrefEmTabDist(mode)   : oldEmTabDist;
	indentStyle      = indentStyleIsDef                                    || forceDefaults ? GetPrefAutoIndent(mode)  : window->indentStyle_;
	highlight        = highlightIsDef                                      || forceDefaults ? GetPrefHighlightSyntax() : window->highlightSyntax_;

	/* Dim/undim smart-indent and highlighting menu items depending on
	   whether patterns/macros are available */
	const char *languageModeName = LanguageModeName(mode);
	haveHighlightPatterns = FindPatternSet(languageModeName ? languageModeName : "") != nullptr;
	haveSmartIndentMacros = SmartIndentMacrosAvailable(LanguageModeName(mode));
	if (window->IsTopDocument()) {
		XtSetSensitive(window->highlightItem_, haveHighlightPatterns);
		XtSetSensitive(window->smartIndentItem_, haveSmartIndentMacros);
	}

	// Turn off requested options which are not available 
	highlight = haveHighlightPatterns && highlight;
	if (indentStyle == SMART_INDENT && !haveSmartIndentMacros)
		indentStyle = AUTO_INDENT;

	// Change highlighting 
	window->highlightSyntax_ = highlight;
	window->SetToggleButtonState(window->highlightItem_, highlight, False);
	StopHighlighting(window);

	// we defer highlighting to RaiseDocument() if doc is hidden 
	if (window->IsTopDocument() && highlight)
		StartHighlighting(window, False);

	// Force a change of smart indent macros (SetAutoIndent will re-start) 
	if (window->indentStyle_ == SMART_INDENT) {
		EndSmartIndent(window);
		window->indentStyle_ = AUTO_INDENT;
	}

	// set requested wrap, indent, and tabs 
	window->SetAutoWrap(wrapMode);
	window->SetAutoIndent(indentStyle);
	window->SetTabDist(tabDist);
	window->SetEmTabDist(emTabDist);

	// Load calltips files for new mode 
	if (mode != PLAIN_LANGUAGE_MODE && LanguageModes[mode]->defTipsFile) {
		AddTagsFile(LanguageModes[mode]->defTipsFile, TIP);
	}

	// Add/remove language specific menu items 
	UpdateUserMenus(window);
}

/*
** Find and return the name of the appropriate languange mode for
** the file in "window".  Returns a pointer to a string, which will
** remain valid until a change is made to the language modes list.
*/
static int matchLanguageMode(Document *window) {
	char *ext;
	int i, j, fileNameLen, extLen, beginPos, endPos, start;
	const char *versionExtendedPath;

	/*... look for an explicit mode statement first */

	// Do a regular expression search on for recognition pattern 
	std::string first200 = window->buffer_->BufGetRangeEx(0, 200);
	for (i = 0; i < NLanguageModes; i++) {
		if (LanguageModes[i]->recognitionExpr) {
			if (SearchString(first200, LanguageModes[i]->recognitionExpr, SEARCH_FORWARD, SEARCH_REGEX, False, 0, &beginPos, &endPos, nullptr, nullptr, nullptr)) {
				return i;
			}
		}
	}

	/* Look at file extension ("@@/" starts a ClearCase version extended path,
	   which gets appended after the file extension, and therefore must be
	   stripped off to recognize the extension to make ClearCase users happy) */
	fileNameLen = window->filename_.size();


	// TODO(eteran): this is playing some games with the c_str() that I don't think
	//               is a good idea. It would be better if GetClearCaseVersionExtendedPath
	//               returned string_view.
	if ((versionExtendedPath = GetClearCaseVersionExtendedPath(window->filename_.c_str())) != nullptr) {
		fileNameLen = versionExtendedPath - window->filename_.c_str();
	}

	for (i = 0; i < NLanguageModes; i++) {
		for (j = 0; j < LanguageModes[i]->nExtensions; j++) {
			ext = LanguageModes[i]->extensions[j];
			extLen = strlen(ext);
			start = fileNameLen - extLen;

			if (start >= 0 && !strncmp(&window->filename_[start], ext, extLen))
				return i;
		}
	}

	// no appropriate mode was found 
	return PLAIN_LANGUAGE_MODE;
}

static int loadLanguageModesStringEx(const std::string &string, int fileVer) {
	
	// TODO(eteran): implement this natively
	auto buffer = new char[string.size() + 1];
	strcpy(buffer, string.c_str());
	int r = loadLanguageModesString(buffer, fileVer);
	delete [] buffer;
	return r;

}

static int loadLanguageModesString(const char *inString, int fileVer) {
	const char *errMsg;
	char *styleName;
	const char *inPtr = inString;
	int i;

	for (;;) {

		// skip over blank space 
		inPtr += strspn(inPtr, " \t\n");

		/* Allocate a language mode structure to return, set unread fields to
		   empty so everything can be freed on errors by freeLanguageModeRec */
		auto lm = new LanguageMode;
		
		lm->nExtensions     = 0;
		lm->recognitionExpr = nullptr;
		lm->defTipsFile     = nullptr;
		lm->delimiters      = nullptr;

		// read language mode name 
		lm->name = ReadSymbolicField(&inPtr);
		if (!lm->name) {
			delete lm;
			return modeError(nullptr, inString, inPtr, "language mode name required");
		}
		if (!SkipDelimiter(&inPtr, &errMsg))
			return modeError(lm, inString, inPtr, errMsg);

		// read list of extensions 
		lm->extensions = readExtensionList(&inPtr, &lm->nExtensions);
		if (!SkipDelimiter(&inPtr, &errMsg))
			return modeError(lm, inString, inPtr, errMsg);

		// read the recognition regular expression 
		if (*inPtr == '\n' || *inPtr == '\0' || *inPtr == ':')
			lm->recognitionExpr = nullptr;
		else if (!ReadQuotedString(&inPtr, &errMsg, &lm->recognitionExpr))
			return modeError(lm, inString, inPtr, errMsg);
		if (!SkipDelimiter(&inPtr, &errMsg))
			return modeError(lm, inString, inPtr, errMsg);

		// read the indent style 
		styleName = ReadSymbolicField(&inPtr);
		if(!styleName)
			lm->indentStyle = DEFAULT_INDENT;
		else {
			for (i = 0; i < N_INDENT_STYLES; i++) {
				if (!strcmp(styleName, AutoIndentTypes[i])) {
					lm->indentStyle = i;
					break;
				}
			}
			XtFree(styleName);
			if (i == N_INDENT_STYLES)
				return modeError(lm, inString, inPtr, "unrecognized indent style");
		}
		if (!SkipDelimiter(&inPtr, &errMsg))
			return modeError(lm, inString, inPtr, errMsg);

		// read the wrap style 
		styleName = ReadSymbolicField(&inPtr);
		if(!styleName)
			lm->wrapStyle = DEFAULT_WRAP;
		else {
			for (i = 0; i < N_WRAP_STYLES; i++) {
				if (!strcmp(styleName, AutoWrapTypes[i])) {
					lm->wrapStyle = i;
					break;
				}
			}
			XtFree(styleName);
			if (i == N_WRAP_STYLES)
				return modeError(lm, inString, inPtr, "unrecognized wrap style");
		}
		if (!SkipDelimiter(&inPtr, &errMsg))
			return modeError(lm, inString, inPtr, errMsg);

		// read the tab distance 
		if (*inPtr == '\n' || *inPtr == '\0' || *inPtr == ':')
			lm->tabDist = DEFAULT_TAB_DIST;
		else if (!ReadNumericField(&inPtr, &lm->tabDist))
			return modeError(lm, inString, inPtr, "bad tab spacing");
		if (!SkipDelimiter(&inPtr, &errMsg))
			return modeError(lm, inString, inPtr, errMsg);

		// read emulated tab distance 
		if (*inPtr == '\n' || *inPtr == '\0' || *inPtr == ':')
			lm->emTabDist = DEFAULT_EM_TAB_DIST;
		else if (!ReadNumericField(&inPtr, &lm->emTabDist))
			return modeError(lm, inString, inPtr, "bad emulated tab spacing");
		if (!SkipDelimiter(&inPtr, &errMsg))
			return modeError(lm, inString, inPtr, errMsg);

		// read the delimiters string 
		if (*inPtr == '\n' || *inPtr == '\0' || *inPtr == ':')
			lm->delimiters = nullptr;
		else if (!ReadQuotedString(&inPtr, &errMsg, &lm->delimiters))
			return modeError(lm, inString, inPtr, errMsg);

		// After 5.3 all language modes need a default tips file field 
		if (!SkipDelimiter(&inPtr, &errMsg))
			if (fileVer > 5003)
				return modeError(lm, inString, inPtr, errMsg);

		// read the default tips file 
		if (*inPtr == '\n' || *inPtr == '\0')
			lm->defTipsFile = nullptr;
		else if (!ReadQuotedString(&inPtr, &errMsg, &lm->defTipsFile))
			return modeError(lm, inString, inPtr, errMsg);

		// pattern set was read correctly, add/replace it in the list 
		for (i = 0; i < NLanguageModes; i++) {
			if (!strcmp(LanguageModes[i]->name, lm->name)) {
				freeLanguageModeRec(LanguageModes[i]);
				LanguageModes[i] = lm;
				break;
			}
		}
		if (i == NLanguageModes) {
			LanguageModes[NLanguageModes++] = lm;
			if (NLanguageModes > MAX_LANGUAGE_MODES)
				return modeError(nullptr, inString, inPtr, "maximum allowable number of language modes exceeded");
		}

		// if the string ends here, we're done 
		inPtr += strspn(inPtr, " \t\n");
		if (*inPtr == '\0')
			return True;
	} // End for(;;) 
}

static QString WriteLanguageModesStringEx(void) {
	char *str;
	char numBuf[25];

	auto outBuf = new TextBuffer;

	for (int i = 0; i < NLanguageModes; i++) {
		outBuf->BufInsertEx(outBuf->BufGetLength(), "\t");
		outBuf->BufInsertEx(outBuf->BufGetLength(), LanguageModes[i]->name);
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		outBuf->BufInsertEx(outBuf->BufGetLength(), str = createExtString(LanguageModes[i]->extensions, LanguageModes[i]->nExtensions));
		XtFree(str);
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		if (LanguageModes[i]->recognitionExpr) {
			std::string str = MakeQuotedStringEx(LanguageModes[i]->recognitionExpr);
			outBuf->BufInsertEx(outBuf->BufGetLength(), str);
		}
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		if (LanguageModes[i]->indentStyle != DEFAULT_INDENT)
			outBuf->BufInsertEx(outBuf->BufGetLength(), AutoIndentTypes[LanguageModes[i]->indentStyle]);
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		if (LanguageModes[i]->wrapStyle != DEFAULT_WRAP)
			outBuf->BufInsertEx(outBuf->BufGetLength(), AutoWrapTypes[LanguageModes[i]->wrapStyle]);
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		if (LanguageModes[i]->tabDist != DEFAULT_TAB_DIST) {
			sprintf(numBuf, "%d", LanguageModes[i]->tabDist);
			outBuf->BufInsertEx(outBuf->BufGetLength(), numBuf);
		}
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		if (LanguageModes[i]->emTabDist != DEFAULT_EM_TAB_DIST) {
			sprintf(numBuf, "%d", LanguageModes[i]->emTabDist);
			outBuf->BufInsertEx(outBuf->BufGetLength(), numBuf);
		}
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		if (LanguageModes[i]->delimiters) {
			std::string str = MakeQuotedStringEx(LanguageModes[i]->delimiters);
			outBuf->BufInsertEx(outBuf->BufGetLength(), str);
		}
		outBuf->BufInsertEx(outBuf->BufGetLength(), ":");
		if (LanguageModes[i]->defTipsFile) {
			std::string str = MakeQuotedStringEx(LanguageModes[i]->defTipsFile);
			outBuf->BufInsertEx(outBuf->BufGetLength(), str);
		}

		outBuf->BufInsertEx(outBuf->BufGetLength(), "\n");
	}

	// Get the output, and lop off the trailing newline 
	std::string outStr = outBuf->BufGetRangeEx(0, outBuf->BufGetLength() - 1);
	delete outBuf;
	return QString::fromStdString(EscapeSensitiveCharsEx(outStr));
}

static char *createExtString(char **extensions, int nExtensions) {
	int e, length = 1;
	char *outStr, *outPtr;

	for (e = 0; e < nExtensions; e++)
		length += strlen(extensions[e]) + 1;
	outStr = outPtr = XtMalloc(length);
	for (e = 0; e < nExtensions; e++) {
		strcpy(outPtr, extensions[e]);
		outPtr += strlen(extensions[e]);
		*outPtr++ = ' ';
	}
	if (nExtensions == 0)
		*outPtr = '\0';
	else
		*(outPtr - 1) = '\0';
	return outStr;
}

static char **readExtensionList(const char **inPtr, int *nExtensions) {
	char *extensionList[MAX_FILE_EXTENSIONS];
	const char *strStart;
	int i, len;

	// skip over blank space 
	*inPtr += strspn(*inPtr, " \t");

	for (i = 0; i < MAX_FILE_EXTENSIONS && **inPtr != ':' && **inPtr != '\0'; i++) {
		*inPtr += strspn(*inPtr, " \t");
		strStart = *inPtr;
		while (**inPtr != ' ' && **inPtr != '\t' && **inPtr != ':' && **inPtr != '\0')
			(*inPtr)++;
		len = *inPtr - strStart;
		extensionList[i] = XtMalloc(len + 1);
		strncpy(extensionList[i], strStart, len);
		extensionList[i][len] = '\0';
	}
	*nExtensions = i;
	if (i == 0) {
		return nullptr;
	}
	
	auto retList = new char*[i];
	std::copy_n(extensionList, i, retList);
	return retList;
}

int ReadNumericField(const char **inPtr, int *value) {
	int charsRead;

	// skip over blank space 
	*inPtr += strspn(*inPtr, " \t");

	if (sscanf(*inPtr, "%d%n", value, &charsRead) != 1) {
		return False;
	}
	
	*inPtr += charsRead;
	return True;
}

/*
** Parse a symbolic field, skipping initial and trailing whitespace,
** stops on first invalid character or end of string.  Valid characters
** are letters, numbers, _, -, +, $, #, and internal whitespace.  Internal
** whitespace is compressed to single space characters.
*/
char *ReadSymbolicField(const char **inPtr) {
	char *outStr, *outPtr;
	const char *strStart;
	const char *strPtr;
	int len;

	// skip over initial blank space 
	*inPtr += strspn(*inPtr, " \t");

	/* Find the first invalid character or end of string to know how
	   much memory to allocate for the returned string */
	strStart = *inPtr;
	while (isalnum((unsigned char)**inPtr) || **inPtr == '_' || **inPtr == '-' || **inPtr == '+' || **inPtr == '$' || **inPtr == '#' || **inPtr == ' ' || **inPtr == '\t')
		(*inPtr)++;
	len = *inPtr - strStart;
	if (len == 0)
		return nullptr;
	outStr = outPtr = XtMalloc(len + 1);

	// Copy the string, compressing internal whitespace to a single space 
	strPtr = strStart;
	while (strPtr - strStart < len) {
		if (*strPtr == ' ' || *strPtr == '\t') {
			strPtr += strspn(strPtr, " \t");
			*outPtr++ = ' ';
		} else
			*outPtr++ = *strPtr++;
	}

	// If there's space on the end, take it back off 
	if (outPtr > outStr && *(outPtr - 1) == ' ')
		outPtr--;
	if (outPtr == outStr) {
		XtFree(outStr);
		return nullptr;
	}
	*outPtr = '\0';
	return outStr;
}

/*
** Parse a symbolic field, skipping initial and trailing whitespace,
** stops on first invalid character or end of string.  Valid characters
** are letters, numbers, _, -, +, $, #, and internal whitespace.  Internal
** whitespace is compressed to single space characters.
*/
QString ReadSymbolicFieldEx(const char **inPtr) {

	// skip over initial blank space 
	*inPtr += strspn(*inPtr, " \t");

	/* Find the first invalid character or end of string to know how
	   much memory to allocate for the returned string */
	const char *strStart = *inPtr;
	while (isalnum((unsigned char)**inPtr) || **inPtr == '_' || **inPtr == '-' || **inPtr == '+' || **inPtr == '$' || **inPtr == '#' || **inPtr == ' ' || **inPtr == '\t') {
		(*inPtr)++;
	}
	
	int len = *inPtr - strStart;
	if (len == 0) {
		return QString();
	}
	
	std::string outStr;
	outStr.reserve(len);
	
	auto outPtr = std::back_inserter(outStr);

	// Copy the string, compressing internal whitespace to a single space 
	const char *strPtr = strStart;
	while (strPtr - strStart < len) {
		if (*strPtr == ' ' || *strPtr == '\t') {
			strPtr += strspn(strPtr, " \t");
			*outPtr++ = ' ';
		} else {
			*outPtr++ = *strPtr++;
		}
	}

	// If there's space on the end, take it back off 
	if(!outStr.empty() && outStr.back() == ' ') {
		outStr.pop_back();	
	}
	
	if(outStr.empty()) {
		return QString();
	}
	
	return QString::fromStdString(outStr);
}

/*
** parse an individual quoted string.  Anything between
** double quotes is acceptable, quote characters can be escaped by "".
** Returns allocated string "string" containing
** argument minus quotes.  If not successful, returns False with
** (statically allocated) message in "errMsg".
*/
int ReadQuotedString(const char **inPtr, const char **errMsg, char **string) {
	char *outPtr;
	const char *c;

	// skip over blank space 
	*inPtr += strspn(*inPtr, " \t");

	// look for initial quote 
	if (**inPtr != '\"') {
		*errMsg = "expecting quoted string";
		return False;
	}
	(*inPtr)++;

	// calculate max length and allocate returned string 
	for (c = *inPtr;; c++) {
		if (*c == '\0') {
			*errMsg = "string not terminated";
			return False;
		} else if (*c == '\"') {
			if (*(c + 1) == '\"')
				c++;
			else
				break;
		}
	}

	// copy string up to end quote, transforming escaped quotes into quotes 
	*string = XtMalloc(c - *inPtr + 1);
	outPtr = *string;
	while (true) {
		if (**inPtr == '\"') {
			if (*(*inPtr + 1) == '\"')
				(*inPtr)++;
			else
				break;
		}
		*outPtr++ = *(*inPtr)++;
	}
	*outPtr = '\0';

	// skip end quote 
	(*inPtr)++;
	return True;
}

/*
** parse an individual quoted string.  Anything between
** double quotes is acceptable, quote characters can be escaped by "".
** Returns allocated string "string" containing
** argument minus quotes.  If not successful, returns False with
** (statically allocated) message in "errMsg".
*/
int ReadQuotedStringEx(const char **inPtr, const char **errMsg, QString *string) {
	const char *c;

	// skip over blank space 
	*inPtr += strspn(*inPtr, " \t");

	// look for initial quote 
	if (**inPtr != '\"') {
		*errMsg = "expecting quoted string";
		return False;
	}
	(*inPtr)++;

	// calculate max length and allocate returned string 
	for (c = *inPtr;; c++) {
		if (*c == '\0') {
			*errMsg = "string not terminated";
			return False;
		} else if (*c == '\"') {
			if (*(c + 1) == '\"')
				c++;
			else
				break;
		}
	}

	// copy string up to end quote, transforming escaped quotes into quotes
	QString str;
	str.reserve(c - *inPtr);
	
	auto outPtr = std::back_inserter(str);

	while (true) {
		if (**inPtr == '\"') {
			if (*(*inPtr + 1) == '\"')
				(*inPtr)++;
			else
				break;
		}
		*outPtr++ = QLatin1Char(*(*inPtr)++);
	}

	// skip end quote 
	(*inPtr)++;
	
	*string = str;
	return True;
}

/*
** Replace characters which the X resource file reader considers control
** characters, such that a string will read back as it appears in "string".
** (So far, newline characters are replaced with with \n\<newline> and
** backslashes with \\.  This has not been tested exhaustively, and
** probably should be.  It would certainly be more asthetic if other
** control characters were replaced as well).
**
** Returns an allocated string which must be freed by the caller with XtFree.
*/
char *EscapeSensitiveChars(const char *string) {
	const char *c;
	char *outStr, *outPtr;
	int length = 0;

	// calculate length and allocate returned string 
	for (c = string; *c != '\0'; c++) {
		if (*c == '\\')
			length++;
		else if (*c == '\n')
			length += 3;
		length++;
	}
	outStr = XtMalloc(length + 1);
	outPtr = outStr;

	// add backslashes 
	for (c = string; *c != '\0'; c++) {
		if (*c == '\\')
			*outPtr++ = '\\';
		else if (*c == '\n') {
			*outPtr++ = '\\';
			*outPtr++ = 'n';
			*outPtr++ = '\\';
		}
		*outPtr++ = *c;
	}
	*outPtr = '\0';
	return outStr;
}


/*
** Replace characters which the X resource file reader considers control
** characters, such that a string will read back as it appears in "string".
** (So far, newline characters are replaced with with \n\<newline> and
** backslashes with \\.  This has not been tested exhaustively, and
** probably should be.  It would certainly be more asthetic if other
** control characters were replaced as well).
**
** Returns an allocated string which must be freed by the caller with XtFree.
*/
std::string EscapeSensitiveCharsEx(view::string_view string) {

	int length = 0;

	// calculate length and allocate returned string 
	for(char ch : string) {
		if (ch == '\\')
			length++;
		else if (ch == '\n')
			length += 3;
		length++;
	}
	
	std::string outStr;
	outStr.reserve(length);
	auto outPtr = std::back_inserter(outStr);

	// add backslashes 
	for(char ch : string) {
		if (ch == '\\')
			*outPtr++ = '\\';
		else if (ch == '\n') {
			*outPtr++ = '\\';
			*outPtr++ = 'n';
			*outPtr++ = '\\';
		}
		*outPtr++ = ch;
	}

	return outStr;
}

/*
** Adds double quotes around a string and escape existing double quote
** characters with two double quotes.  Enables the string to be read back
** by ReadQuotedString.
*/
std::string MakeQuotedStringEx(view::string_view string) {

	int length = 0;

	// calculate length and allocate returned string 
	for(char ch: string) {
		if (ch == '\"') {
			length++;
		}
		length++;
	}
	
	std::string outStr;
	outStr.reserve(length + 3);
	auto outPtr = std::back_inserter(outStr);

	// add starting quote 
	*outPtr++ = '\"';

	// copy string, escaping quotes with "" 
	for(char ch: string) {
		if (ch == '\"') {
			*outPtr++ = '\"';
		}
		*outPtr++ = ch;
	}

	// add ending quote 
	*outPtr++ = '\"';

	return outStr;
}

/*
** Read a dialog text field containing a symbolic name (language mode names,
** style names, highlight pattern names, colors, and fonts), clean the
** entered text of leading and trailing whitespace, compress all
** internal whitespace to one space character, and check it over for
** colons, which interfere with the preferences file reader/writer syntax.
** Returns nullptr on error, and puts up a dialog if silent is False.  Returns
** an empty string if the text field is blank.
*/
char *ReadSymbolicFieldTextWidget(Widget textW, const char *fieldName, int silent) {
	char *parsedString;

	// read from the text widget 
	char *string = XmTextGetString(textW);
	const char *stringPtr = string;

	/* parse it with the same routine used to read symbolic fields from
	   files.  If the string is not read entirely, there are invalid
	   characters, so warn the user if not in silent mode. */
	parsedString = ReadSymbolicField(&stringPtr);
	if (*stringPtr != '\0') {
		if (!silent) {		
			QMessageBox::warning(nullptr /*textW*/, QLatin1String("Invalid Character"), QString(QLatin1String("Invalid character \"%1\" in %2")).arg(QLatin1Char(stringPtr[1])).arg(QLatin1String(fieldName)));
			XmProcessTraversal(textW, XmTRAVERSE_CURRENT);
		}
		
		XtFree(string);
		XtFree(parsedString);
		return nullptr;
	}
	XtFree(string);
	if(!parsedString) {
		parsedString = XtStringDup("");
	}
	return parsedString;
}

/*
** Read a dialog text field containing a symbolic name (language mode names,
** style names, highlight pattern names, colors, and fonts), clean the
** entered text of leading and trailing whitespace, compress all
** internal whitespace to one space character, and check it over for
** colons, which interfere with the preferences file reader/writer syntax.
** Returns nullptr on error, and puts up a dialog if silent is False.  Returns
** an empty string if the text field is blank.
*/
QString ReadSymbolicFieldTextWidgetEx(Widget textW, const char *fieldName, int silent) {

	// read from the text widget 
	QString string = XmTextGetStringEx(textW);
	const std::string string_copy = string.toStdString();
	const char *stringPtr = &string_copy[0];

	/* parse it with the same routine used to read symbolic fields from
	   files.  If the string is not read entirely, there are invalid
	   characters, so warn the user if not in silent mode. */
	QString parsedString = ReadSymbolicFieldEx(&stringPtr);
	
	if (*stringPtr != '\0') {
		if (!silent) {
			QMessageBox::warning(nullptr /*textW*/, QLatin1String("Invalid Character"), QString(QLatin1String("Invalid character \"%1\" in %2")).arg(QLatin1Char(stringPtr[1])).arg(QLatin1String(fieldName)));
			XmProcessTraversal(textW, XmTRAVERSE_CURRENT);
		}
		return QString();
	}
	
	if(parsedString.isNull()) {
		parsedString = QLatin1String("");
	}
	
	return parsedString;
}

/*
** Create a pulldown menu pane with the names of the current language modes.
** XmNuserData for each item contains the language mode name.
*/
Widget CreateLanguageModeMenu(Widget parent, XtCallbackProc cbProc, void *cbArg) {
	Widget menu, btn;
	int i;
	XmString s1;

	menu = CreatePulldownMenu(parent, "languageModes", nullptr, 0);
	for (i = 0; i < NLanguageModes; i++) {
		btn = XtVaCreateManagedWidget("languageMode", xmPushButtonGadgetClass, menu, XmNlabelString, s1 = XmStringCreateSimpleEx(LanguageModes[i]->name), XmNmarginHeight, 0, XmNuserData, LanguageModes[i]->name, nullptr);
		XmStringFree(s1);
		XtAddCallback(btn, XmNactivateCallback, cbProc, cbArg);
	}
	return menu;
}

/*
** Set the language mode menu in option menu "optMenu" to
** show a particular language mode
*/
void SetLangModeMenu(Widget optMenu, const char *modeName) {
	int i;
	Cardinal nItems;
	WidgetList items;
	Widget pulldown, selectedItem;
	char *itemName;

	XtVaGetValues(optMenu, XmNsubMenuId, &pulldown, nullptr);
	XtVaGetValues(pulldown, XmNchildren, &items, XmNnumChildren, &nItems, nullptr);
	if (nItems == 0)
		return;
	selectedItem = items[0];
	for (i = 0; i < (int)nItems; i++) {
		XtVaGetValues(items[i], XmNuserData, &itemName, nullptr);
		if (!strcmp(itemName, modeName)) {
			selectedItem = items[i];
			break;
		}
	}
	XtVaSetValues(optMenu, XmNmenuHistory, selectedItem, nullptr);
}

/*
** Create a submenu for chosing language mode for the current window.
*/
void CreateLanguageModeSubMenu(Document *window, const Widget parent, const char *name, const char *label, const char mnemonic) {
	XmString string = XmStringCreateSimpleEx((char *)label);

	window->langModeCascade_ = XtVaCreateManagedWidget(name, xmCascadeButtonGadgetClass, parent, XmNlabelString, string, XmNmnemonic, mnemonic, XmNsubMenuId, nullptr, nullptr);
	XmStringFree(string);

	updateLanguageModeSubmenu(window);
}

/*
** Re-build the language mode sub-menu using the current data stored
** in the master list: LanguageModes.
*/
void updateLanguageModeSubmenu(Document *window) {
	long i;
	XmString s1;
	Widget menu, btn;
	Arg args[1] = {{XmNradioBehavior, (XtArgVal)True}};

	// Destroy and re-create the menu pane 
	XtVaGetValues(window->langModeCascade_, XmNsubMenuId, &menu, nullptr);
	if(menu)
		XtDestroyWidget(menu);
	menu = CreatePulldownMenu(XtParent(window->langModeCascade_), "languageModes", args, 1);
	btn =
	    XtVaCreateManagedWidget("languageMode", xmToggleButtonGadgetClass, menu, XmNlabelString, s1 = XmStringCreateSimpleEx("Plain"), XmNuserData, PLAIN_LANGUAGE_MODE, XmNset, window->languageMode_ == PLAIN_LANGUAGE_MODE, nullptr);
	XmStringFree(s1);
	XtAddCallback(btn, XmNvalueChangedCallback, setLangModeCB, window);
	for (i = 0; i < NLanguageModes; i++) {
		btn = XtVaCreateManagedWidget("languageMode", xmToggleButtonGadgetClass, menu, XmNlabelString, s1 = XmStringCreateSimpleEx(LanguageModes[i]->name), XmNmarginHeight, 0, XmNuserData, i, XmNset, window->languageMode_ == i,
		                              nullptr);
		XmStringFree(s1);
		XtAddCallback(btn, XmNvalueChangedCallback, setLangModeCB, window);
	}
	XtVaSetValues(window->langModeCascade_, XmNsubMenuId, menu, nullptr);
}

static void setLangModeCB(Widget w, XtPointer clientData, XtPointer callData) {

	(void)w;
	(void)callData;
	(void)clientData;

	Document *window = Document::WidgetToWindow(MENU_WIDGET(w));
	const char *params[1];
	void *mode;

	if (!XmToggleButtonGetState(w))
		return;

	// get name of language mode stored in userData field of menu item 
	XtVaGetValues(w, XmNuserData, &mode, nullptr);

	// If the mode didn't change, do nothing 
	if (window->languageMode_ == (long)mode)
		return;

	// redo syntax highlighting word delimiters, etc. 
	/*
	    reapplyLanguageMode(window, (int)mode, False);
	*/
	params[0] = (((long)mode) == PLAIN_LANGUAGE_MODE) ? "" : LanguageModes[(long)mode]->name;
	XtCallActionProc(window->textArea_, "set_language_mode", nullptr, (char **)params, 1);
}

/*
** Skip a delimiter and it's surrounding whitespace
*/
int SkipDelimiter(const char **inPtr, const char **errMsg) {
	*inPtr += strspn(*inPtr, " \t");
	if (**inPtr != ':') {
		*errMsg = "syntax error";
		return False;
	}
	(*inPtr)++;
	*inPtr += strspn(*inPtr, " \t");
	return True;
}

/*
** Skip an optional separator and its surrounding whitespace
** return true if delimiter found
*/
int SkipOptSeparator(char separator, const char **inPtr) {
	*inPtr += strspn(*inPtr, " \t");
	if (**inPtr != separator) {
		return False;
	}
	(*inPtr)++;
	*inPtr += strspn(*inPtr, " \t");
	return True;
}

/*
** Short-hand error processing for language mode parsing errors, frees
** lm (if non-null), prints a formatted message explaining where the
** error is, and returns False;
*/
static int modeError(LanguageMode *lm, const char *stringStart, const char *stoppedAt, const char *message) {
	if(lm) {
		freeLanguageModeRec(lm);
	}

	return ParseError(nullptr, stringStart, stoppedAt, "language mode specification", message);
}

/*
** Report parsing errors in resource strings or macros, formatted nicely so
** the user can tell where things became botched.  Errors can be sent either
** to stderr, or displayed in a dialog.  For stderr, pass toDialog as nullptr.
** For a dialog, pass the dialog parent in toDialog.
*/

bool ParseErrorEx(QWidget *toDialog, const QString &string, int stoppedAt, const QString &errorIn, const QString message) {
	int nNonWhite = 0;
	int c;

	for (c = stoppedAt; c >= 0; c--) {
		if (c == 0) {
			break;
		} else if (string[c] == QLatin1Char('\n') && nNonWhite >= 5) {
			break;
		} else if (string[c] != QLatin1Char(' ') && string[c] != QLatin1Char('\t')) {
			nNonWhite++;
		}
	}
	
	int len = stoppedAt - c + (stoppedAt == string.size() ? 0 : 1);

	QString errorLine = QString(QLatin1String("%1<==")).arg(string.mid(c, len));

	if(!toDialog) {
		fprintf(stderr, "NEdit: %s in %s:\n%s\n", message.toLatin1().data(), errorIn.toLatin1().data(), errorLine.toLatin1().data());
	} else {
		QMessageBox::warning(toDialog, QLatin1String("Parse Error"), QString(QLatin1String("%1 in %2:\n%3")).arg(message).arg(errorIn).arg(errorLine));
	}
	
	return false;
}

int ParseError(Widget toDialog, const char *stringStart, const char *stoppedAt, const char *errorIn, const char *message) {
	int len;
	int nNonWhite = 0;
	const char *c;

	for (c = stoppedAt; c >= stringStart; c--) {
		if (c == stringStart)
			break;
		else if (*c == '\n' && nNonWhite >= 5)
			break;
		else if (*c != ' ' && *c != '\t')
			nNonWhite++;
	}
	len = stoppedAt - c + (*stoppedAt == '\0' ? 0 : 1);

	auto errorLine = new char[len + 4];
	strncpy(errorLine, c, len);
	errorLine[len++] = '<';
	errorLine[len++] = '=';
	errorLine[len++] = '=';
	errorLine[len] = '\0';
	
	if(!toDialog) {
		fprintf(stderr, "NEdit: %s in %s:\n%s\n", message, errorIn, errorLine);
	} else {
		QMessageBox::warning(nullptr /*toDialog*/, QLatin1String("Parse Error"), QString(QLatin1String("%1 in %2:\n%3")).arg(QLatin1String(message)).arg(QLatin1String(errorIn)).arg(QLatin1String(errorLine)));
	}
	
	delete [] errorLine;
	return False;
}

/*
** Compare two strings which may be nullptr
*/
int AllocatedStringsDiffer(const char *s1, const char *s2) {
	if (s1 == nullptr && s2 == nullptr)
		return False;
	if (s1 == nullptr || s2 == nullptr)
		return True;
	return strcmp(s1, s2);
}

static void updatePatternsTo5dot1(void) {
	const char *htmlDefaultExpr = "^[ \t]*HTML[ \t]*:[ \t]*Default[ \t]*$";
	const char *vhdlAnchorExpr = "^[ \t]*VHDL:";
	
	
	std::string newHighlight   = TempStringPrefs.highlight.toStdString();
	std::string newStyles      = TempStringPrefs.styles.toStdString();
	std::string newLanguage    = TempStringPrefs.language.toStdString();
	std::string newSmartIndent = TempStringPrefs.smartIndent.toStdString();

	/* Add new patterns if there aren't already existing patterns with
	   the same name.  If possible, insert before VHDL in language mode
	   list.  If not, just add to end */
	if (!regexFind(newHighlight, "^[ \t]*PostScript:")) {
		newHighlight = spliceStringEx(newHighlight, "PostScript:Default", vhdlAnchorExpr);
	}
	
	if (!regexFind(newLanguage, "^[ \t]*PostScript:")) {
		newLanguage = spliceStringEx(newLanguage, "PostScript:.ps .PS .eps .EPS .epsf .epsi::::::", vhdlAnchorExpr);
	}
	
	if (!regexFind(newHighlight, "^[ \t]*Lex:")) {
		newHighlight = spliceStringEx(newHighlight, "Lex:Default", vhdlAnchorExpr);
	}
	
	if (!regexFind(newLanguage, "^[ \t]*Lex:")) {
		newLanguage = spliceStringEx(newLanguage, "Lex:.lex::::::", vhdlAnchorExpr);
	}
	
	if (!regexFind(newHighlight, "^[ \t]*SQL:")) {
		newHighlight = spliceStringEx(newHighlight, "SQL:Default", vhdlAnchorExpr);
	}
	
	if (!regexFind(newLanguage, "^[ \t]*SQL:")) {
		newLanguage = spliceStringEx(newLanguage, "SQL:.sql::::::", vhdlAnchorExpr);
	}
	if (!regexFind(newHighlight, "^[ \t]*Matlab:")) {
		newHighlight = spliceStringEx(newHighlight, "Matlab:Default", vhdlAnchorExpr);
	}
	
	if (!regexFind(newLanguage, "^[ \t]*Matlab:")) {
		newLanguage = spliceStringEx(newLanguage, "Matlab:..m .oct .sci::::::", vhdlAnchorExpr);
	}
	
	if (!regexFind(newSmartIndent, "^[ \t]*Matlab:")) {
		newSmartIndent = spliceStringEx(newSmartIndent, "Matlab:Default", nullptr);
	}
	
	if (!regexFind(newStyles, "^[ \t]*Label:")) {
		newStyles = spliceStringEx(newStyles, "Label:red:Italic", "^[ \t]*Flag:");
	}
	
	if (!regexFind(newStyles, "^[ \t]*Storage Type1:")) {
		newStyles = spliceStringEx(newStyles, "Storage Type1:saddle brown:Bold", "^[ \t]*String:");
	}

	/* Replace html pattern with sgml html pattern, as long as there
	   isn't an existing html pattern which will be overwritten */
	if (regexFind(newHighlight, htmlDefaultExpr)) {
		regexReplaceEx(&newHighlight, htmlDefaultExpr, "SGML HTML:Default");
		
		if (!regexReplaceEx(&newLanguage, "^[ \t]*HTML:.*$", "SGML HTML:.sgml .sgm .html .htm:\"\\<(?ihtml)\\>\":::::\n")) {
			newLanguage = spliceStringEx(newLanguage, "SGML HTML:.sgml .sgm .html .htm:\"\\<(?ihtml)\\>\":::::\n", vhdlAnchorExpr);
		}
	}
	
	TempStringPrefs.smartIndent = QString::fromStdString(newSmartIndent);
	TempStringPrefs.language    = QString::fromStdString(newLanguage);
	TempStringPrefs.styles      = QString::fromStdString(newStyles);
	TempStringPrefs.highlight   = QString::fromStdString(newHighlight);
}

static void updatePatternsTo5dot2(void) {

	const char *cppLm5dot1 = "^[ \t]*C\\+\\+:\\.cc \\.hh \\.C \\.H \\.i \\.cxx "
	                         "\\.hxx::::::\"\\.,/\\\\`'!\\|@#%\\^&\\*\\(\\)-=\\+\\{\\}\\[\\]\"\":;\\<\\>\\?~\"";
	const char *perlLm5dot1 = "^[ \t]*Perl:\\.pl \\.pm \\.p5:\"\\^\\[ \\\\t\\]\\*#\\[ \\\\t\\]\\*!\\.\\*perl\":::::";
	const char *psLm5dot1 = "^[ \t]*PostScript:\\.ps \\.PS \\.eps \\.EPS \\.epsf \\.epsi:\"\\^%!\":::::\"/%\\(\\)\\{\\}\\[\\]\\<\\>\"";
	const char *shLm5dot1 = "^[ \t]*Sh Ksh Bash:\\.sh \\.bash \\.ksh \\.profile:\"\\^\\[ \\\\t\\]\\*#\\[ "
	                        "\\\\t\\]\\*!\\[ \\\\t\\]\\*/bin/\\(sh\\|ksh\\|bash\\)\":::::";
	const char *tclLm5dot1 = "^[ \t]*Tcl:\\.tcl::::::";

	const char *cppLm5dot2 = "C++:.cc .hh .C .H .i .cxx .hxx .cpp::::::\".,/\\`'!|@#%^&*()-=+{}[]\"\":;<>?~\"";
	const char *perlLm5dot2 = "Perl:.pl .pm .p5 .PL:\"^[ \\t]*#[ \\t]*!.*perl\":Auto:None:::\".,/\\\\`'!$@#%^&*()-=+{}[]\"\":;<>?~|\"";
	const char *psLm5dot2 = "PostScript:.ps .eps .epsf .epsi:\"^%!\":::::\"/%(){}[]<>\"";
	const char *shLm5dot2 = "Sh Ksh Bash:.sh .bash .ksh .profile .bashrc .bash_logout .bash_login .bash_profile:\"^[ "
	                        "\\t]*#[ \\t]*![ \\t]*/.*bin/(sh|ksh|bash)\":::::";
	const char *tclLm5dot2 = "Tcl:.tcl .tk .itcl .itk::Smart:None:::";

	const char *cssLm5dot2 = "CSS:css::Auto:None:::\".,/\\`'!|@#%^&*()=+{}[]\"\":;<>?~\"";
	const char *reLm5dot2 = "Regex:.reg .regex:\"\\(\\?[:#=!iInN].+\\)\":None:Continuous:::";
	const char *xmlLm5dot2 = "XML:.xml .xsl .dtd:\"\\<(?i\\?xml|!doctype)\"::None:::\"<>/=\"\"'()+*?|\"";

	const char *cssHl5dot2 = "CSS:Default";
	const char *reHl5dot2 = "Regex:Default";
	const char *xmlHl5dot2 = "XML:Default";

	const char *ptrStyle = "Pointer:#660000:Bold";
	const char *reStyle = "Regex:#009944:Bold";
	const char *wrnStyle = "Warning:brown2:Italic";
	
	std::string newHighlight = TempStringPrefs.highlight.toStdString();
	std::string newStyles    = TempStringPrefs.styles.toStdString();
	std::string newLanguage  = TempStringPrefs.language.toStdString();

	/* First upgrade modified language modes, only if the user hasn't
	   altered the default 5.1 definitions. */
	if (regexFind(newLanguage, cppLm5dot1))
		regexReplaceEx(&newLanguage, cppLm5dot1, cppLm5dot2);
	if (regexFind(newLanguage, perlLm5dot1))
		regexReplaceEx(&newLanguage, perlLm5dot1, perlLm5dot2);
	if (regexFind(newLanguage, psLm5dot1))
		regexReplaceEx(&newLanguage, psLm5dot1, psLm5dot2);
	if (regexFind(newLanguage, shLm5dot1))
		regexReplaceEx(&newLanguage, shLm5dot1, shLm5dot2);
	if (regexFind(newLanguage, tclLm5dot1))
		regexReplaceEx(&newLanguage, tclLm5dot1, tclLm5dot2);

	/* Then append the new modes (trying to keep them in alphabetical order
	   makes no sense, since 5.1 didn't use alphabetical order). */
	if (!regexFind(newLanguage, "^[ \t]*CSS:"))
		newLanguage = spliceStringEx(newLanguage, cssLm5dot2, nullptr);
	if (!regexFind(newLanguage, "^[ \t]*Regex:"))
		newLanguage = spliceStringEx(newLanguage, reLm5dot2, nullptr);
	if (!regexFind(newLanguage, "^[ \t]*XML:"))
		newLanguage = spliceStringEx(newLanguage, xmlLm5dot2, nullptr);

	/* Enable default highlighting patterns for these modes, unless already
	   present */
	if (!regexFind(newHighlight, "^[ \t]*CSS:"))
		newHighlight = spliceStringEx(newHighlight, cssHl5dot2, nullptr);
	if (!regexFind(newHighlight, "^[ \t]*Regex:"))
		newHighlight = spliceStringEx(newHighlight, reHl5dot2, nullptr);
	if (!regexFind(newHighlight, "^[ \t]*XML:"))
		newHighlight = spliceStringEx(newHighlight, xmlHl5dot2, nullptr);

	// Finally, append the new highlight styles 

	if (!regexFind(newStyles, "^[ \t]*Warning:"))
		newStyles = spliceStringEx(newStyles, wrnStyle, nullptr);
	if (!regexFind(newStyles, "^[ \t]*Regex:"))
		newStyles = spliceStringEx(newStyles, reStyle, "^[ \t]*Warning:");
	if (!regexFind(newStyles, "^[ \t]*Pointer:"))
		newStyles = spliceStringEx(newStyles, ptrStyle, "^[ \t]*Regex:");
		
	
	TempStringPrefs.language  = QString::fromStdString(newLanguage);
	TempStringPrefs.styles    = QString::fromStdString(newStyles);
	TempStringPrefs.highlight = QString::fromStdString(newHighlight);
}

static void updatePatternsTo5dot3(void) {
	// This is a bogus function on non-VMS 
}

static void updatePatternsTo5dot4(void) {

	const char *pyLm5dot3 = "Python:\\.py:\"\\^#!\\.\\*python\":Auto:None::::?\n";
	const char *xrLm5dot3 = "X Resources:\\.Xresources \\.Xdefaults "
	                        "\\.nedit:\"\\^\\[!#\\]\\.\\*\\(\\[Aa\\]pp\\|\\[Xx\\]\\)\\.\\*\\[Dd\\]efaults\"::::::?\n";

	const char *pyLm5dot4 = "Python:.py:\"^#!.*python\":Auto:None:::\"!\"\"#$%&'()*+,-./:;<=>?@[\\\\]^`{|}~\":\n";
	const char *xrLm5dot4 = "X Resources:.Xresources .Xdefaults .nedit nedit.rc:\"^[!#].*([Aa]pp|[Xx]).*[Dd]efaults\"::::::\n";

	std::string newStyles   = TempStringPrefs.styles.toStdString();
	std::string newLanguage = TempStringPrefs.language.toStdString();

	/* Upgrade modified language modes, only if the user hasn't
	   altered the default 5.3 definitions. */
	if (regexFind(newLanguage, pyLm5dot3))
		regexReplaceEx(&newLanguage, pyLm5dot3, pyLm5dot4);
	if (regexFind(newLanguage, xrLm5dot3))
		regexReplaceEx(&newLanguage, xrLm5dot3, xrLm5dot4);

	// Add new styles 
	if (!regexFind(newStyles, "^[ \t]*Identifier2:")) {
		newStyles = spliceStringEx(newStyles, "Identifier2:SteelBlue:Plain", "^[ \t]*Subroutine:");
	}
	
	TempStringPrefs.language  = QString::fromStdString(newLanguage);
	TempStringPrefs.styles    = QString::fromStdString(newStyles);
}

static void updatePatternsTo5dot6(void) {
	const char *pats[] = {"Csh:\\.csh \\.cshrc \\.login \\.logout:\"\\^\\[ \\\\t\\]\\*#\\[ \\\\t\\]\\*!\\[ "
	                      "\\\\t\\]\\*/bin/csh\"::::::\\n",
	                      "Csh:.csh .cshrc .tcshrc .login .logout:\"^[ \\t]*#[ \\t]*![ \\t]*/bin/t?csh\"::::::\n", "LaTeX:\\.tex \\.sty \\.cls \\.ltx \\.ins:::::::\\n", "LaTeX:.tex .sty .cls .ltx .ins .clo .fd:::::::\n",
	                      "X Resources:\\.Xresources \\.Xdefaults "
	                      "\\.nedit:\"\\^\\[!#\\]\\.\\*\\(\\[Aa\\]pp\\|\\[Xx\\]\\)\\.\\*\\[Dd\\]efaults\"::::::\\n",
	                      "X Resources:.Xresources .Xdefaults .nedit .pats nedit.rc:\"^[!#].*([Aa]pp|[Xx]).*[Dd]efaults\"::::::\n", nullptr};

	std::string newStyles   = TempStringPrefs.styles.toStdString();
	std::string newLanguage = TempStringPrefs.language.toStdString();

	/* Upgrade modified language modes, only if the user hasn't
	   altered the default 5.5 definitions. */
	for (int i = 0; pats[i]; i += 2) {
		if (regexFind(newLanguage, pats[i])) {
			regexReplaceEx(&newLanguage, pats[i], pats[i + 1]);
		}
	}

	// Add new styles 
	if (!regexFind(newStyles, "^[ \t]*Bracket:")) {
		newStyles = spliceStringEx(newStyles, "Bracket:dark blue:Bold", "^[ \t]*Storage Type:");
	}
	
	if (!regexFind(newStyles, "^[ \t]*Operator:")) {
		newStyles = spliceStringEx(newStyles, "Operator:dark blue:Bold", "^[ \t]*Bracket:");
	}
		
	TempStringPrefs.styles   = QString::fromStdString(newStyles);
	TempStringPrefs.language = QString::fromStdString(newLanguage);
}

/*
 * We migrate a color from the X resources to the prefs if:
 *      1.  The prefs entry is equal to the default entry
 *      2.  The X resource is not equal to the default entry
 */
static void migrateColor(XrmDatabase prefDB, XrmDatabase appDB, const char *clazz, const char *name, int color_index, const char *default_val) {
	char *type, *valueString;
	XrmValue rsrcValue;

	/* If this color has been customized in the color dialog then use
	    that value */
	if (strcmp(default_val, PrefData.colorNames[color_index]))
		return;

	// Retrieve the value of the resource from the DB 
	if (XrmGetResource(prefDB, name, clazz, &type, &rsrcValue)) {
		if (strcmp(type, XmRString)) {
			fprintf(stderr, "Internal Error: Unexpected resource type, %s\n", type);
			return;
		}
		valueString = rsrcValue.addr;
	} else if (XrmGetResource(appDB, name, clazz, &type, &rsrcValue)) {
		if (strcmp(type, XmRString)) {
			fprintf(stderr, "Internal Error: Unexpected resource type, %s\n", type);
			return;
		}
		valueString = rsrcValue.addr;
	} else
		// No resources set 
		return;

	// An X resource is set.  If it's non-default, update the prefs. 
	if (strcmp(valueString, default_val)) {
		strncpy(PrefData.colorNames[color_index], valueString, MAX_COLOR_LEN);
	}
}

/*
 * In 5.4 we moved color preferences from X resources to a color dialog,
 * meaning they're in the normal prefs system.  Users who have customized
 * their colors with X resources would probably prefer not to have to redo
 * the customization in the dialog, so we migrate them to the prefs for them.
 */
static void migrateColorResources(XrmDatabase prefDB, XrmDatabase appDB) {
	migrateColor(prefDB, appDB, APP_CLASS ".Text.Foreground", APP_NAME ".text.foreground", TEXT_FG_COLOR, NEDIT_DEFAULT_FG);
	migrateColor(prefDB, appDB, APP_CLASS ".Text.Background", APP_NAME ".text.background", TEXT_BG_COLOR, NEDIT_DEFAULT_TEXT_BG);
	migrateColor(prefDB, appDB, APP_CLASS ".Text.SelectForeground", APP_NAME ".text.selectForeground", SELECT_FG_COLOR, NEDIT_DEFAULT_SEL_FG);
	migrateColor(prefDB, appDB, APP_CLASS ".Text.SelectBackground", APP_NAME ".text.selectBackground", SELECT_BG_COLOR, NEDIT_DEFAULT_SEL_BG);
	migrateColor(prefDB, appDB, APP_CLASS ".Text.HighlightForeground", APP_NAME ".text.highlightForeground", HILITE_FG_COLOR, NEDIT_DEFAULT_HI_FG);
	migrateColor(prefDB, appDB, APP_CLASS ".Text.HighlightBackground", APP_NAME ".text.highlightBackground", HILITE_BG_COLOR, NEDIT_DEFAULT_HI_BG);
	migrateColor(prefDB, appDB, APP_CLASS ".Text.LineNumForeground", APP_NAME ".text.lineNumForeground", LINENO_FG_COLOR, NEDIT_DEFAULT_LINENO_FG);
	migrateColor(prefDB, appDB, APP_CLASS ".Text.CursorForeground", APP_NAME ".text.cursorForeground", CURSOR_FG_COLOR, NEDIT_DEFAULT_CURSOR_FG);
}

/*
** Inserts a string into intoString, reallocating it with XtMalloc.  If
** regular expression atExpr is found, inserts the string before atExpr
** followed by a newline.  If atExpr is not found, inserts insertString
** at the end, PRECEDED by a newline.
*/
static std::string spliceStringEx(const std::string &intoString, view::string_view insertString, const char *atExpr) {
	int beginPos;
	int endPos;
	
	std::string newString;
	newString.reserve(intoString.size() + insertString.size());

	if (atExpr && SearchString(intoString, atExpr, SEARCH_FORWARD, SEARCH_REGEX, false, 0, &beginPos, &endPos, nullptr, nullptr, nullptr)) {
		
		newString.append(intoString, 0, beginPos);
		newString.append(insertString.begin(), insertString.end());	
		newString.append("\n");
		newString.append(intoString, beginPos, std::string::npos);
	} else {
		newString.append(intoString);
		newString.append("\n");
		newString.append(insertString.begin(), insertString.end());	
	}

	return newString;
}

/*
** Simplified regular expression search routine which just returns true
** or false depending on whether inString matches expr
*/
static int regexFind(view::string_view inString, const char *expr) {
	int beginPos, endPos;
	return SearchString(inString, expr, SEARCH_FORWARD, SEARCH_REGEX, false, 0, &beginPos, &endPos, nullptr, nullptr, nullptr);
}

/*
** Simplified case-sensisitive string search routine which just
** returns true or false depending on whether inString matches expr
*/
static int caseFind(view::string_view inString, const char *expr) {
	int beginPos;
	int endPos;
	return SearchString(inString, expr, SEARCH_FORWARD, SEARCH_CASE_SENSE, false, 0, &beginPos, &endPos, nullptr, nullptr, nullptr);
}


/*
** Common implementation for simplified string replacement routines.
*/
static bool stringReplaceEx(std::string *inString, const char *expr, const char *replaceWith, int searchType, int replaceLen) {
	
	const std::string &oldString = *inString;
	
	int beginPos;
	int endPos;
	int inLen = oldString.size();

	if (0 >= replaceLen)
		replaceLen = strlen(replaceWith);
		
	if (!SearchString(oldString, expr, SEARCH_FORWARD, searchType, False, 0, &beginPos, &endPos, nullptr, nullptr, nullptr)) {
		return false;
	}

	std::string newString;
	newString.reserve(oldString.size() + replaceLen);
	newString.append(oldString.substr(0, beginPos));
	newString.append(replaceWith, replaceLen);
	newString.append(oldString.substr(endPos, inLen - endPos));
	
	*inString = std::move(newString);
	return true;
}

/*
** Simplified regular expression replacement routine which replaces the
** first occurence of expr in inString with replaceWith.
** If expr is not found, does nothing and returns false.
*/
static int regexReplaceEx(std::string *inString, const char *expr, const char *replaceWith) {
	return stringReplaceEx(inString, expr, replaceWith, SEARCH_REGEX, -1);
}

/*
** Simplified case-sensisitive string replacement routine which
** replaces the first occurence of expr in inString with replaceWith.
** If expr is not found, does nothing and returns false.
*/
static int caseReplaceEx(std::string *inString, const char *expr, const char *replaceWith, int replaceLen) {
	return stringReplaceEx(inString, expr, replaceWith, SEARCH_CASE_SENSE, replaceLen);
}

/*
** Looks for a (case-sensitive literal) match of an old macro text in a
** temporary macro commands buffer. If the text is found, it is replaced by
** a substring of the default macros, bounded by a given start and end pattern
** (inclusive). Returns the length of the replacement.
*/
static int replaceMacroIfUnchanged(const char *oldText, const char *newStart, const char *newEnd) {
	
	std::string macroCmds = TempStringPrefs.macroCmds.toStdString();
	
	if (caseFind(macroCmds, oldText)) {
		const char *start = strstr(PrefDescrip[2].defaultString, newStart);

		if (start) {
			const char *end = strstr(start, newEnd);
			if (end) {
				int length = (int)(end - start) + strlen(newEnd);
				caseReplaceEx(&macroCmds, oldText, start, length);
				TempStringPrefs.macroCmds = QString::fromStdString(macroCmds);
				return length;
			}
		}
	}
	return 0;
}

/*
** Replace all '#' characters in shell commands by '##' to keep commands
** containing those working. '#' is a line number placeholder in 5.3 and
** had no special meaning before.
*/
static void updateShellCmdsTo5dot3(void) {

	if (TempStringPrefs.shellCmds.isNull())
		return;

	/* Count number of '#'. If there are '#' characters in the non-command
	** part of the definition we count too much and later allocate too much
	** memory for the new string, but this doesn't hurt.
	*/
	int nHash = 0;
	
	for(QChar ch: TempStringPrefs.shellCmds) {
		if (ch == QLatin1Char('#')) {
			nHash++;
		}
	}

	// No '#' -> no conversion necessary. 
	if (!nHash) {
		return;
	}

	std::string newString;
	newString.reserve(TempStringPrefs.shellCmds.size() + nHash);
	
	std::string shellCmds = TempStringPrefs.shellCmds.toStdString();

	char *cOld = &shellCmds[0]; // TODO(eteran): probably a better approach to this..
	auto cNew  = std::back_inserter(newString);
	bool isCmd = false;
	char *pCol = nullptr;
	char *pNL  = nullptr;

	/* Copy all characters from TempStringPrefs.shellCmds into newString
	** and duplicate '#' in command parts. A simple check for really beeing
	** inside a command part (starting with '\n', between the the two last
	** '\n' a colon ':' must have been found) is preformed.
	*/
	while (*cOld != '\0') {
		/* actually every 2nd line is a command. We additionally
		** check if there is a colon ':' in the previous line.
		*/
		if (*cOld == '\n') {
			if ((pCol > pNL) && !isCmd) {
				isCmd = true;
			} else {
				isCmd = false;
			}
			
			pNL = cOld;
		}

		if (!isCmd && *cOld == ':') {
			pCol = cOld;
		}

		// Duplicate hashes if we're in a command part 
		if (isCmd && *cOld == '#') {
			*cNew++ = '#';
		}

		// Copy every character 
		*cNew++ = *cOld++;
	}

	// exchange the string 
	TempStringPrefs.shellCmds = QString::fromStdString(newString);
}

static void updateShellCmdsTo5dot4(void) {
#ifdef __FreeBSD__
	const char *wc5dot3 = "^(\\s*)set wc=`wc`; echo \\$wc\\[1\\] \"words,\" \\$wc\\[2\\] \"lines,\" \\$wc\\[3\\] \"characters\"\\n";
	const char *wc5dot4 = "wc | awk '{print $2 \" lines, \" $1 \" words, \" $3 \" characters\"}'\n";
#else
	const char *wc5dot3 = "^(\\s*)set wc=`wc`; echo \\$wc\\[1\\] \"lines,\" \\$wc\\[2\\] \"words,\" \\$wc\\[3\\] \"characters\"\\n";
	const char *wc5dot4 = "wc | awk '{print $1 \" lines, \" $2 \" words, \" $3 \" characters\"}'\n";
#endif


	std::string shellCmds = TempStringPrefs.shellCmds.toStdString();

	if (regexFind(shellCmds, wc5dot3)) {
		regexReplaceEx(&shellCmds, wc5dot3, wc5dot4);
	}
	
	TempStringPrefs.shellCmds = QString::fromStdString(shellCmds);
}

static void updateMacroCmdsTo5dot5(void) {
	const char *uc5dot4 = "^(\\s*)if \\(substring\\(sel, keepEnd - 1, keepEnd == \" \"\\)\\)\\n";
	const char *uc5dot5 = "		if (substring(sel, keepEnd - 1, keepEnd) == \" \")\n";
	
	std::string macroCmds = TempStringPrefs.macroCmds.toStdString();
	
	if (regexFind(macroCmds, uc5dot4)) {
		regexReplaceEx(&macroCmds, uc5dot4, uc5dot5);
	}
	
	TempStringPrefs.macroCmds = QString::fromStdString(macroCmds);
}

static void updateMacroCmdsTo5dot6(void) {
	/*
	   This is ridiculous. Macros don't belong in the default preferences
	   string.
	   This code is also likely to break when the macro commands are upgraded
	   again in a next release, because it looks for patterns in the default
	   macro string (which may change).
	   Using a "Default" mechanism, like we do for highlighting patterns
	   would simplify upgrading A LOT in the future, but changing the way
	   default macros are stored, is a lot of work too, unfortunately.
	*/
	const char *pats[] = {"Complete Word:Alt+D::: {\n\
		# Tuning parameters\n\
		ScanDistance = 200\n\
		\n\
		# Search back to a word boundary to find the word to complete\n\
		startScan = max(0, $cursor - ScanDistance)\n\
		endScan = min($text_length, $cursor + ScanDistance)\n\
		scanString = get_range(startScan, endScan)\n\
		keyEnd = $cursor-startScan\n\
		keyStart = search_string(scanString, \"<\", keyEnd, \"backward\", \"regex\")\n\
		if (keyStart == -1)\n\
		    return\n\
		keyString = \"<\" substring(scanString, keyStart, keyEnd)\n\
		\n\
		# search both forward and backward from the cursor position.  Note that\n\
		# using a regex search can lead to incorrect results if any of the special\n\
		# regex characters is encountered, which is not considered a delimiter\n\
		backwardSearchResult = search_string(scanString, keyString, keyStart-1, \\\n\
		    	\"backward\", \"regex\")\n\
		forwardSearchResult = search_string(scanString, keyString, keyEnd, \"regex\")\n\
		if (backwardSearchResult == -1 && forwardSearchResult == -1) {\n\
		    beep()\n\
		    return\n\
		}\n\
		\n\
		# if only one direction matched, use that, otherwise use the nearest\n\
		if (backwardSearchResult == -1)\n\
		    matchStart = forwardSearchResult\n\
		else if (forwardSearchResult == -1)\n\
		    matchStart = backwardSearchResult\n\
		else {\n\
		    if (keyStart - backwardSearchResult <= forwardSearchResult - keyEnd)\n\
		    	matchStart = backwardSearchResult\n\
		    else\n\
		    	matchStart = forwardSearchResult\n\
		}\n\
		\n\
		# find the complete word\n\
		matchEnd = search_string(scanString, \">\", matchStart, \"regex\")\n\
		completedWord = substring(scanString, matchStart, matchEnd)\n\
		\n\
		# replace it in the window\n\
		replace_range(startScan + keyStart, $cursor, completedWord)\n\
	}",                                      "Complete Word:",           "\n\t}",         "Fill Sel. w/Char:::R: {\n\
		if ($selection_start == -1) {\n\
		    beep()\n\
		    return\n\
		}\n\
		\n\
		# Ask the user what character to fill with\n\
		fillChar = string_dialog(\"Fill selection with what character?\", \"OK\", \"Cancel\")\n\
		if ($string_dialog_button == 2 || $string_dialog_button == 0)\n\
		    return\n\
		\n\
		# Count the number of lines in the selection\n\
		nLines = 0\n\
		for (i=$selection_start; i<$selection_end; i++)\n\
		    if (get_character(i) == \"\\n\")\n\
		    	nLines++\n\
		\n\
		# Create the fill text\n\
		rectangular = $selection_left != -1\n\
		line = \"\"\n\
		fillText = \"\"\n\
		if (rectangular) {\n\
		    for (i=0; i<$selection_right-$selection_left; i++)\n\
			line = line fillChar\n\
		    for (i=0; i<nLines; i++)\n\
			fillText = fillText line \"\\n\"\n\
		    fillText = fillText line\n\
		} else {\n\
		    if (nLines == 0) {\n\
		    	for (i=$selection_start; i<$selection_end; i++)\n\
		    	    fillText = fillText fillChar\n\
		    } else {\n\
		    	startIndent = 0\n\
		    	for (i=$selection_start-1; i>=0 && get_character(i)!=\"\\n\"; i--)\n\
		    	    startIndent++\n\
		    	for (i=0; i<$wrap_margin-startIndent; i++)\n\
		    	    fillText = fillText fillChar\n\
		    	fillText = fillText \"\\n\"\n\
			for (i=0; i<$wrap_margin; i++)\n\
			    line = line fillChar\n\
			for (i=0; i<nLines-1; i++)\n\
			    fillText = fillText line \"\\n\"\n\
			for (i=$selection_end-1; i>=$selection_start && get_character(i)!=\"\\n\"; \\\n\
			    	i--)\n\
			    fillText = fillText fillChar\n\
		    }\n\
		}\n\
		\n\
		# Replace the selection with the fill text\n\
		replace_selection(fillText)\n\
	}",         "Fill Sel. w/Char:",         "\n\t}",
	                      "Comments>/* Uncomment */@C@C++@Java@CSS@JavaScript@Lex:::R: {\n\
		sel = get_selection()\n\
		selStart = $selection_start\n\
		selEnd = $selection_end\n\
		commentStart = search_string(sel, \"/*\", 0)\n\
		if (substring(sel, commentStart + 2, commentStart + 3) == \" \")\n\
		    keepStart = commentStart + 3\n\
		else\n\
		    keepStart = commentStart + 2\n\
		keepEnd = search_string(sel, \"*/\", length(sel), \"backward\")\n\
		commentEnd = keepEnd + 2\n\
		if (substring(sel, keepEnd - 1, keepEnd) == \" \")\n\
		    keepEnd = keepEnd - 1\n\
		replace_range(selStart + commentStart, selStart + commentEnd, \\\n\
			substring(sel, keepStart, keepEnd))\n\
		select(selStart, selEnd - (keepStart-commentStart) - \\\n\
			(commentEnd - keepEnd))\n\
	}", "Comments>/* Uncomment */", "\n\t}",         "Comments>Bar Uncomment@C:::R: {\n\
		selStart = $selection_start\n\
		selEnd = $selection_end\n\
		newText = get_range(selStart+3, selEnd-4)\n\
		newText = replace_in_string(newText, \"^ \\\\* \", \"\", \"regex\")\n\
		replace_range(selStart, selEnd, newText)\n\
		select(selStart, selStart + length(newText))\n\
	}", "Comments>Bar Uncomment@C:", "\n\t}",
	                      "Make C Prototypes@C@C++:::: {\n\
		if ($selection_start == -1) {\n\
		    start = 0\n\
		    end = $text_length\n\
		} else {\n\
		    start = $selection_start\n\
		    end = $selection_end\n\
		}\n\
		string = get_range(start, end)\n\
		nDefs = 0",                                 "Make C Prototypes@C@C++:", "\t\tnDefs = 0", nullptr};
	int i;
	for (i = 0; pats[i]; i += 3)
		replaceMacroIfUnchanged(pats[i], pats[i + 1], pats[i + 2]);
	return;
}

// Decref the default calltips file(s) for this window 
void UnloadLanguageModeTipsFile(Document *window) {
	int mode;

	mode = window->languageMode_;
	if (mode != PLAIN_LANGUAGE_MODE && LanguageModes[mode]->defTipsFile) {
		DeleteTagsFile(LanguageModes[mode]->defTipsFile, TIP, False);
	}
}

/*
 * Code for the dialog itself
 */
void ChooseColors(Document *window) {

	if(window->dialogColors_) {
		window->dialogColors_->show();
		window->dialogColors_->raise();
		return;
	}
	
	window->dialogColors_ = new DialogColors(window);
	window->dialogColors_->show();
}

/*
**  This function passes up a pointer to the static name of the default
**  shell, currently defined as the user's login shell.
**  In case of errors, the fallback of "sh" will be returned.
*/
static const char *getDefaultShell(void) {
	struct passwd *passwdEntry = nullptr;
	static char shellBuffer[MAXPATHLEN + 1] = "sh";

	passwdEntry = getpwuid(getuid()); //  getuid() never fails.  

	if (nullptr == passwdEntry) {
		//  Something bad happened! Do something, quick!  
		perror("nedit: Failed to get passwd entry (falling back to 'sh')");
		return "sh";
	}

	//  *passwdEntry may be overwritten  
	/*  TODO: To make this and other function calling getpwuid() more robust,
	    passwdEntry should be kept in a central position (Core->sysinfo?).
	    That way, local code would always get a current copy of passwdEntry,
	    but could still be kept lean.  The obvious alternative of a central
	    facility within NEdit to access passwdEntry would increase coupling
	    and would have to cover a lot of assumptions.  */
	strncpy(shellBuffer, passwdEntry->pw_shell, MAXPATHLEN);
	shellBuffer[MAXPATHLEN] = '\0';

	return shellBuffer;
}
