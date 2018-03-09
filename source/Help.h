
#ifndef HELP_H_
#define HELP_H_

#include <QCoreApplication>

class Help {
    Q_DECLARE_TR_FUNCTIONS(Help)

private:
    Help() = delete;

public:
    enum class Topic {
        HELP_START,
        HELP_SELECT,
        HELP_SEARCH,
        HELP_CLIPBOARD,
        HELP_MOUSE,
        HELP_KEYBOARD,
        HELP_FILL,
        HELP_INTERFACE,
        HELP_FORMAT,
        HELP_PROGRAMMER,
        HELP_TABS,
        HELP_INDENT,
        HELP_SYNTAX,
        HELP_TAGS,
        HELP_CALLTIPS,
        HELP_BASICSYNTAX,
        HELP_ESCAPESEQUENCES,
        HELP_PARENCONSTRUCTS,
        HELP_ADVANCEDTOPICS,
        HELP_EXAMPLES,
        HELP_SHELL,
        HELP_LEARN,
        HELP_MACRO_LANG,
        HELP_MACRO_SUBRS,
        HELP_RANGESET,
        HELP_HILITEINFO,
        HELP_ACTIONS,
        HELP_CUSTOMIZE,
        HELP_PREFERENCES,
        HELP_RESOURCES,
        HELP_BINDING,
        HELP_PATTERNS,
        HELP_SMART_INDENT,
        HELP_COMMAND_LINE,
        HELP_SERVER,
        HELP_RECOVERY,
        HELP_VERSION,
        HELP_DISTRIBUTION,
        HELP_MAILING_LIST,
        HELP_DEFECTS,
        HELP_TABS_DIALOG,
        HELP_CUSTOM_TITLE_DIALOG,
    };

    static void displayTopic(Topic topic);

};

#endif
