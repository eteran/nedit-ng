
#ifndef DIALOG_PREFERENCES_H_
#define DIALOG_PREFERENCES_H_

#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QIcon>
#include <QLineEdit>
#include <QListWidget>
#include <QStackedWidget>
#include <QString>

class DialogPreferences : public QDialog {
	Q_OBJECT
private:
	struct Page {
		QWidget *widget;
		QString name;
		QIcon icon;
	};

public:
	explicit DialogPreferences(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~DialogPreferences() override = default;

private:
	void addPage(Page page);

private:
	void buttonBoxClicked(QAbstractButton *button);
	void applySettings();
	void acceptDialog();

private:
	Page createPage1();
	Page createPage2();
	Page createPage3();

private:
	QStackedWidget *stackedLayout_;
	QLineEdit *filter_;
	QDialogButtonBox *buttonBox_;
	QListWidget *contentsWidget_;
};

#endif
