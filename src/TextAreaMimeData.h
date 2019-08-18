
#ifndef TEXT_AREA_MIME_DATA_H_
#define TEXT_AREA_MIME_DATA_H_

#include <QMimeData>
#include "TextBufferFwd.h"
class TextArea;

class TextAreaMimeData : public QMimeData {
	Q_OBJECT
public:
	explicit TextAreaMimeData(TextBuffer *buffer);
	~TextAreaMimeData() override = default;

public:
	QStringList formats() const override final;
	bool hasFormat(const QString &mimeType) const override;

protected:
	QVariant retrieveData(const QString &mimeType, QVariant::Type type) const override;

public:
	TextBuffer *buffer() const;

public:
	static bool isOwner(const QMimeData *mimeData, const TextBuffer *buffer);

private:
	TextBuffer *buffer_;
};

#endif
