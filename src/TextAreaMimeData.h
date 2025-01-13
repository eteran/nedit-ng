
#ifndef TEXT_AREA_MIME_DATA_H_
#define TEXT_AREA_MIME_DATA_H_

#include "TextBufferFwd.h"
#include <QMimeData>
#include <memory>

class TextArea;

class TextAreaMimeData final : public QMimeData {
	Q_OBJECT
public:
	explicit TextAreaMimeData(std::shared_ptr<TextBuffer> buffer);
	~TextAreaMimeData() override = default;

public:
	QStringList formats() const final;
	bool hasFormat(const QString &mimeType) const override;

protected:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QVariant retrieveData(const QString &mimeType, QMetaType type) const override;
#else
	QVariant retrieveData(const QString &mimeType, QVariant::Type type) const override;
#endif

public:
	TextBuffer *buffer() const;

public:
	static bool isOwner(const QMimeData *mimeData, const TextBuffer *buffer);

private:
	std::shared_ptr<TextBuffer> buffer_;
};

#endif
