
#include "Font.h"
#include <QFont>

namespace Font {

QFont fromString(const QString &fontName) {
	QFont font;
	font.fromString(fontName);
	font.setStyleName(QString());
	font.setStyleStrategy(QFont::ForceIntegerMetrics);
	return font;
}

}
