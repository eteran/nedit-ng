
#include "Settings.h"
#include "IndentStyle.h"
#include "SearchType.h"
#include "ShowMatchingStyle.h"
#include "WrapStyle.h"
#include "search.h" // For ReplaceScope enum
#include "nedit.h"
#include <QSettings>
#include <QStandardPaths>

// Some default colors
#define NEDIT_DEFAULT_FG        "#221f1e"
#define NEDIT_DEFAULT_TEXT_BG   "#d6d2d0"
#define NEDIT_DEFAULT_SEL_FG    "#ffffff"
#define NEDIT_DEFAULT_SEL_BG    "#43ace8"
#define NEDIT_DEFAULT_HI_FG     "white"        /* These are colors for flashing */
#define NEDIT_DEFAULT_HI_BG     "red"          /* matching parens. */
#define NEDIT_DEFAULT_LINENO_FG "black"
#define NEDIT_DEFAULT_CURSOR_FG "black"


// New styles added in 5.2 for auto-upgrade
#define ADD_5_2_STYLES " Pointer:#660000:Bold\nRegex:#009944:Bold\nWarning:brown2:Italic"

/**
 * @brief Settings::configFile
 * @return
 */
QString Settings::configFile() {
    QString configDir  = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString configFile = QString(QLatin1String("%1/%2/%3")).arg(configDir, QLatin1String("nedit-ng"), QLatin1String("config.ini"));
    return configFile;
}

/**
 * @brief Settings::historyFile
 * @return
 */
QString Settings::historyFile() {
    QString configDir  = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString configFile = QString(QLatin1String("%1/%2/%3")).arg(configDir, QLatin1String("nedit-ng"), QLatin1String("history"));
    return configFile;
}

/**
 * @brief Settings::autoLoadMacroFile
 * @return
 */
QString Settings::autoLoadMacroFile() {
    QString configDir  = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString configFile = QString(QLatin1String("%1/%2/%3")).arg(configDir, QLatin1String("nedit-ng"), QLatin1String("autoload.nm"));
    return configFile;
}

/**
 * @brief Settings::styleFile
 * @return
 */
QString Settings::styleFile() {
    QString configDir  = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QString configFile = QString(QLatin1String("%1/%2/%3")).arg(configDir, QLatin1String("nedit-ng"), QLatin1String("style.qss"));
    return configFile;
}




//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void Settings::loadPreferences() {

    QString filename = configFile();
    QSettings settings(filename, QSettings::IniFormat);

    fileVersion   = settings.value(tr("nedit.fileVersion"),   QLatin1String("1.0")).toString();
#if defined(Q_OS_LINUX)
    shellCommands = settings.value(tr("nedit.shellCommands"), QLatin1String("&spell:Alt+B:EX:\n    cat>spellTmp; xterm -e ispell -x spellTmp; cat spellTmp; rm spellTmp\n    &wc::ED:\nwc | awk '{print $1 \" lines, \" $2 \" words, \" $3 \" characters\"}'\n    s&ort::EX:\nsort\n&number lines::AW:\nnl -ba\n&make:Alt+Z:W:\nmake\n    ex&pand::EX:\nexpand\n&unexpand::EX:\nunexpand\n")).toString();
#elif defined(Q_OS_FREEBSD)
    shellCommands = settings.value(tr("nedit.shellCommands"), QLatin1String("&spell:Alt+B:EX:\n    cat>spellTmp; xterm -e ispell -x spellTmp; cat spellTmp; rm spellTmp\n    &wc::ED:\nwc | awk '{print $2 \" lines, \" $1 \" words, \" $3 \" characters\"}'\n    s&ort::EX:\nsort\n&number lines::AW:\npr -tn\n&make:Alt+Z:W:\nmake\n    ex&pand::EX:\nexpand\n&unexpand::EX:\nunexpand\n")).toString();
#else
    shellCommands = settings.value(tr("nedit.shellCommands"), QLatin1String("&spell:Alt+B:ED:\n    (cat;echo \"\") | spell\n&wc::ED:\nwc | awk '{print $1 \" lines, \" $2 \" words, \" $3 \" characters\"}'\n    \ns&ort::EX:\nsort\n&number lines::AW:\nnl -ba\n&make:Alt+Z:W:\nmake\n    ex&pand::EX:\nexpand\n&unexpand::EX:\nunexpand\n")).toString();
