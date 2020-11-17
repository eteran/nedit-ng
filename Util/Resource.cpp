
#include "Util/Resource.h"

#include <gsl/gsl_util>

#include <QByteArray>
#include <QResource>
#include <QString>

/**
 * @brief loadResource
 * @param resource
 * @return
 */
QByteArray loadResource(const QString &resource) {

	QResource res(resource);
	if (!res.isValid()) {
		qFatal("Failed to load internal resource");
	}

	// don't copy the data, if it's uncompressed, we can deal with it in place :-)
	auto defaults = QByteArray::fromRawData(reinterpret_cast<const char *>(res.data()), gsl::narrow<int>(res.size()));

#if QT_VERSION >= QT_VERSION_CHECK(5, 13, 0)
	if (res.compressionAlgorithm() == QResource::ZlibCompression) {
#else
	if (res.isCompressed()) {
#endif
		defaults = qUncompress(defaults);
	}

	return defaults;
}
