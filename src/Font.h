
#ifndef FONT_H_
#define FONT_H_

class QString;
class QFont;

namespace Font {

enum Type {
	Plain  = 0,
	Italic = 1,
	Bold   = 2,
};

QFont fromString(const QString &fontName);

}

#endif
