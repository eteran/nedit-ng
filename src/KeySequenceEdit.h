
#ifndef QKEYSEQUENCEEDIT_H
#define QKEYSEQUENCEEDIT_H

#include <QKeySequence>
#include <QToolButton>
#include <QVector>
#include <QWidget>

class QLineEdit;

/**
 * @class KeySequenceEdit
 * @brief The KeySequenceEdit widget allows to input a QKeySequence.
 *
 * This widget lets the user choose a QKeySequence, which is usually used as
 * a shortcut. The recording is initiated when the widget receives the focus
 * and ends one second after the user releases the last key.
 */
class KeySequenceEdit final : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QKeySequence keySequence READ keySequence WRITE setKeySequence NOTIFY keySequenceChanged USER true)
	Q_PROPERTY(int maximumSequenceLength READ maximumSequenceLength WRITE setMaximumSequenceLength NOTIFY maximumSequenceLengthChanged)
	Q_PROPERTY(bool modifierRequired READ modifierRequired WRITE setModifierRequired NOTIFY modifierRequiredChanged)
	Q_DISABLE_COPY(KeySequenceEdit)

public:
	explicit KeySequenceEdit(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	explicit KeySequenceEdit(const QKeySequence &keySequence, QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	~KeySequenceEdit() override = default;

public:
	QKeySequence keySequence() const;
	int maximumSequenceLength() const;
	bool modifierRequired() const;

public Q_SLOTS:
	void setKeySequence(const QKeySequence &keySequence);
	void setMaximumSequenceLength(int maximum);
	void setModifierRequired(bool required);
	void clear();

Q_SIGNALS:
	void editingFinished();
	void keySequenceChanged(const QKeySequence &keySequence);
	void maximumSequenceLengthChanged(int maximum);
	void modifierRequiredChanged(bool newValue);

protected:
	bool event(QEvent *) override;
	void keyPressEvent(QKeyEvent *) override;
	void keyReleaseEvent(QKeyEvent *) override;
	void timerEvent(QTimerEvent *) override;

private:
	void resetState();
	void finishEditing();

private:
	QLineEdit *lineEdit_;
	QKeySequence keySequence_;
	QVector<int> keys_;
	int prevKey_               = -1;
	int releaseTimer_          = 0;
	int maximumSequenceLength_ = 4;
	bool modifierRequired_     = false;
};

#endif
