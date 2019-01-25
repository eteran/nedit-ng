
#ifndef FILE_FORMATS_H_
#define FILE_FORMATS_H_

#include <QLatin1String>

enum class FileFormats : int {
	Unix,
	Dos,
	Mac
};

inline constexpr QLatin1String to_string(FileFormats format) {

	switch(format) {
	case FileFormats::Unix:
		return QLatin1String("unix");
	case FileFormats::Dos:
		return QLatin1String("dos");
	case FileFormats::Mac:
		return QLatin1String("macintosh");
	}

	Q_UNREACHABLE();
}

#endif
