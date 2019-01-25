
#ifndef CLOSE_MODE_H_
#define CLOSE_MODE_H_

#include <QLatin1String>

enum class CloseMode {
	Prompt,
	Save,
	NoSave
};

inline constexpr QLatin1String to_string(CloseMode mode) {

	switch(mode) {
	case CloseMode::Prompt:
		return QLatin1String("prompt");
	case CloseMode::Save:
		return QLatin1String("save");
	case CloseMode::NoSave:
		return QLatin1String("nosave");
	}

	Q_UNREACHABLE();
}

#endif
