
#ifndef TEXT_AREA_MIME_DATA_H_
#define TEXT_AREA_MIME_DATA_H_

#include <QMimeData>
#include "TextBufferFwd.h"
class TextArea;

class TextAreaMimeData : public QMimeData {
	Q_OBJECT
public:
	TextAreaMimeData(TextBuffer *buffer);
	~TextAreaMimeData() override = default;

public:
	QStringList formats() const override;
	bool hasFormat(const QString &mimeType) const override;

protected:
	QVariant retrieveData(const QString &mimeType, QVariant::Type type) const override;

public:
	TextBuffer *buffer() const;

private:
	TextBuffer *buffer_;
};

#endif