#endif
    macroCommands         = settings.value(tr("nedit.macroCommands"),         QLatin1String("Complete Word:Alt+D:: {\n		# This macro attempts to complete the current word by\n		# finding another word in the same document that has\n		# the same prefix; repeated invocations of the macro\n		# (by repeated typing of its accelerator, say) cycles\n		# through the alternatives found.\n		# \n		# Make sure $compWord contains something (a dummy index)\n		$compWord[\"\"] = \"\"\n		\n		# Test whether the rest of $compWord has been initialized:\n		# this avoids having to initialize the global variable\n		# $compWord in an external macro file\n		if (!(\"wordEnd\" in $compWord)) {\n		    # we need to initialize it\n		    $compWord[\"wordEnd\"] = 0\n		    $compWord[\"repeat\"] = 0\n		    $compWord[\"init\"] = 0\n		    $compWord[\"wordStart\"] = 0\n		}\n		\n		if ($compWord[\"wordEnd\"] == $cursor) {\n		        $compWord[\"repeat\"] += 1\n		}\n		else {\n		   $compWord[\"repeat\"] = 1\n		   $compWord[\"init\"] = $cursor\n		\n		   # search back to a word boundary to find the word to complete\n		   # (we use \\w here to allow for programming \"words\" that can include\n		   # digits and underscores; use \\l for letters only)\n		   $compWord[\"wordStart\"] = search(\"<\\\\w+\", $cursor, \"backward\", \"regex\", \"wrap\")\n		\n		   if ($compWord[\"wordStart\"] == -1)\n		      return\n		\n		    if ($search_end == $cursor)\n		       $compWord[\"word\"] = get_range($compWord[\"wordStart\"], $cursor)\n		    else\n		        return\n		}\n		s = $cursor\n		for (i=0; i <= $compWord[\"repeat\"]; i++)\n		    s = search($compWord[\"word\"], s - 1, \"backward\", \"regex\", \"wrap\")\n		\n		if (s == $compWord[\"wordStart\"]) {\n		   beep()\n		   $compWord[\"repeat\"] = 0\n		   s = $compWord[\"wordStart\"]\n		   se = $compWord[\"init\"]\n		}\n		else\n		   se = search(\">\", s, \"regex\")\n		\n		replace_range($compWord[\"wordStart\"], $cursor, get_range(s, se))\n		\n		$compWord[\"wordEnd\"] = $cursor\n	}\n	Fill Sel. w/Char::R: {\n		# This macro replaces each character position in\n		# the selection with the string typed into the dialog\n		# it displays.\n		if ($selection_start == -1) {\n		    beep()\n		    return\n		}\n		\n		# Ask the user what character to fill with\n		fillChar = string_dialog(\"Fill selection with what character?\", \\\n		                         \"OK\", \"Cancel\")\n		if ($string_dialog_button == 2 || $string_dialog_button == 0)\n		    return\n		\n		# Count the number of lines (NL characters) in the selection\n		# (by removing all non-NLs in selection and counting the remainder)\n		nLines = length(replace_in_string(get_selection(), \\\n		                                  \"^.*$\", \"\", \"regex\"))\n		\n		rectangular = $selection_left != -1\n		\n		# work out the pieces of required of the replacement text\n		# this will be top mid bot where top is empty or ends in NL,\n		# mid is 0 or more lines of repeats ending with NL, and\n		# bot is 0 or more repeats of the fillChar\n		\n		toplen = -1 # top piece by default empty (no NL)\n		midlen = 0\n		botlen = 0\n		\n		if (rectangular) {\n		    # just fill the rectangle:  mid\\n \\ nLines\n		    #                           mid\\n /\n		    #                           bot   - last line with no nl\n		    midlen = $selection_right -  $selection_left\n		    botlen = $selection_right -  $selection_left\n		} else {\n		    #                  |col[0]\n		    #         .........toptoptop\\n                      |col[0]\n		    # either  midmidmidmidmidmid\\n \\ nLines - 1   or ...botbot...\n		    #         midmidmidmidmidmid\\n /                          |col[1]\n		    #         botbot...         |\n		    #                 |col[1]   |wrap margin\n		    # we need column positions col[0], col[1] of selection start and\n		    # end (use a loop and arrays to do the two positions)\n		    sel[0] = $selection_start\n		    sel[1] = $selection_end\n		\n		    # col[0] = pos_to_column($selection_start)\n		    # col[1] = pos_to_column($selection_end)\n		\n		    for (i = 0; i < 2; ++i) {\n		        end = sel[i]\n		        pos = search(\"^\", end, \"regex\", \"backward\")\n		        thisCol = 0\n		        while (pos < end) {\n		            nexttab = search(\"\\t\", pos)\n		            if (nexttab < 0 || nexttab >= end) {\n		                thisCol += end - pos # count remaining non-tabs\n		                nexttab = end\n		            } else {\n		                thisCol += nexttab - pos + $tab_dist\n		                thisCol -= (thisCol % $tab_dist)\n		            }\n		            pos = nexttab + 1 # skip past the tab or end\n		        }\n		        col[i] = thisCol\n		    }\n		    toplen = max($wrap_margin - col[0], 0)\n		    botlen = min(col[1], $wrap_margin)\n		\n		    if (nLines == 0) {\n		        toplen = -1\n		        botlen = max(botlen - col[0], 0)\n		    } else {\n		        midlen = $wrap_margin\n		        if (toplen < 0)\n		            toplen = 0\n		        nLines-- # top piece will end in a NL\n		    }\n		}\n		\n		# Create the fill text\n		# which is the longest piece? make a line of that length\n		# (use string doubling - this allows the piece to be\n		# appended to double in size at each iteration)\n		\n		len = max(toplen, midlen, botlen)\n		charlen = length(fillChar) # maybe more than one char given!\n		\n		line = \"\"\n		while (len > 0) {\n		    if (len % 2)\n		        line = line fillChar\n		    len /= 2\n		    if (len > 0)\n		        fillChar = fillChar fillChar\n		}\n		# assemble our pieces\n		toppiece = \"\"\n		midpiece = \"\"\n		botpiece = \"\"\n		if (toplen >= 0)\n		    toppiece = substring(line, 0, toplen * charlen) \"\\n\"\n		if (botlen > 0)\n		    botpiece = substring(line, 0, botlen * charlen)\n		\n		# assemble midpiece (use doubling again)\n		line = substring(line, 0, midlen * charlen) \"\\n\"\n		while (nLines > 0) {\n		    if (nLines % 2)\n		        midpiece = midpiece line\n		    nLines /= 2\n		    if (nLines > 0)\n		        line = line line\n		}\n		# Replace the selection with the complete fill text\n		replace_selection(toppiece midpiece botpiece)\n	}\n	Quote Mail Reply::: {\n		if ($selection_start == -1)\n		    replace_all(\"^.*$\", \"\\\\> &\", \"regex\")\n		else\n		    replace_in_selection(\"^.*$\", \"\\\\> &\", \"regex\")\n	}\n	Unquote Mail Reply::: {\n		if ($selection_start == -1)\n		    replace_all(\"(^\\\\> )(.*)$\", \"\\\\2\", \"regex\")\n		else\n		    replace_in_selection(\"(^\\\\> )(.*)$\", \"\\\\2\", \"regex\")\n	}\n	Comments>/* Comment */@C@C++@Java@CSS@JavaScript@Lex::R: {\n		selStart = $selection_start\n		selEnd = $selection_end\n		replace_range(selStart, selEnd, \"/* \" get_selection() \" */\")\n		select(selStart, selEnd + 6)\n	}\n	Comments>/* Uncomment */@C@C++@Java@CSS@JavaScript@Lex::R: {\n		pos = search(\"(?n\\\\s*/\\\\*\\\\s*)\", $selection_start, \"regex\")\n		start = $search_end\n		end = search(\"(?n\\\\*/\\\\s*)\", $selection_end, \"regex\", \"backward\")\n		if (pos != $selection_start || end == -1 )\n		    return\n		replace_selection(get_range(start, end))\n		select(pos, $cursor)\n	}\n	Comments>// Comment@C@C++@Java@JavaScript::R: {\n		replace_in_selection(\"^.*$\", \"// &\", \"regex\")\n	}\n	Comments>// Uncomment@C@C++@Java@JavaScript::R: {\n		replace_in_selection(\"(^[ \\\\t]*// ?)(.*)$\", \"\\\\2\", \"regex\")\n	}\n	Comments># Comment@Perl@Sh Ksh Bash@NEdit Macro@Makefile@Awk@Csh@Python@Tcl::R: {\n		replace_in_selection(\"^.*$\", \"#&\", \"regex\")\n	}\n	Comments># Uncomment@Perl@Sh Ksh Bash@NEdit Macro@Makefile@Awk@Csh@Python@Tcl::R: {\n		replace_in_selection(\"(^[ \\\\t]*#)(.*)$\", \"\\\\2\", \"regex\")\n	}\n	Comments>-- Comment@SQL::R: {\n		replace_in_selection(\"^.*$\", \"--&\", \"regex\")\n	}\n	Comments>-- Uncomment@SQL::R: {\n		replace_in_selection(\"(^[ \\\\t]*--)(.*)$\", \"\\\\2\", \"regex\")\n	}\n	Comments>! Comment@X Resources::R: {\n		replace_in_selection(\"^.*$\", \"!&\", \"regex\")\n	}\n	Comments>! Uncomment@X Resources::R: {\n		replace_in_selection(\"(^[ \\\\t]*!)(.*)$\", \"\\\\2\", \"regex\")\n	}\n	Comments>% Comment@LaTeX::R: {\n		replace_in_selection(\"^.*$\", \"%&\", \"regex\")\n		}\n	Comments>% Uncomment@LaTeX::R: {\n		replace_in_selection(\"(^[ \\\\t]*%)(.*)$\", \"\\\\2\", \"regex\")\n		}\n	Comments>Bar Comment@C::R: {\n		if ($selection_left != -1) {\n		    dialog(\"Selection must not be rectangular\")\n		    return\n		}\n		start = $selection_start\n		end = $selection_end-1\n		origText = get_range($selection_start, $selection_end-1)\n		newText = \"/*\\n\" replace_in_string(get_range(start, end), \\\n		    \"^\", \" * \", \"regex\") \"\\n */\\n\"\n		replace_selection(newText)\n		select(start, start + length(newText))\n	}\n	Comments>Bar Uncomment@C::R: {\n		selStart = $selection_start\n		selEnd = $selection_end\n		pos = search(\"/\\\\*\\\\s*\\\\n\", selStart, \"regex\")\n		if (pos != selStart) return\n		start = $search_end\n		end = search(\"\\\\n\\\\s*\\\\*/\\\\s*\\\\n?\", selEnd, \"regex\", \"backward\")\n		if (end == -1 || $search_end < selEnd) return\n		newText = get_range(start, end)\n		newText = replace_in_string(newText,\"^ *\\\\* ?\", \"\", \"regex\", \"copy\")\n		if (get_range(selEnd, selEnd - 1) == \"\\n\") selEnd -= 1\n		replace_range(selStart, selEnd, newText)\n		select(selStart, selStart + length(newText))\n	}\n	Make C Prototypes@C@C++::: {\n		# simplistic extraction of C function prototypes, usually good enough\n		if ($selection_start == -1) {\n		    start = 0\n		    end = $text_length\n		} else {\n		    start = $selection_start\n		    end = $selection_end\n		}\n		string = get_range(start, end)\n		# remove all C++ and C comments, then all blank lines in the extracted range\n		string = replace_in_string(string, \"//.*$\", \"\", \"regex\", \"copy\")\n		string = replace_in_string(string, \"(?n/\\\\*.*?\\\\*/)\", \"\", \"regex\", \"copy\")\n		string = replace_in_string(string, \"^\\\\s*\\n\", \"\", \"regex\", \"copy\")\n		nDefs = 0\n		searchPos = 0\n		prototypes = \"\"\n		staticPrototypes = \"\"\n		for (;;) {\n		    headerStart = search_string(string, \\\n		        \"^[a-zA-Z]([^;#\\\"'{}=><!/]|\\n)*\\\\)[ \\t]*\\n?[ \\t]*\\\\{\", \\\n		        searchPos, \"regex\")\n		    if (headerStart == -1)\n		        break\n		    headerEnd = search_string(string, \")\", $search_end,\"backward\") + 1\n		    prototype = substring(string, headerStart, headerEnd) \";\\n\"\n		    if (substring(string, headerStart, headerStart+6) == \"static\")\n		        staticPrototypes = staticPrototypes prototype\n		    else\n		        prototypes = prototypes prototype\n		    searchPos = headerEnd\n		    nDefs++\n		}\n		if (nDefs == 0) {\n		    dialog(\"No function declarations found\")\n		    return\n		}\n		new()\n		focus_window(\"last\")\n		replace_range(0, 0, prototypes staticPrototypes)\n	}")).toString();
    bgMenuCommands        = settings.value(tr("nedit.bgMenuCommands"),        QLatin1String("Undo::: {\nundo()\n}\n	Redo::: {\nredo()\n}\n	Cut::R: {\ncut_clipboard()\n}\n	Copy::R: {\ncopy_clipboard()\n}\n	Paste::: {\npaste_clipboard()\n}")).toString();
    highlightPatterns     = settings.value(tr("nedit.highlightPatterns"),     QLatin1String("Ada:Default\n        Awk:Default\n        C++:Default\n        C:Default\n        CSS:Default\n        Csh:Default\n        Fortran:Default\n        Java:Default\n        JavaScript:Default\n        LaTeX:Default\n        Lex:Default\n        Makefile:Default\n        Matlab:Default\n        NEdit Macro:Default\n        Pascal:Default\n        Perl:Default\n        PostScript:Default\n        Python:Default\n        Regex:Default\n        SGML HTML:Default\n        SQL:Default\n        Sh Ksh Bash:Default\n        Tcl:Default\n        VHDL:Default\n        Verilog:Default\n        XML:Default\n        X Resources:Default\n        Yacc:Default")).toString();
    languageModes         = settings.value(tr("nedit.languageModes"),         QLatin1String("Ada:.ada .ad .ads .adb .a:::::::\n        Awk:.awk:::::::\n        C++:.cc .hh .C .H .i .cxx .hxx .cpp .c++::::::\".,/\\`'!|@#%^&*()-=+{}[]\"\":;<>?~\":\n        C:.c .h::::::\".,/\\`'!|@#%^&*()-=+{}[]\"\":;<>?~\":\n        CSS:css::Auto:None:::\".,/\\`'!|@#%^&*()=+{}[]\"\":;<>?~\":\n        Csh:.csh .cshrc .tcshrc .login .logout:\"^[ \\t]*#[ \\t]*![ \\t]*/bin/t?csh\"::::::\n        Fortran:.f .f77 .for:::::::\n        Java:.java:::::::\n        JavaScript:.js:::::::\n        LaTeX:.tex .sty .cls .ltx .ins .clo .fd:::::::\n        Lex:.lex:::::::\n        Makefile:Makefile makefile .gmk:::None:8:8::\n        Matlab:.m .oct .sci:::::::\n        NEdit Macro:.nm .neditmacro:::::::\n        Pascal:.pas .p .int:::::::\n        Perl:.pl .pm .p5 .PL:\"^[ \\t]*#[ \\t]*!.*perl\":Auto:None:::\".,/\\\\`'!$@#%^&*()-=+{}[]\"\":;<>?~|\":\n        PostScript:.ps .eps .epsf .epsi:\"^%!\":::::\"/%(){}[]<>\":\n        Python:.py:\"^#!.*python\":Auto:None:::\"!\"\"#$%&'()*+,-./:;<=>?@[\\\\]^`{|}~\":\n        Regex:.reg .regex:\"\\(\\?[:#=!iInN].+\\)\":None:Continuous::::\n        SGML HTML:.sgml .sgm .html .htm:\"\\<[Hh][Tt][Mm][Ll]\\>\"::::::\n        SQL:.sql:::::::\n        Sh Ksh Bash:.sh .bash .ksh .profile .bashrc .bash_logout .bash_login .bash_profile:\"^[ \\t]*#[ \\t]*![ \\t]*/.*bin/(bash|ksh|sh|zsh)\"::::::\n        Tcl:.tcl .tk .itcl .itk::Smart:None::::\n        VHDL:.vhd .vhdl .vdl:::::::\n        Verilog:.v:::::::\n        XML:.xml .xsl .dtd:\"\\<(?i\\?xml|!doctype)\"::None:::\"<>/=\"\"'()+*?|\":\n        X Resources:.Xresources .Xdefaults .nedit .pats nedit.rc:\"^[!#].*([Aa]pp|[Xx]).*[Dd]efaults\"::::::\n        Yacc:.y::::::\".,/\\`'!|@#%^&*()-=+{}[]\"\":;<>?~\":")).toString();
    styles                = settings.value(tr("nedit.styles"),                QLatin1String("Plain:black:Plain\n    	Comment:gray20:Italic\n    	Keyword:black:Bold\n        Operator:dark blue:Bold\n        Bracket:dark blue:Bold\n    	Storage Type:brown:Bold\n    	Storage Type1:saddle brown:Bold\n    	String:darkGreen:Plain\n    	String1:SeaGreen:Plain\n    	String2:darkGreen:Bold\n    	Preprocessor:RoyalBlue4:Plain\n    	Preprocessor1:blue:Plain\n    	Character Const:darkGreen:Plain\n    	Numeric Const:darkGreen:Plain\n    	Identifier:brown:Plain\n    	Identifier1:RoyalBlue4:Plain\n        Identifier2:SteelBlue:Plain\n 	Subroutine:brown:Plain\n	Subroutine1:chocolate:Plain\n   	Ada Attributes:plum:Bold\n	Label:red:Italic\n	Flag:red:Bold\n    	Text Comment:SteelBlue4:Italic\n    	Text Key:VioletRed4:Bold\n	Text Key1:VioletRed4:Plain\n    	Text Arg:RoyalBlue4:Bold\n    	Text Arg1:SteelBlue4:Bold\n	Text Arg2:RoyalBlue4:Plain\n    	Text Escape:gray30:Bold\n	LaTeX Math:darkGreen:Plain\n" ADD_5_2_STYLES)).toString();
    smartIndentInit       = settings.value(tr("nedit.smartIndentInit"),       QLatin1String("C:Default\n	C++:Default\n	Python:Default\n	Matlab:Default")).toString();
    smartIndentInitCommon = settings.value(tr("nedit.smartIndentInitCommon"), QLatin1String("Default")).toString();
    autoWrap              = static_cast<WrapStyle>(settings.value(tr("nedit.autoWrap"),              CONTINUOUS_WRAP).toInt());
    wrapMargin            = settings.value(tr("nedit.wrapMargin"),            0).toInt();
    autoIndent            = settings.value(tr("nedit.autoIndent"),            AUTO_INDENT).toInt();
    autoSave              = settings.value(tr("nedit.autoSave"),              true).toBool();
    openInTab             = settings.value(tr("nedit.openInTab"),             true).toBool();
    saveOldVersion        = settings.value(tr("nedit.saveOldVersion"),        false).toBool();
    showMatching          = settings.value(tr("nedit.showMatching"),          FLASH_DELIMIT).toInt();
    matchSyntaxBased      = settings.value(tr("nedit.matchSyntaxBased"),      true).toBool();
    highlightSyntax       = settings.value(tr("nedit.highlightSyntax"),       true).toBool();
    backlightChars        = settings.value(tr("nedit.backlightChars"),        false).toBool();
    backlightCharTypes    = settings.value(tr("nedit.backlightCharTypes"),    QLatin1String("0-8,10-31,127:red;9:#dedede;32,160-255:#f0f0f0;128-159:orange")).toString();
    searchDialogs         = settings.value(tr("nedit.searchDialogs"),         false).toBool();
    beepOnSearchWrap      = settings.value(tr("nedit.beepOnSearchWrap"),      false).toBool();
    retainSearchDialogs   = settings.value(tr("nedit.retainSearchDialogs"),   false).toBool();
    searchWraps           = settings.value(tr("nedit.searchWraps"),           true).toBool();
    stickyCaseSenseButton = settings.value(tr("nedit.stickyCaseSenseButton"), true).toBool();
    repositionDialogs     = settings.value(tr("nedit.repositionDialogs"),     false).toBool();
    autoScroll            = settings.value(tr("nedit.autoScroll"),            false).toBool();
    autoScrollVPadding    = settings.value(tr("nedit.autoScrollVPadding"),    4).toInt();
    appendLF              = settings.value(tr("nedit.appendLF"),              true).toBool();
    sortOpenPrevMenu      = settings.value(tr("nedit.sortOpenPrevMenu"),      true).toBool();
    statisticsLine        = settings.value(tr("nedit.statisticsLine"),        false).toBool();
    iSearchLine           = settings.value(tr("nedit.iSearchLine"),           false).toBool();
    sortTabs              = settings.value(tr("nedit.sortTabs"),              false).toBool();
    tabBar                = settings.value(tr("nedit.tabBar"),                true).toBool();
    tabBarHideOne         = settings.value(tr("nedit.tabBarHideOne"),         true).toBool();
    toolTips              = settings.value(tr("nedit.toolTips"),              true).toBool();
    globalTabNavigate     = settings.value(tr("nedit.globalTabNavigate"),     false).toBool();
    lineNumbers           = settings.value(tr("nedit.lineNumbers"),           false).toBool();
    pathInWindowsMenu     = settings.value(tr("nedit.pathInWindowsMenu"),     true).toBool();
    warnFileMods          = settings.value(tr("nedit.warnFileMods"),          true).toBool();
    warnRealFileMods      = settings.value(tr("nedit.warnRealFileMods"),      true).toBool();
    warnExit              = settings.value(tr("nedit.warnExit"),              true).toBool();
    searchMethod          = settings.value(tr("nedit.searchMethod"),          SEARCH_LITERAL).toInt();
