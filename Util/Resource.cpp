
#include "Util/Resource.h"

#include <QByteArray>
#include <QResource>
#include <QString>

#include <gsl/gsl_util>

/**
 * @brief Loads an internal resource from the application's resources.
 *
 * @param resource The name of the resource to load, e.g., ":/path/to/resource".
 * @return The resource data.
 */
QByteArray LoadResource(const QString &resource) {

	const QResource res(resource);
	if (!res.isValid()) {
		qFatal("Failed to load internal resource");
	}

	// don't copy the data, if it's uncompressed, we can deal with it in place :-)
	auto defaults = QByteArray::fromRawData(reinterpret_cast<const char *>(res.data()), gsl::narrow<int>(res.size()));

#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
	switch (res.compressionAlgorithm()) {
	case QResource::NoCompression:
		break;
	case QResource::ZlibCompression:
		defaults = qUncompress(defaults);
		break;
	case QResource::ZstdCompression:
		qFatal("Resources compiled with Zstd compression which is not supported by this build of NEdit.");
		break;
	default:
		qFatal("Resources compiled with an unsupported compression algorithm.");
		break;
	}
#else
	if (res.isCompressed()) {
		defaults = qUncompress(defaults);
	}
#endif

	return defaults;
}
