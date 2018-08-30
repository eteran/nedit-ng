
#include "TextAreaMimeData.h"
#include "TextArea.h"
#include "TextBuffer.h"

TextAreaMimeData::TextAreaMimeData(TextBuffer *buffer) : buffer_(buffer) {
}

QVariant TextAreaMimeData::retrieveData(const QString &mimeType, QVariant::Type type) const {

	auto that = const_cast<TextAreaMimeData *>(this);
	if(buffer_->primary.selected) {
		const std::string text = buffer_->BufGetSelectionTextEx();
		that->setText(QString::fromStdString(text));
	}

	return QMimeData::retrieveData(mimeType, type);
}

QStringList TextAreaMimeData::formats() const {

	if(buffer_->primary.selected) {
		return QStringList() << QLatin1String("text/plain");
	}

	return QMimeData::formats();
}

bool TextAreaMimeData::hasFormat(const QString &mimeType) const {
	return formats().contains(mimeType);
}

TextBuffer *TextAreaMimeData::buffer() const {
	return buffer_;
}
