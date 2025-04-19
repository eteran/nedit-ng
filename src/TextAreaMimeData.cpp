
#include "TextAreaMimeData.h"
#include "TextArea.h"
#include "TextBuffer.h"

/**
 * @brief
 *
 * @param buffer
 */
TextAreaMimeData::TextAreaMimeData(std::shared_ptr<TextBuffer> buffer)
	: buffer_(std::move(buffer)) {
}

/**
 * @brief
 *
 * @param mimeType
 * @param type
 * @return
 */
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
QVariant TextAreaMimeData::retrieveData(const QString &mimeType, QMetaType type) const {
#else
QVariant TextAreaMimeData::retrieveData(const QString &mimeType, QVariant::Type type) const {
#endif
	auto that = const_cast<TextAreaMimeData *>(this);
	if (buffer_->primary.hasSelection()) {
		const std::string text = buffer_->BufGetSelectionText();
		that->setText(QString::fromStdString(text));
	}

	return QMimeData::retrieveData(mimeType, type);
}

/**
 * @brief
 *
 * @return
 */
QStringList TextAreaMimeData::formats() const {

	if (buffer_->primary.hasSelection()) {
		static const QStringList f = {
			QLatin1String("text/plain")};

		return f;
	}

	return QMimeData::formats();
}

/**
 * @brief
 *
 * @param mimeType
 * @return
 */
bool TextAreaMimeData::hasFormat(const QString &mimeType) const {
	return formats().contains(mimeType);
}

/**
 * @brief
 *
 * @return
 */
TextBuffer *TextAreaMimeData::buffer() const {
	return buffer_.get();
}

/**
 * @brief
 *
 * @param mimeData
 * @param buffer
 * @return
 */
bool TextAreaMimeData::isOwner(const QMimeData *mimeData, const TextBuffer *buffer) {

	if (auto mime = qobject_cast<const TextAreaMimeData *>(mimeData)) {
		if (mime->buffer() == buffer) {
			return true;
		}
	}

	return false;
}
