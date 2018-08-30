
#include "TextAreaMimeData.h"
#include "TextArea.h"
#include "TextBuffer.h"

/**
 * @brief TextAreaMimeData::TextAreaMimeData
 * @param buffer
 */
TextAreaMimeData::TextAreaMimeData(TextBuffer *buffer) : buffer_(buffer) {
}

/**
 * @brief TextAreaMimeData::retrieveData
 * @param mimeType
 * @param type
 * @return
 */
QVariant TextAreaMimeData::retrieveData(const QString &mimeType, QVariant::Type type) const {

	auto that = const_cast<TextAreaMimeData *>(this);
	if(buffer_->primary.selected) {
		const std::string text = buffer_->BufGetSelectionTextEx();
		that->setText(QString::fromStdString(text));
	}

	return QMimeData::retrieveData(mimeType, type);
}

/**
 * @brief TextAreaMimeData::formats
 * @return
 */
QStringList TextAreaMimeData::formats() const {

	if(buffer_->primary.selected) {
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
	return buffer_;
}
