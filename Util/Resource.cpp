
#include "Util/Resource.h"

#include <gsl/gsl_util>

#include <QResource>
#include <QByteArray>
#include <QString>

/**
 * @brief loadResource
 * @param resource
 * @return
 */
QByteArray loadResource(const QString &resource) {

	QResource res(resource);
	if(!res.isValid()) {
		qFatal("Failed to load internal resource");
	}

	// don't copy the data, if it's uncompressed, we can deal with it in place :-)
	auto defaults = QByteArray::fromRawData(reinterpret_cast<const char *>(res.data()), gsl::narrow<int>(res.size()));

	if(res.isCompressed()) {
		defaults = qUncompress(defaults);
	}

	return defaults;
}
