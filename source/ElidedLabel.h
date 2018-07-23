
#ifndef ELIDED_LABEL_H_
#define ELIDED_LABEL_H_

#include <QLabel>

class ElidedLabel : public QLabel {
    Q_OBJECT
    Q_PROPERTY(Qt::TextElideMode textElideMode READ textElideMode WRITE setTextElideMode)

public:
    /**
    * Default constructor.
    */
    explicit ElidedLabel(QWidget *parent = nullptr);
    explicit ElidedLabel(const QString &text, QWidget *parent = nullptr);

    ~ElidedLabel() noexcept override = default;

public:
    QSize minimumSizeHint() const override;
    QSize sizeHint() const override;

    /**
    * Overridden for internal reasons; the API remains unaffected.
    */
    virtual void setAlignment(Qt::Alignment);

    /**
    *  Returns the text elide mode.
    */
    Qt::TextElideMode textElideMode() const;

    /**
    * Sets the text elide mode.
    * @param mode The text elide mode.
    */
    void setTextElideMode(Qt::TextElideMode mode);

    /**
    * Get the full text set via setText.
    */
    QString fullText() const;

public Q_SLOTS:
    /**
    * Sets the text. Note that this is not technically a reimplementation of
    * QLabel::setText(), which is not virtual (in Qt 4.3). Therefore, you may
    * need to cast the object to ElidedLabel in some situations: \Example \code
    * ElidedLabel* squeezed = new ElidedLabel("text", parent);
    * QLabel* label = squeezed;
    * label->setText("new text");	// this will not work
    * squeezed->setText("new text");	// works as expected
    * static_cast<ElidedLabel*>(label)->setText("new text");	// works as
    * expected \endcode
    * @param mode The new text.
    */
    void setText(const QString &text);

    /**
    * Clears the text. Same remark as above.
    *
    */
    void clear();

protected:

    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *) override;
    void contextMenuEvent(QContextMenuEvent *) override;
    void squeezeTextToLabel();

private:
    QString fullText_;
    Qt::TextElideMode elideMode_;
};

#endif
