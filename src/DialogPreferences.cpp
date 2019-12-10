
#include "DialogPreferences.h"
#include "BorderLayout.h"
#include "PageEditor.h"
#include <QLabel>
#include <QPlainTextEdit>
#include <QTextBrowser>

constexpr int CategoryIconSize = 48;

/**
 * @brief DialogPreferences::DialogPreferences
 * @param parent
 * @param f
 */
DialogPreferences::DialogPreferences(QWidget *parent, Qt::WindowFlags f)
	: QDialog(parent, f) {

	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
	setWindowTitle(tr("Preferences"));

	// setup the navigation
	auto layoutLeft = new QVBoxLayout;
	filter_         = new QLineEdit;
	contentsWidget_ = new QListWidget;
	filter_->setPlaceholderText(tr("Filter"));
	layoutLeft->addWidget(filter_);
	layoutLeft->addWidget(contentsWidget_);

	contentsWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
	contentsWidget_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
	contentsWidget_->setIconSize(QSize(CategoryIconSize, CategoryIconSize));
	contentsWidget_->setMovement(QListView::Static);

	// setup the buttonbox
	buttonBox_ = new QDialogButtonBox;
	buttonBox_->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel);
	connect(buttonBox_, &QDialogButtonBox::rejected, this, &DialogPreferences::close);
	connect(buttonBox_, &QDialogButtonBox::clicked, this, &DialogPreferences::buttonBoxClicked);

	// setup center!
	stackedLayout_ = new QStackedWidget;

	addPage(createPage1());
	addPage(createPage2());
	addPage(createPage3());

	connect(contentsWidget_, &QListWidget::currentRowChanged, stackedLayout_, &QStackedWidget::setCurrentIndex);
	contentsWidget_->setCurrentRow(0);

	auto layout = new BorderLayout;
	layout->addWidget(stackedLayout_, BorderLayout::Center);
	layout->add(layoutLeft, BorderLayout::West);
	layout->addWidget(buttonBox_, BorderLayout::South);
	setLayout(layout);
}

/**
 * @brief DialogPreferences::addPage
 * @param page
 */
void DialogPreferences::addPage(Page page) {

	auto widget = new QWidget;

	auto label = new QLabel(page.name);
	QFont font = label->font();
	font.setPointSize(16);
	font.setBold(true);
	label->setFont(font);

	auto layout = new QVBoxLayout;
	layout->addWidget(label);
	layout->addWidget(page.widget);

	widget->setLayout(layout);
	stackedLayout_->addWidget(widget);

	contentsWidget_->addItem(new QListWidgetItem(page.icon, page.name));
}

/**
 * @brief DialogPreferences::buttonBoxClicked
 * @param button
 */
void DialogPreferences::buttonBoxClicked(QAbstractButton *button) {

	switch (buttonBox_->standardButton(button)) {
	case QDialogButtonBox::Apply:
		applySettings();
		break;
	case QDialogButtonBox::Ok:
		acceptDialog();
		break;
	default:
		break;
	}
}

/**
 * @brief DialogPreferences::applySettings
 */
void DialogPreferences::applySettings() {
	qDebug("Apply");
}

/**
 * @brief DialogPreferences::acceptDialog
 */
void DialogPreferences::acceptDialog() {
	qDebug("Ok");
}

auto DialogPreferences::createPage1() -> Page {
	Page page;
	page.icon   = QIcon::fromTheme(QLatin1String("accessories-text-editor"));
	page.name   = tr("Editor");
	page.widget = new PageEditor;
	return page;
}

auto DialogPreferences::createPage2() -> Page {
	Page page;
	page.icon   = QIcon::fromTheme(QLatin1String("document-save-as"));
	page.name   = tr("Page 2");
	page.widget = new QPlainTextEdit;
	return page;
}

auto DialogPreferences::createPage3() -> Page {
	Page page;
	page.icon   = QIcon::fromTheme(QLatin1String("document-open"));
	page.name   = tr("Page 3");
	page.widget = new QTextBrowser;
	return page;
}
