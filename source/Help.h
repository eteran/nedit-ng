
#ifndef HELP_H_
#define HELP_H_

#include <QCoreApplication>

class Help {
    Q_DECLARE_TR_FUNCTIONS(Help)

public:
    Help() = delete;

public:
    enum class Topic {
        Start,
        Select,
        Search,
        Clipboard,
        Mouse,
        Keyboard,
        Fill,
        Interface,
        Format,
        Programmer,
        Tabs,
        Indent,
        Syntax,
        Tags,
        Calltips,
        Basicsyntax,
        Escapesequences,
        Parenconstructs,
        Advancedtopics,
        Examples,
        Shell,
        Learn,
        MacroLang,
        MacroSubrs,
        Rangeset,
        Hiliteinfo,
        Actions,
        Customize,
        Preferences,
        Resources,
        Binding,
        Patterns,
        SmartIndent,
        CommandLine,
        Server,
        Recovery,
        Version,
        Distribution,
        MailingList,
        Defects,
        TabsDialog,
        CustomTitleDialog,
    };

    static void displayTopic(QWidget *parent, Topic topic);

};

#endif
