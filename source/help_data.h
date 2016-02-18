/*******************************************************************************
*                                                                              *
* help_data.h --  Nirvana Editor help module data                              *
*                                                                              *
*                 Generated on Jul 5, 2010 (Do NOT edit!)                      *
*                 Source of content from file help.etx                         *
*                                                                              *
* Copyright (c) 1999-2010 Mark Edel                                            *
*                                                                              *
* This is free software; you can redistribute it and/or modify it under the    *
* terms of the GNU General Public License as published by the Free Software    *
* Foundation; either version 2 of the License, or (at your option) any later   *
* version.                                                                     *
*                                                                              *
* This software is distributed in the hope that it will be useful, but WITHOUT *
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or        *
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License        *
* for more details.                                                            *
*                                                                              *
* In addition, as a special exception to the GNU GPL, the copyright holders    *
* give permission to link the code of this program with the Motif and Open     *
* Motif libraries (or with modified versions of these that use the same        *
* license), and distribute linked combinations including the two. You must     *
* obey the GNU General Public License in all respects for all of the code used *
* other than linking with Motif/Open Motif. If you modify this file, you may   *
* extend this exception to your version of the file, but you are not obligated *
* to do so. If you do not wish to do so, delete this exception statement from  *
* your version.                                                                *
*                                                                              *
* You should have received a copy of the GNU General Public License along with *
* software; if not, write to the Free Software Foundation, Inc., 59 Temple     *
* Place, Suite 330, Boston, MA  02111-1307 USA                                 *
*                                                                              *
* Nirvana Text Editor                                                          *
* September 10, 1991                                                           *
*                                                                              *
* Written by Mark Edel                                                         *
*                                                                              *
*******************************************************************************/

const char *HelpTitles[] = {"Getting Started", "Selecting Text",                  "Finding and Replacing Text", "Cut and Paste",                "Using the Mouse",          "Keyboard Shortcuts",          "Shifting and Filling",
                            "Tabbed Editing",  "File Format",                     "Programming with NEdit",     "Tab Stops/Emulated Tab Stops", "Auto/Smart Indent",        "Syntax Highlighting",         "Finding Declarations (ctags)",
                            "Calltips",        "Basic Regular Expression Syntax", "Metacharacters",             "Parenthetical Constructs",     "Advanced Topics",          "Example Regular Expressions", "Shell Commands and Filters",
                            "Learn/Replay",    "Macro Language",                  "Macro Subroutines",          "Rangesets",                    "Highlighting Information", "Action Routines",             "Customizing NEdit",
                            "Preferences",     "X Resources",                     "Key Binding",                "Highlighting Patterns",        "Smart Indent Macros",      "NEdit Command Line",          "Client/Server Mode",
                            "Crash Recovery",  "Version",                         "GNU General Public License", "Mailing Lists",                "Problems/Defects",         "Tabs Dialog",                 "Customize Window Title Dialog",
                            NULL};