#if defined(REPLACE_SCOPE)
    replaceDefaultScope   = settings.value(tr("nedit.replaceDefaultScope"),   REPL_DEF_SCOPE_SMART).toInt();
#endif

    textRows                          = settings.value(tr("nedit.textRows"),					      24).toInt();
    textCols                          = settings.value(tr("nedit.textCols"),					      80).toInt();
    tabDistance                       = settings.value(tr("nedit.tabDistance"), 				      8).toInt();
    emulateTabs                       = settings.value(tr("nedit.emulateTabs"), 				      0).toInt();
    insertTabs                        = settings.value(tr("nedit.insertTabs"),  				      true).toBool();
    textFont                          = settings.value(tr("nedit.textFont"),					      QLatin1String("Courier New,10,-1,5,50,0,0,0,0,0")).toString();
    boldHighlightFont                 = settings.value(tr("nedit.boldHighlightFont"),			      QLatin1String("Courier New,10,-1,5,75,0,0,0,0,0")).toString();
    italicHighlightFont               = settings.value(tr("nedit.italicHighlightFont"), 		      QLatin1String("Courier New,10,-1,5,50,1,0,0,0,0")).toString();
    boldItalicHighlightFont           = settings.value(tr("nedit.boldItalicHighlightFont"), 	      QLatin1String("Courier New,10,-1,5,75,1,0,0,0,0")).toString();

    colors[TEXT_FG_COLOR]                       = settings.value(tr("nedit.textFgColor"), 				      QLatin1String(NEDIT_DEFAULT_FG)).toString();
    colors[TEXT_BG_COLOR]                       = settings.value(tr("nedit.textBgColor"), 				      QLatin1String(NEDIT_DEFAULT_TEXT_BG)).toString();
    colors[SELECT_FG_COLOR]                     = settings.value(tr("nedit.selectFgColor"),				      QLatin1String(NEDIT_DEFAULT_SEL_FG)).toString();
    colors[SELECT_BG_COLOR]                     = settings.value(tr("nedit.selectBgColor"),				      QLatin1String(NEDIT_DEFAULT_SEL_BG)).toString();
    colors[HILITE_FG_COLOR]                     = settings.value(tr("nedit.hiliteFgColor"),				      QLatin1String(NEDIT_DEFAULT_HI_FG)).toString();
    colors[HILITE_BG_COLOR]                     = settings.value(tr("nedit.hiliteBgColor"),				      QLatin1String(NEDIT_DEFAULT_HI_BG)).toString();
    colors[LINENO_FG_COLOR]                     = settings.value(tr("nedit.lineNoFgColor"),				      QLatin1String(NEDIT_DEFAULT_LINENO_FG)).toString();
    colors[CURSOR_FG_COLOR]                     = settings.value(tr("nedit.cursorFgColor"),				      QLatin1String(NEDIT_DEFAULT_CURSOR_FG)).toString();

    tooltipBgColor                    = settings.value(tr("nedit.tooltipBgColor"),  			      QLatin1String("LemonChiffon1")).toString();
    shell                             = settings.value(tr("nedit.shell"),						      QLatin1String("DEFAULT")).toString();
    geometry                          = settings.value(tr("nedit.geometry"),					      QLatin1String("")).toString();
    remapDeleteKey                    = settings.value(tr("nedit.remapDeleteKey"),  			      false).toBool();
    stdOpenDialog                     = settings.value(tr("nedit.stdOpenDialog"),				      false).toBool();
    tagFile                           = settings.value(tr("nedit.tagFile"), 					      QLatin1String("")).toString();
    wordDelimiters                    = settings.value(tr("nedit.wordDelimiters"),  			      QLatin1String(".,/\\`'!|@#%^&*()-=+{}[]\":;<>?")).toString();
    serverName                        = settings.value(tr("nedit.serverName"),  				      QLatin1String("")).toString();
    maxPrevOpenFiles                  = settings.value(tr("nedit.maxPrevOpenFiles"),			      30).toInt();
    smartTags                         = settings.value(tr("nedit.smartTags"),					      true).toBool();
    typingHidesPointer                = settings.value(tr("nedit.typingHidesPointer"),  		      false).toBool();
    alwaysCheckRelativeTagsSpecs      = settings.value(tr("nedit.alwaysCheckRelativeTagsSpecs"),      true).toBool();
    prefFileRead                      = settings.value(tr("nedit.prefFileRead"),				      false).toBool();
    findReplaceUsesSelection          = settings.value(tr("nedit.findReplaceUsesSelection"),	      false).toBool();
    overrideDefaultVirtualKeyBindings = settings.value(tr("nedit.overrideDefaultVirtualKeyBindings"), VIRT_KEY_OVERRIDE_AUTO).toInt();
    titleFormat                       = settings.value(tr("nedit.titleFormat"),                       QLatin1String("{%c} [%s] %f (%S) - %d")).toString();
    undoModifiesSelection             = settings.value(tr("nedit.undoModifiesSelection"),             true).toBool();
    focusOnRaise                      = settings.value(tr("nedit.focusOnRaise"),                      false).toBool();
    forceOSConversion                 = settings.value(tr("nedit.forceOSConversion"),                 true).toBool();
    truncSubstitution                 = settings.value(tr("nedit.truncSubstitution"),                 TRUNCSUBST_FAIL).toInt();
    honorSymlinks                     = settings.value(tr("nedit.honorSymlinks"),                     true).toBool();

    settingsLoaded_ = true;
}

