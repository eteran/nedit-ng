
#include <QtDebug>
#include <QStringList>
#include <QString>
#include "DialogLanguageModes.h"
#include "preferences.h"


//------------------------------------------------------------------------------
// Name: DialogLanguageModes
//------------------------------------------------------------------------------
DialogLanguageModes::DialogLanguageModes(QWidget *parent, Qt::WindowFlags f) : QDialog(parent, f) {
	ui.setupUi(this);

	for (int i = 0; i < NLanguageModes; i++) {
		languageModes_.push_back(copyLanguageModeRec(LanguageModes[i]));
	}

	ui.listLanguages->addItem(tr("New"));
	for (languageModeRec *language : languageModes_) {
		ui.listLanguages->addItem(QLatin1String(language->name));
	}
	ui.listLanguages->setCurrentRow(0);
}

//------------------------------------------------------------------------------
// Name: ~DialogLanguageModes
//------------------------------------------------------------------------------
DialogLanguageModes::~DialogLanguageModes() {
	qDeleteAll(languageModes_);
}

//------------------------------------------------------------------------------
// Name: on_listLanguages_itemSelectionChanged
//------------------------------------------------------------------------------
void DialogLanguageModes::on_listLanguages_itemSelectionChanged() {
	QList<QListWidgetItem *> selections = ui.listLanguages->selectedItems();
	if(selections.size() != 1) {
		return;
	}

	QListWidgetItem *const selection = selections[0];
	QString languageName = selection->text();

	if(languageName == tr("New")) {
		ui.editName              ->setText(QString());
		ui.editExtensions        ->setText(QString());
		ui.editRegex             ->setText(QString());
		ui.editCallTips          ->setText(QString());
		ui.editDelimiters        ->setText(QString());
		ui.editTabSpacing        ->setText(QString());
		ui.editEmulatedTabSpacing->setText(QString());

		ui.radioIndentDefault->setChecked(true);
		ui.radioWrapDefault  ->setChecked(true);
	} else {
		for (languageModeRec *language : languageModes_) {
			if(languageName == QLatin1String(language->name)) {

				QStringList extensions;
				for(int i = 0; i < language->nExtensions; ++i) {
					extensions.push_back(QLatin1String(language->extensions[i]));
				}

				ui.editName      ->setText(QLatin1String(language->name));
				ui.editExtensions->setText(extensions.join(tr(" ")));
				ui.editRegex     ->setText(QLatin1String(language->recognitionExpr));
				ui.editCallTips  ->setText(QLatin1String(language->defTipsFile));
				ui.editDelimiters->setText(QLatin1String(language->delimiters));

				if(language->tabDist != -1) {
					ui.editTabSpacing->setText(tr("%1").arg(language->tabDist));
				} else {
					ui.editTabSpacing->setText(QString());
				}

				if(language->emTabDist != -1) {
					ui.editEmulatedTabSpacing->setText(tr("%1").arg(language->emTabDist));
				} else {
					ui.editEmulatedTabSpacing->setText(QString());
				}

				switch(language->indentStyle) {
				case 0:
					ui.radioIndentNone->setChecked(true);
					break;
				case 1:
					ui.radioIndentAuto->setChecked(true);
					break;
				case 2:
					ui.radioIndentSmart->setChecked(true);
					break;
				default:
					ui.radioIndentDefault->setChecked(true);
					break;
				}

				switch(language->wrapStyle) {
				case 0:
					ui.radioWrapNone->setChecked(true);
					break;
				case 1:
					ui.radioWrapAuto->setChecked(true);
					break;
				case 2:
					ui.radioWrapContinuous->setChecked(true);
					break;
				default:
					ui.radioWrapDefault->setChecked(true);
					break;
				}

				break;
			}
		}
	}
}

//------------------------------------------------------------------------------
// Name: on_buttonBox_accepted
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonBox_accepted() {
}

//------------------------------------------------------------------------------
// Name: on_buttonUp_clicked
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonUp_clicked() {
}

//------------------------------------------------------------------------------
// Name: on_buttonDown_clicked
//------------------------------------------------------------------------------
void DialogLanguageModes::on_buttonDown_clicked() {
}
