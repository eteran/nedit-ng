
#ifndef FIXED_FONT_SELECTOR_H_
#define FIXED_FONT_SELECTOR_H_

#include <QWidget>

#include "ui_FixedFontSelector.h"

class FixedFontSelector : public QWidget {
	Q_OBJECT

public:
    FixedFontSelector(QWidget *parent = nullptr);
	virtual ~FixedFontSelector();

public:
	QFont currentFont();

public Q_SLOTS:
	void setCurrentFont(const QFont &font);
	void setCurrentFont(const QString &font);

private Q_SLOTS:
	void on_fontCombo_currentFontChanged(const QFont &font);
	void on_fontSize_currentIndexChanged(int index);

Q_SIGNALS:
	void currentFontChanged(const QFont &font);

private:
	Ui::FixedFontSelector ui;
};

#endif
