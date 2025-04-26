
#ifndef NEW_MODE_H_
#define NEW_MODE_H_

#include <QLatin1String>

enum class NewMode {
	Prefs,
	Tab,
	Window,
	Opposite
};

inline QLatin1String ToString(NewMode mode) {

	switch (mode) {
	case NewMode::Prefs:
		return QLatin1String("prefs");
	case NewMode::Tab:
		return QLatin1String("tab");
	case NewMode::Window:
		return QLatin1String("window");
	case NewMode::Opposite:
		return QLatin1String("opposite");
	}

	Q_UNREACHABLE();
}

#endif
