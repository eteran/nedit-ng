
#include "Font.h"
#include <QFont>

namespace Font {

QFont fromString(const QString &fontName) {
	QFont font;
	font.fromString(fontName);
	font.setStyleStrategy(QFont::ForceIntegerMetrics);
	return font;
}

}
