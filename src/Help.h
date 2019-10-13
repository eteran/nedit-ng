
#ifndef HELP_H_
#define HELP_H_

#include "Util/QtHelper.h"

class QWidget;

namespace Help {
Q_DECLARE_NAMESPACE_TR(Help)

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

void displayTopic(QWidget *parent, Topic topic);
}

#endif
