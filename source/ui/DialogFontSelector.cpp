
#include <QtDebug>
#include <QMessageBox>
#include "DialogFontSelector.h"

#include <set>
#include <cmath>
#include <QStringList>
#include <QString>
#include "util/MotifHelper.h"

namespace {
const char DELIM = '-';
const int MAX_NUM_FONTS            = 32767;
const int TEMP_BUF_SIZE            = 256;
const int NUM_COMPONENTS_FONT_NAME = 14;

/*  gets a specific substring from a string */
void getStringComponent(const char *inStr, int pos, char *outStr) {
	int i;
	int j;

	*outStr = '\0';

	if (pos > NUM_COMPONENTS_FONT_NAME) {
		fprintf(stderr, "Warning: getStringComponent being used for ");
		fprintf(stderr, "pos > %d\nIf such ", NUM_COMPONENTS_FONT_NAME);
		fprintf(stderr, "use is intended remove these warning lines\n");
	}

	for (i = 0; (pos > 0) && (inStr[i] != '\0'); i++) {
		if (inStr[i] == DELIM) {
			pos--;
		}
	}

	if (inStr[i] == '\0') {
		return;
	}

	for (j = 0; (inStr[i] != DELIM) && (inStr[i] != '\0'); i++, j++) {
		outStr[j] = inStr[i];
	}

	outStr[j] = '\0';
}

/*  returns TRUE if argument is not name of a proportional font */
int notPropFont(const char *font) {
	char buff1[TEMP_BUF_SIZE];

	getStringComponent(font, 11, buff1);
	if ((strcmp(buff1, "p") == 0) || (strcmp(buff1, "P") == 0))
		return FALSE;
	else
		return TRUE;
}

/*  given a font name this function returns the part used in the first
    scroll list */
void getFontPart(const char *font, char *buff1) {
	char buff2[TEMP_BUF_SIZE];
	char buff3[TEMP_BUF_SIZE];
	char buff4[TEMP_BUF_SIZE];

	getStringComponent(font, 2, buff1);
	getStringComponent(font, 1, buff2);

	sprintf(buff3, "%s (%s", buff1, buff2);

	getStringComponent(font, 13, buff1);
	getStringComponent(font, 14, buff4);

	if (((strncmp(buff1, "iso8859", 7) == 0) || (strncmp(buff1, "ISO8859", 7) == 0)) && (strcmp(buff4, "1") == 0))
		sprintf(buff1, "%s)", buff3);
	else {
		sprintf(buff2, "%s, %s,", buff3, buff1);
		sprintf(buff1, "%s %s)", buff2, buff4);
	}
}

/*  given a font name this function returns the part used in the second
    scroll list */
void getStylePart(const char *font, char *buff1) {
	char buff2[TEMP_BUF_SIZE];
	char buff3[TEMP_BUF_SIZE];

	getStringComponent(font, 3, buff3);
	getStringComponent(font, 5, buff2);

	if ((strcmp(buff2, "normal") != 0) && (strcmp(buff2, "Normal") != 0) && (strcmp(buff2, "NORMAL") != 0))
		sprintf(buff1, "%s %s", buff3, buff2);
	else
		strcpy(buff1, buff3);

	getStringComponent(font, 6, buff2);

	if (buff2[0] != '\0')
		sprintf(buff3, "%s %s", buff1, buff2);
	else
		strcpy(buff3, buff1);

	getStringComponent(font, 4, buff2);

	if ((strcmp(buff2, "o") == 0) || (strcmp(buff2, "O") == 0))
		sprintf(buff1, "%s oblique", buff3);
	else if ((strcmp(buff2, "i") == 0) || (strcmp(buff2, "I") == 0))
		sprintf(buff1, "%s italic", buff3);

	if (strcmp(buff1, " ") == 0)
		strcpy(buff1, "-");
}

/*  given a font name this function returns the part used in the third
    scroll list */
void getSizePart(const char *font, char *buff1, int inPixels) {
	int size;

	if (inPixels) {
		getStringComponent(font, 7, buff1);
		size = atoi(buff1);
		sprintf(buff1, "%2d", size);
	} else {
		double temp;

		getStringComponent(font, 8, buff1);
		size = atoi(buff1);
		temp = (double)size / 10.0;
		if (buff1[strlen(buff1) - 1] == '0') {
			size = (int)floor(temp + 0.5);
			sprintf(buff1, "%2d", size);
		} else
			sprintf(buff1, "%4.1f", temp);
	}
}

}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
DialogFontSelector::DialogFontSelector(Widget parentWidget, int showPropFont, const char *originalFont, QColor foreground, QColor background, QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f), fontData_(nullptr), numFonts_(0) {
	ui.setupUi(this);
	
	switch(showPropFont) {
	case ONLY_FIXED: // shows only fixed fonts  doesn't show prop font toggle button also.
		ui.checkShowProportional->setChecked(false);
		ui.checkShowProportional->setVisible(false);
		break;
	case PREF_FIXED: // can select either fixed or proportional fonts; but starting option is Fixed fonts.
		ui.checkShowProportional->setChecked(false);
		ui.checkShowProportional->setVisible(true);
		break;
	case PREF_PROP:  // can select either fixed or proportional fonts; but starting option is proportional fonts.
		ui.checkShowProportional->setChecked(true);
		ui.checkShowProportional->setVisible(true);
		break;	
	}
	

	fontData_ = XListFonts(XtDisplay(parentWidget), "-*-*-*-*-*-*-*-*-*-*-*-*-*-*", MAX_NUM_FONTS, &numFonts_);

	setupScrollLists(NONE); // update scroll lists

	QPalette pal = ui.editSample->palette();
	pal.setColor(QPalette::Text, foreground);
	pal.setColor(QPalette::Base, background);
	ui.editSample->setPalette(pal);

	// set the default sample
	if(originalFont) {
		QFont font;
		font.setRawName(QLatin1String(originalFont));
		ui.editSample->setFont(font);
		ui.editSample->setEchoMode(QLineEdit::Normal);
		ui.editSample->setEnabled(true);
		ui.editFontName->setText(QLatin1String(originalFont));
	}			
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
DialogFontSelector::~DialogFontSelector() {
	XFreeFontNames(fontData_);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFontSelector::on_listFonts_itemSelectionChanged() {

	QList<QListWidgetItem *> selections = ui.listFonts->selectedItems();
	QString sel;
	if(selections.size() == 1) {
		sel = selections[0]->text();
	}

	sel1_ = sel;
	setupScrollLists(FONT);

	if ((!sel1_.isNull()) && (!sel2_.isNull()) && (!sel3_.isNull())) {
		choiceMade();
	} else {
		ui.editSample->setEchoMode(QLineEdit::NoEcho);
		ui.editSample->setEnabled(false);
		ui.editFontName->setText(QString());
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFontSelector::on_listStyles_itemSelectionChanged() {

	QList<QListWidgetItem *> selections = ui.listStyles->selectedItems();
	QString sel;
	if(selections.size() == 1) {
		sel = selections[0]->text();
	}

	sel2_ = sel;

	setupScrollLists(STYLE);
	if ((!sel1_.isNull()) && (!sel2_.isNull()) && (!sel3_.isNull())) {
		choiceMade();
	} else {
		ui.editSample->setEchoMode(QLineEdit::NoEcho);
		ui.editSample->setEnabled(false);
		ui.editFontName->setText(QString());
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFontSelector::on_listSizes_itemSelectionChanged() {

	QList<QListWidgetItem *> selections = ui.listSizes->selectedItems();
	QString sel;
	if(selections.size() == 1) {
		sel = selections[0]->text();
	}

	sel3_ = sel;
	setupScrollLists(SIZE);
	if ((!sel1_.isNull()) && (!sel2_.isNull()) && (!sel3_.isNull())) {
		choiceMade();
	} else {
		ui.editSample->setEchoMode(QLineEdit::NoEcho);
		ui.editSample->setEnabled(false);
		ui.editFontName->setText(QString());
	}
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFontSelector::on_checkShowInPoints_toggled(bool checked) {
	Q_UNUSED(checked);

	// NOTE(eteran): this is a change in behavior, the original would
	//               simply match the selection by index (presumably because
	//               it's the same resultant physical size)
	//               but instead, we just clear all selections
	ui.listFonts->clear();
	ui.listStyles->clear();
    ui.listSizes->clear();

	setupScrollLists(NONE);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFontSelector::on_checkShowProportional_toggled(bool checked) {
	Q_UNUSED(checked);

	ui.listFonts->clear();
	ui.listStyles->clear();
    ui.listSizes->clear();

	setupScrollLists(NONE);
}

//------------------------------------------------------------------------------
// Name:
//------------------------------------------------------------------------------
void DialogFontSelector::setupScrollLists(listSpecifier dontChange) {

	// this is to prevent unwanted cascading changes
	static bool inChange = false;
	if(inChange) {
		return;
	}

	inChange = true;

	std::set<QString> itemList1;
	std::set<QString> itemList2;
	std::set<QString> itemList3;

	char buff1[TEMP_BUF_SIZE];

	for (int i = 0; i < numFonts_; i++) {
		if ((dontChange != FONT) && (styleMatch(fontData_[i])) && (sizeMatch(fontData_[i])) && ((ui.checkShowProportional->isChecked()) || (notPropFont(fontData_[i])))) {
			getFontPart(fontData_[i], buff1);
			itemList1.insert(QLatin1String(buff1));
		}

		if ((dontChange != STYLE) && (fontMatch(fontData_[i])) && (sizeMatch(fontData_[i])) && ((ui.checkShowProportional->isChecked()) || (notPropFont(fontData_[i])))) {
			getStylePart(fontData_[i], buff1);
			itemList2.insert(QLatin1String(buff1));
		}

		if ((dontChange != SIZE) && (fontMatch(fontData_[i])) && (styleMatch(fontData_[i])) && ((ui.checkShowProportional->isChecked()) || (notPropFont(fontData_[i])))) {
			getSizePart(fontData_[i], buff1, ui.checkShowInPoints->isChecked());
			itemList3.insert(QLatin1String(buff1));
		}
	}

	//  recreate all three scroll lists where necessary
	if (dontChange != FONT) {

		// remember what was selected
		QList<QListWidgetItem *> selections = ui.listFonts->selectedItems();
		QString sel;
		if(selections.size() == 1) {
			sel = selections[0]->text();
		}

		// re-populate the list
		ui.listFonts->clear();
		for(QString str : itemList1) {
			ui.listFonts->addItem(str);
		}

		// re-select the previous selection
		if(!sel.isNull()) {
			QList<QListWidgetItem *> items = ui.listFonts->findItems(sel, Qt::MatchExactly);
			for(QListWidgetItem *item : items) {
				item->setSelected(true);
				ui.listFonts->scrollToItem(item);
			}
		}
	}

	if (dontChange != STYLE) {

		// remember what was selected
		QList<QListWidgetItem *> selections = ui.listStyles->selectedItems();
		QString sel;
		if(selections.size() == 1) {
			sel = selections[0]->text();
		}

		// re-populate the list
		ui.listStyles->clear();
		for(QString str : itemList2) {
			ui.listStyles->addItem(str);
		}

		// re-select the previous selection
		if(!sel.isNull()) {
			QList<QListWidgetItem *> items = ui.listStyles->findItems(sel, Qt::MatchExactly);
			for(QListWidgetItem *item : items) {
				item->setSelected(true);
				ui.listStyles->scrollToItem(item);
			}
		}
	}

	if (dontChange != SIZE) {

		// remember what was selected
		QList<QListWidgetItem *> selections = ui.listSizes->selectedItems();
		QString sel;
		if(selections.size() == 1) {
			sel = selections[0]->text();
		}

		// re-populate the list
		ui.listSizes->clear();
		for(QString str : itemList3) {
			ui.listSizes->addItem(str);
		}

		// re-select the previous selection
		if(!sel.isNull()) {
			QList<QListWidgetItem *> items = ui.listSizes->findItems(sel, Qt::MatchExactly);
			for(QListWidgetItem *item : items) {
				item->setSelected(true);
				ui.listSizes->scrollToItem(item);
			}
		}
	}

	inChange = false;
}


/*  returns true if the style portion of the font matches the currently selected style */
bool DialogFontSelector::styleMatch(const char *font) {
	char buff[TEMP_BUF_SIZE];

	if(sel2_.isNull()) {
		return true;
	}

	getStylePart(font, buff);

	if (sel2_ == QLatin1String(buff)) {
		return true;
	} else {
		return false;
	}
}

/*  returns true if the size portion of the font matches the currently selected size */
bool  DialogFontSelector::sizeMatch(const char *font) {
	char buff[TEMP_BUF_SIZE];

	if(sel3_.isNull()) {
		return true;
	}

	getSizePart(font, buff, ui.checkShowInPoints->isChecked());
	if (QLatin1String(buff) == sel3_) {
		return true;
	} else {
		return false;
	}
}

/*  returns true if the font portion of the font matches the currently selected font */
bool DialogFontSelector::fontMatch(const char *font) {
	char buff[TEMP_BUF_SIZE];

	if(sel1_.isNull()) {
		return true;
	}

	getFontPart(font, buff);
	if (QLatin1String(buff) == sel1_) {
		return true;
	} else {
		return false;
	}
}

/*  function called when all three choices have been made; sets up font name and displays sample font */
void DialogFontSelector::choiceMade() {

	QString fontName;

	for (int i = 0; i < numFonts_; i++) {
		if ((fontMatch(fontData_[i])) && (styleMatch(fontData_[i])) && (sizeMatch(fontData_[i]))) {
			fontName = QLatin1String(fontData_[i]);
			break;
		}
	}

	if (!fontName.isNull()) {
		QFont font;
		font.setRawName(fontName);
		ui.editSample->setFont(font);
		ui.editSample->setEchoMode(QLineEdit::Normal);
		ui.editSample->setEnabled(true);
		ui.editFontName->setText(fontName);
	} else {
		QMessageBox::critical(this, tr("Font Specification"), tr("Invalid Font Specification"));
	}
}
