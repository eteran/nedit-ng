
#include "FixedFontSelector.h"
#include "Font.h"
#include <QtDebug>

/**
 * @brief FixedFontSelector::FixedFontSelector
 * @param parent
 */
FixedFontSelector::FixedFontSelector(QWidget *parent) : QWidget(parent) {
	ui.setupUi(this);

    for(int size : QFontDatabase::standardSizes()) {
        ui.fontSize->addItem(tr("%1").arg(size), size);
	}
}

/**
 * @brief FixedFontSelector::~FixedFontSelector
 */
FixedFontSelector::~FixedFontSelector() {
}

/**
 * @brief FixedFontSelector::currentFont
 * @return
 */
QFont FixedFontSelector::currentFont() {
	return ui.fontCombo->currentFont();
}

/**
 * @brief FixedFontSelector::setCurrentFont
 * @param font
 */
void FixedFontSelector::setCurrentFont(const QString &font) {
    setCurrentFont(Font::fromString(font));
}

/**
 * @brief FixedFontSelector::setCurrentFont
 * @param font
 */
void FixedFontSelector::setCurrentFont(const QFont &font) {
	ui.fontCombo->setCurrentFont(font);
	int n = ui.fontSize->findData(font.pointSize());
	if(n != -1) {
		ui.fontSize->setCurrentIndex(n);
	}
}

/**
 * @brief FixedFontSelector::on_fontCombo_currentFontChanged
 * @param font
 */
void FixedFontSelector::on_fontCombo_currentFontChanged(const QFont &font) {
	Q_EMIT currentFontChanged(font);
}

/**
 * @brief FixedFontSelector::on_fontSize_currentIndexChanged
 * @param index
 */
void FixedFontSelector::on_fontSize_currentIndexChanged(int index) {

	QFont font = ui.fontCombo->currentFont();
	font.setPointSize(ui.fontSize->itemData(index).toInt());
	ui.fontCombo->setCurrentFont(font);

	Q_EMIT currentFontChanged(font);
}
