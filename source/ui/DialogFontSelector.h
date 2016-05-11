
#ifndef DIALOG_FONT_SELECTOR_H_
#define DIALOG_FONT_SELECTOR_H_

#include <QDialog>
#include <QColor>
#include "ui_DialogFontSelector.h"
#include <X11/Intrinsic.h>

#define ONLY_FIXED 0
#define PREF_FIXED 1
#define PREF_PROP  2

class DialogFontSelector : public QDialog {
	Q_OBJECT
private:
	enum listSpecifier {
		NONE,
		FONT,
		STYLE,
		SIZE
	};

public:
	DialogFontSelector(Widget parentWidget, int showPropFont, const char *originalFont, QColor foreground, QColor background, QWidget *parent = 0, Qt::WindowFlags f = 0);
	virtual ~DialogFontSelector();

private Q_SLOTS:
	void on_listFonts_itemSelectionChanged();
	void on_listStyles_itemSelectionChanged();
	void on_listSizes_itemSelectionChanged();
	void on_checkShowInPoints_toggled(bool checked);
	void on_checkShowProportional_toggled(bool checked);

private:
	void setupScrollLists(listSpecifier dontChange);
	bool styleMatch(const char *font);
	bool sizeMatch(const char *font);
	bool fontMatch(const char *font);
	void choiceMade();

public:
	Ui::DialogFontSelector ui;
	char **fontData_;   // font name info
	int numFonts_;      // number of fonts
	QString sel1_;      // selection from list 1
	QString sel2_;      // selection from list 2
	QString sel3_;      // selection from list 3
};

#endif
