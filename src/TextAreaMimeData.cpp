
#include "TextAreaMimeData.h"
#include "TextArea.h"
#include "TextBuffer.h"

/**
 * @brief Constructor for TextAreaMimeData.
 *
 * @param buffer The text buffer associated with this mime data.
 */
TextAreaMimeData::TextAreaMimeData(std::shared_ptr<TextBuffer> buffer)
	: buffer_(std::move(buffer)) {
}

/**
 * @brief Retrieve data from the mime data.
 *
 * @param mimeType The mime type of the data to retrieve.
 * @param type The type of the data to retrieve.
 * @return The retrieved data.
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
 * @brief Retrieve the formats of the mime data.
 *
 * @return A list of formats supported by this mime data.
 */
QStringList TextAreaMimeData::formats() const {

	if (buffer_->primary.hasSelection()) {
		static const QStringList f = {
			QStringLiteral("text/plain")};

		return f;
	}

	return QMimeData::formats();
}

/**
 * @brief Check if the mime data has a specific format.
 *
 * @param mimeType The mime type to check for.
 * @return `true` if the mime data has the specified format, `false` otherwise.
 */
bool TextAreaMimeData::hasFormat(const QString &mimeType) const {
	return formats().contains(mimeType);
}

/**
 * @brief Get the text buffer associated with this mime data.
 *
 * @return The text buffer associated with this mime data.
 */
TextBuffer *TextAreaMimeData::buffer() const {
	return buffer_.get();
}

/**
 * @brief Check if the mime data is owned by the specified text buffer.
 *
 * @param mimeData The mime data to check.
 * @param buffer The text buffer to check against.
 * @return `true` if the mime data is owned by the specified buffer, `false` otherwise.
 */
bool TextAreaMimeData::isOwner(const QMimeData *mimeData, const TextBuffer *buffer) {

	if (auto mime = qobject_cast<const TextAreaMimeData *>(mimeData)) {
		if (mime->buffer() == buffer) {
			return true;
		}
	}

	return false;
}