void Settings::importSettings(const QString &filename) {
    if(!settingsLoaded_) {
        qWarning("NEdit: Warning, importing while no previous settings loaded!");
    }

    QSettings settings(filename, QSettings::IniFormat);

    fileVersion           = settings.value(tr("nedit.fileVersion"),           fileVersion).toString();
    shellCommands         = settings.value(tr("nedit.shellCommands"),         shellCommands).toString();
    macroCommands         = settings.value(tr("nedit.macroCommands"),         macroCommands).toString();
    bgMenuCommands        = settings.value(tr("nedit.bgMenuCommands"),        bgMenuCommands).toString();
    highlightPatterns     = settings.value(tr("nedit.highlightPatterns"),     highlightPatterns).toString();
    languageModes         = settings.value(tr("nedit.languageModes"),         languageModes).toString();
    styles                = settings.value(tr("nedit.styles"),                styles).toString();
    smartIndentInit       = settings.value(tr("nedit.smartIndentInit"),       smartIndentInit).toString();
    smartIndentInitCommon = settings.value(tr("nedit.smartIndentInitCommon"), smartIndentInitCommon).toString();
    autoWrap              = static_cast<WrapStyle>(settings.value(tr("nedit.autoWrap"), autoWrap).toInt());
    wrapMargin            = settings.value(tr("nedit.wrapMargin"),            wrapMargin).toInt();
    autoIndent            = settings.value(tr("nedit.autoIndent"),            autoIndent).toInt();
    autoSave              = settings.value(tr("nedit.autoSave"),              autoSave).toBool();
    openInTab             = settings.value(tr("nedit.openInTab"),             openInTab).toBool();
    saveOldVersion        = settings.value(tr("nedit.saveOldVersion"),        saveOldVersion).toBool();
    showMatching          = settings.value(tr("nedit.showMatching"),          showMatching).toInt();
    matchSyntaxBased      = settings.value(tr("nedit.matchSyntaxBased"),      matchSyntaxBased).toBool();
    highlightSyntax       = settings.value(tr("nedit.highlightSyntax"),       highlightSyntax).toBool();
    backlightChars        = settings.value(tr("nedit.backlightChars"),        backlightChars).toBool();
    backlightCharTypes    = settings.value(tr("nedit.backlightCharTypes"),    backlightCharTypes).toString();
    searchDialogs         = settings.value(tr("nedit.searchDialogs"),         searchDialogs).toBool();
    beepOnSearchWrap      = settings.value(tr("nedit.beepOnSearchWrap"),      beepOnSearchWrap).toBool();
    retainSearchDialogs   = settings.value(tr("nedit.retainSearchDialogs"),   retainSearchDialogs).toBool();
    searchWraps           = settings.value(tr("nedit.searchWraps"),           searchWraps).toBool();
    stickyCaseSenseButton = settings.value(tr("nedit.stickyCaseSenseButton"), stickyCaseSenseButton).toBool();
    repositionDialogs     = settings.value(tr("nedit.repositionDialogs"),     repositionDialogs).toBool();
    autoScroll            = settings.value(tr("nedit.autoScroll"),            autoScroll).toBool();
    autoScrollVPadding    = settings.value(tr("nedit.autoScrollVPadding"),    autoScrollVPadding).toInt();
    appendLF              = settings.value(tr("nedit.appendLF"),              appendLF).toBool();
    sortOpenPrevMenu      = settings.value(tr("nedit.sortOpenPrevMenu"),      sortOpenPrevMenu).toBool();
    statisticsLine        = settings.value(tr("nedit.statisticsLine"),        statisticsLine).toBool();
    iSearchLine           = settings.value(tr("nedit.iSearchLine"),           iSearchLine).toBool();
    sortTabs              = settings.value(tr("nedit.sortTabs"),              sortTabs).toBool();
    tabBar                = settings.value(tr("nedit.tabBar"),                tabBar).toBool();
    tabBarHideOne         = settings.value(tr("nedit.tabBarHideOne"),         tabBarHideOne).toBool();
    toolTips              = settings.value(tr("nedit.toolTips"),              toolTips).toBool();
    globalTabNavigate     = settings.value(tr("nedit.globalTabNavigate"),     globalTabNavigate).toBool();
    lineNumbers           = settings.value(tr("nedit.lineNumbers"),           lineNumbers).toBool();
    pathInWindowsMenu     = settings.value(tr("nedit.pathInWindowsMenu"),     pathInWindowsMenu).toBool();
    warnFileMods          = settings.value(tr("nedit.warnFileMods"),          warnFileMods).toBool();
    warnRealFileMods      = settings.value(tr("nedit.warnRealFileMods"),      warnRealFileMods).toBool();
    warnExit              = settings.value(tr("nedit.warnExit"),              warnExit).toBool();
    searchMethod          = settings.value(tr("nedit.searchMethod"),          searchMethod).toInt();
#if defined(REPLACE_SCOPE)
    replaceDefaultScope   = settings.value(tr("nedit.replaceDefaultScope"),   replaceDefaultScope).toInt();
#endif

    textRows                          = settings.value(tr("nedit.textRows"),					      textRows).toInt();
    textCols                          = settings.value(tr("nedit.textCols"),					      textCols).toInt();
    tabDistance                       = settings.value(tr("nedit.tabDistance"), 				      tabDistance).toInt();
    emulateTabs                       = settings.value(tr("nedit.emulateTabs"), 				      emulateTabs).toInt();
    insertTabs                        = settings.value(tr("nedit.insertTabs"),  				      insertTabs).toBool();
    textFont                          = settings.value(tr("nedit.textFont"),					      textFont).toString();
    boldHighlightFont                 = settings.value(tr("nedit.boldHighlightFont"),			      boldHighlightFont).toString();
    italicHighlightFont               = settings.value(tr("nedit.italicHighlightFont"), 		      italicHighlightFont).toString();
    boldItalicHighlightFont           = settings.value(tr("nedit.boldItalicHighlightFont"), 	      boldItalicHighlightFont).toString();

    colors[TEXT_FG_COLOR]             = settings.value(tr("nedit.textFgColor"), 				      colors[TEXT_FG_COLOR]).toString();
    colors[TEXT_BG_COLOR]             = settings.value(tr("nedit.textBgColor"), 				      colors[TEXT_BG_COLOR]).toString();
    colors[SELECT_FG_COLOR]           = settings.value(tr("nedit.selectFgColor"),				      colors[SELECT_FG_COLOR]).toString();
    colors[SELECT_BG_COLOR]           = settings.value(tr("nedit.selectBgColor"),				      colors[SELECT_BG_COLOR]).toString();
    colors[HILITE_FG_COLOR]           = settings.value(tr("nedit.hiliteFgColor"),				      colors[HILITE_FG_COLOR]).toString();
    colors[HILITE_BG_COLOR]           = settings.value(tr("nedit.hiliteBgColor"),				      colors[HILITE_BG_COLOR]).toString();
    colors[LINENO_FG_COLOR]           = settings.value(tr("nedit.lineNoFgColor"),				      colors[LINENO_FG_COLOR]).toString();
    colors[CURSOR_FG_COLOR]           = settings.value(tr("nedit.cursorFgColor"),				      colors[CURSOR_FG_COLOR]).toString();

    tooltipBgColor                    = settings.value(tr("nedit.tooltipBgColor"),  			      tooltipBgColor).toString();
    shell                             = settings.value(tr("nedit.shell"),						      shell).toString();
    geometry                          = settings.value(tr("nedit.geometry"),					      geometry).toString();
    remapDeleteKey                    = settings.value(tr("nedit.remapDeleteKey"),  			      remapDeleteKey).toBool();
    stdOpenDialog                     = settings.value(tr("nedit.stdOpenDialog"),				      stdOpenDialog).toBool();
    tagFile                           = settings.value(tr("nedit.tagFile"), 					      tagFile).toString();
    wordDelimiters                    = settings.value(tr("nedit.wordDelimiters"),  			      wordDelimiters).toString();
    serverName                        = settings.value(tr("nedit.serverName"),  				      serverName).toString();
    maxPrevOpenFiles                  = settings.value(tr("nedit.maxPrevOpenFiles"),			      maxPrevOpenFiles).toInt();
    smartTags                         = settings.value(tr("nedit.smartTags"),					      smartTags).toBool();
    typingHidesPointer                = settings.value(tr("nedit.typingHidesPointer"),  		      typingHidesPointer).toBool();
    alwaysCheckRelativeTagsSpecs      = settings.value(tr("nedit.alwaysCheckRelativeTagsSpecs"),      alwaysCheckRelativeTagsSpecs).toBool();
    prefFileRead                      = settings.value(tr("nedit.prefFileRead"),				      prefFileRead).toBool();
    findReplaceUsesSelection          = settings.value(tr("nedit.findReplaceUsesSelection"),	      findReplaceUsesSelection).toBool();
    overrideDefaultVirtualKeyBindings = settings.value(tr("nedit.overrideDefaultVirtualKeyBindings"), overrideDefaultVirtualKeyBindings).toInt();
    titleFormat                       = settings.value(tr("nedit.titleFormat"),                       titleFormat).toString();
    undoModifiesSelection             = settings.value(tr("nedit.undoModifiesSelection"),             undoModifiesSelection).toBool();
    focusOnRaise                      = settings.value(tr("nedit.focusOnRaise"),                      focusOnRaise).toBool();
    forceOSConversion                 = settings.value(tr("nedit.forceOSConversion"),                 forceOSConversion).toBool();
    truncSubstitution                 = settings.value(tr("nedit.truncSubstitution"),                 truncSubstitution).toInt();
    honorSymlinks                     = settings.value(tr("nedit.honorSymlinks"),                     honorSymlinks).toBool();
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
bool Settings::savePreferences() {
    QString filename = configFile();
    QSettings settings(filename, QSettings::IniFormat);

    settings.setValue(tr("nedit.fileVersion"), fileVersion);
    settings.setValue(tr("nedit.shellCommands"), shellCommands);
    settings.setValue(tr("nedit.macroCommands"), macroCommands);
    settings.setValue(tr("nedit.bgMenuCommands"), bgMenuCommands);
    settings.setValue(tr("nedit.highlightPatterns"), highlightPatterns);
    settings.setValue(tr("nedit.languageModes"), languageModes);
    settings.setValue(tr("nedit.styles"), styles);
    settings.setValue(tr("nedit.smartIndentInit"), smartIndentInit);
    settings.setValue(tr("nedit.smartIndentInitCommon"), smartIndentInitCommon);
    settings.setValue(tr("nedit.autoWrap"), autoWrap);
    settings.setValue(tr("nedit.wrapMargin"), wrapMargin);
    settings.setValue(tr("nedit.autoIndent"), autoIndent);
    settings.setValue(tr("nedit.autoSave"), autoSave);
    settings.setValue(tr("nedit.openInTab"), openInTab);
    settings.setValue(tr("nedit.saveOldVersion"), saveOldVersion);
    settings.setValue(tr("nedit.showMatching"), showMatching);
    settings.setValue(tr("nedit.matchSyntaxBased"), matchSyntaxBased);
    settings.setValue(tr("nedit.highlightSyntax"), highlightSyntax);
    settings.setValue(tr("nedit.backlightChars"), backlightChars);
    settings.setValue(tr("nedit.backlightCharTypes"), backlightCharTypes);
    settings.setValue(tr("nedit.searchDialogs"), searchDialogs);
    settings.setValue(tr("nedit.beepOnSearchWrap"), beepOnSearchWrap);
    settings.setValue(tr("nedit.retainSearchDialogs"), retainSearchDialogs);
    settings.setValue(tr("nedit.searchWraps"), searchWraps);
    settings.setValue(tr("nedit.stickyCaseSenseButton"), stickyCaseSenseButton);
    settings.setValue(tr("nedit.repositionDialogs"), repositionDialogs);
    settings.setValue(tr("nedit.autoScroll"), autoScroll);
    settings.setValue(tr("nedit.autoScrollVPadding"), autoScrollVPadding);
    settings.setValue(tr("nedit.appendLF"), appendLF);
    settings.setValue(tr("nedit.sortOpenPrevMenu"), sortOpenPrevMenu);
    settings.setValue(tr("nedit.statisticsLine"), statisticsLine);
    settings.setValue(tr("nedit.iSearchLine"), iSearchLine);
    settings.setValue(tr("nedit.sortTabs"), sortTabs);
    settings.setValue(tr("nedit.tabBar"), tabBar);
    settings.setValue(tr("nedit.tabBarHideOne"), tabBarHideOne);
    settings.setValue(tr("nedit.toolTips"), toolTips);
    settings.setValue(tr("nedit.globalTabNavigate"), globalTabNavigate);
    settings.setValue(tr("nedit.lineNumbers"), lineNumbers);
    settings.setValue(tr("nedit.pathInWindowsMenu"), pathInWindowsMenu);
    settings.setValue(tr("nedit.warnFileMods"), warnFileMods);
    settings.setValue(tr("nedit.warnRealFileMods"), warnRealFileMods);
    settings.setValue(tr("nedit.warnExit"), warnExit);
    settings.setValue(tr("nedit.searchMethod"), searchMethod);
#if defined(REPLACE_SCOPE)
	settings.setValue(tr("nedit.replaceDefaultScope"),     replaceDefaultScope);
#endif
    settings.setValue(tr("nedit.textRows"), textRows);
    settings.setValue(tr("nedit.textCols"), textCols);
    settings.setValue(tr("nedit.tabDistance"), tabDistance);
    settings.setValue(tr("nedit.emulateTabs"), emulateTabs);
    settings.setValue(tr("nedit.insertTabs"), insertTabs);
    settings.setValue(tr("nedit.textFont"), textFont);
    settings.setValue(tr("nedit.boldHighlightFont"), boldHighlightFont);
    settings.setValue(tr("nedit.italicHighlightFont"), italicHighlightFont);
    settings.setValue(tr("nedit.boldItalicHighlightFont"), boldItalicHighlightFont);
    settings.setValue(tr("nedit.textFgColor"), colors[TEXT_FG_COLOR]);
    settings.setValue(tr("nedit.textBgColor"), colors[TEXT_BG_COLOR]);
    settings.setValue(tr("nedit.selectFgColor"), colors[SELECT_FG_COLOR]);
    settings.setValue(tr("nedit.selectBgColor"), colors[SELECT_BG_COLOR]);
    settings.setValue(tr("nedit.hiliteFgColor"), colors[HILITE_FG_COLOR]);
    settings.setValue(tr("nedit.hiliteBgColor"), colors[HILITE_BG_COLOR]);
    settings.setValue(tr("nedit.lineNoFgColor"), colors[LINENO_FG_COLOR]);
    settings.setValue(tr("nedit.cursorFgColor"), colors[CURSOR_FG_COLOR]);
    settings.setValue(tr("nedit.tooltipBgColor"), tooltipBgColor);
    settings.setValue(tr("nedit.shell"), shell);
    settings.setValue(tr("nedit.geometry"), geometry);
    settings.setValue(tr("nedit.remapDeleteKey"), remapDeleteKey);
    settings.setValue(tr("nedit.stdOpenDialog"), stdOpenDialog);
    settings.setValue(tr("nedit.tagFile"), tagFile);
    settings.setValue(tr("nedit.wordDelimiters"), wordDelimiters);
    settings.setValue(tr("nedit.serverName"), serverName);
    settings.setValue(tr("nedit.maxPrevOpenFiles"), maxPrevOpenFiles);
    settings.setValue(tr("nedit.smartTags"), smartTags);
    settings.setValue(tr("nedit.typingHidesPointer"), typingHidesPointer);
    settings.setValue(tr("nedit.alwaysCheckRelativeTagsSpecs"), alwaysCheckRelativeTagsSpecs);
    settings.setValue(tr("nedit.prefFileRead"), prefFileRead);
    settings.setValue(tr("nedit.findReplaceUsesSelection"), findReplaceUsesSelection);
    settings.setValue(tr("nedit.overrideDefaultVirtualKeyBindings"), overrideDefaultVirtualKeyBindings);
    settings.setValue(tr("nedit.titleFormat"), titleFormat);
    settings.setValue(tr("nedit.undoModifiesSelection"), undoModifiesSelection);
    settings.setValue(tr("nedit.focusOnRaise"), focusOnRaise);
    settings.setValue(tr("nedit.forceOSConversion"), forceOSConversion);
    settings.setValue(tr("nedit.truncSubstitution"), truncSubstitution);
    settings.setValue(tr("nedit.honorSymlinks"), honorSymlinks);

    settings.sync();
    return settings.status() == QSettings::NoError;
}
