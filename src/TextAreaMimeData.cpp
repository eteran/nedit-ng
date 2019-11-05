
#include "TextAreaMimeData.h"
#include "TextArea.h"
#include "TextBuffer.h"

/**
 * @brief TextAreaMimeData::TextAreaMimeData
 * @param buffer
 */
TextAreaMimeData::TextAreaMimeData(const std::shared_ptr<TextBuffer> &buffer) : buffer_(buffer) {
}

/**
 * @brief TextAreaMimeData::retrieveData
 * @param mimeType
 * @param type
 * @return
 */
QVariant TextAreaMimeData::retrieveData(const QString &mimeType, QVariant::Type type) const {

	auto that = const_cast<TextAreaMimeData *>(this);
	if(buffer_->primary.hasSelection()) {
		const std::string text = buffer_->BufGetSelectionText();
		that->setText(QString::fromStdString(text));
	}

	return QMimeData::retrieveData(mimeType, type);
}

/**
 * @brief TextAreaMimeData::formats
 * @return
 */
QStringList TextAreaMimeData::formats() const {

	if(buffer_->primary.hasSelection()) {
		static const QStringList f = {
		    QLatin1String("text/plain")
		};

		return f;
	}

	return QMimeData::formats();
}

/**
 * @brief TextAreaMimeData::hasFormat
 * @param mimeType
 * @return
 */
bool TextAreaMimeData::hasFormat(const QString &mimeType) const {
	return formats().contains(mimeType);
}

/**
 * @brief TextAreaMimeData::buffer
 * @return
 */
TextBuffer *TextAreaMimeData::buffer() const {
    return buffer_.get();
}


/**
 * @brief TextAreaMimeData::isOwner
 * @param mimeData
 * @param buffer
 * @return
 */
bool TextAreaMimeData::isOwner(const QMimeData *mimeData, const TextBuffer *buffer) {

	if(auto mime = qobject_cast<const TextAreaMimeData *>(mimeData)) {
		if(mime->buffer() == buffer) {
			return true;
		}
	}

	return false;
}