HelpMenu H_M[] = {{&H_M[1], 1, HELP_START, "start", 0, 'G', NULL},
                  {&H_M[2], 1, HELP_none, "basicOp", 0, 'B', "Basic Operation"},
                  {&H_M[3], 2, HELP_SELECT, "select", 0, 'S', NULL},
                  {&H_M[4], 2, HELP_SEARCH, "search", 0, 'F', NULL},
                  {&H_M[5], 2, HELP_CLIPBOARD, "clipboard", 0, 'C', NULL},
                  {&H_M[6], 2, HELP_MOUSE, "mouse", 0, 'U', NULL},
                  {&H_M[7], 2, HELP_KEYBOARD, "keyboard", 0, 'K', NULL},
                  {&H_M[8], 2, HELP_FILL, "fill", 0, 'h', NULL},
                  {&H_M[9], 2, HELP_INTERFACE, "interface", 0, 'T', NULL},
                  {&H_M[10], 2, HELP_FORMAT, "format", 0, 'i', NULL},
                  {&H_M[11], 1, HELP_none, "features", 0, 'F', "Features for Programming"},
                  {&H_M[12], 2, HELP_PROGRAMMER, "programmer", 0, 'P', NULL},
                  {&H_M[13], 2, HELP_TABS, "tabs", 0, 'T', NULL},
                  {&H_M[14], 2, HELP_INDENT, "indent", 0, 'A', NULL},
                  {&H_M[15], 2, HELP_SYNTAX, "syntax", 0, 'S', NULL},
                  {&H_M[16], 2, HELP_TAGS, "tags", 0, 'F', NULL},
                  {&H_M[17], 2, HELP_CALLTIPS, "calltips", 0, 'C', NULL},
                  {&H_M[18], 1, HELP_none, "regex", 0, 'R', "Regular Expressions"},
                  {&H_M[19], 2, HELP_BASICSYNTAX, "basicSyntax", 0, 'B', NULL},
                  {&H_M[20], 2, HELP_ESCAPESEQUENCES, "escapeSequences", 0, 'M', NULL},
                  {&H_M[21], 2, HELP_PARENCONSTRUCTS, "parenConstructs", 0, 'P', NULL},
                  {&H_M[22], 2, HELP_ADVANCEDTOPICS, "advancedTopics", 0, 'A', NULL},
                  {&H_M[23], 2, HELP_EXAMPLES, "examples", 0, 'E', NULL},
                  {&H_M[24], 1, HELP_none, "extensions", 0, 'M', "Macro/Shell Extensions"},
                  {&H_M[25], 2, HELP_SHELL, "shell", 1, 'S', NULL},
                  {&H_M[26], 2, HELP_LEARN, "learn", 0, 'L', NULL},
                  {&H_M[27], 2, HELP_MACRO_LANG, "macro_lang", 0, 'M', NULL},
                  {&H_M[28], 2, HELP_MACRO_SUBRS, "macro_subrs", 0, 'a', NULL},
                  {&H_M[29], 2, HELP_RANGESET, "rangeset", 0, 'R', NULL},
                  {&H_M[30], 2, HELP_HILITEINFO, "hiliteInfo", 0, 'H', NULL},
                  {&H_M[31], 2, HELP_ACTIONS, "actions", 0, 'A', NULL},
                  {&H_M[32], 1, HELP_none, "customizing", 0, 'C', "Customizing"},
                  {&H_M[33], 2, HELP_CUSTOMIZE, "customize", 0, 'C', NULL},
                  {&H_M[34], 2, HELP_PREFERENCES, "preferences", 0, 'P', NULL},
                  {&H_M[35], 2, HELP_RESOURCES, "resources", 0, 'X', NULL},
                  {&H_M[36], 2, HELP_BINDING, "binding", 0, 'K', NULL},
                  {&H_M[37], 2, HELP_PATTERNS, "patterns", 0, 'H', NULL},
                  {&H_M[38], 2, HELP_SMART_INDENT, "smart_indent", 0, 'S', NULL},
                  {&H_M[39], 1, HELP_COMMAND_LINE, "command_line", 0, 'N', NULL},
                  {&H_M[40], 1, HELP_SERVER, "server", 0, 'C', NULL},
                  {&H_M[41], 1, HELP_RECOVERY, "recovery", 0, 'a', NULL},
                  {&H_M[42], 1, HELP_none, "separator1", 0, '-', NULL},
                  {&H_M[43], 1, HELP_VERSION, "version", 0, 'V', NULL},
                  {&H_M[44], 1, HELP_DISTRIBUTION, "distribution", 0, 'G', NULL},
                  {&H_M[45], 1, HELP_MAILING_LIST, "mailing_list", 0, 'L', NULL},
                  {&H_M[46], 1, HELP_DEFECTS, "defects", 0, 'P', NULL},
                  {&H_M[47], 1, HELP_TABS_DIALOG, "tabs_dialog", 9, 'T', NULL},
                  {NULL, 1, HELP_CUSTOM_TITLE_DIALOG, "custom_title_dialog", 9, 'C', NULL}};

Href H_R[] = {{&H_R[1], 54, HELP_TAGS, "ctags support"}, {&H_R[2], 5902, HELP_BASICSYNTAX, "Alternation"}, {NULL, 14799, HELP_PREFERENCES, "Autoload Files"}};

static const char *NEditVersion = "NEdit 5.6\nJul 5, 2010\n";
