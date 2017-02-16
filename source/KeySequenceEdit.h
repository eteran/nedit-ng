
#ifndef QKEYSEQUENCEEDIT_H
#define QKEYSEQUENCEEDIT_H

#include <QKeySequence>
#include <QToolButton>
#include <QVector>
#include <QWidget>

class QLineEdit;

class KeySequenceEdit : public QWidget {
	Q_OBJECT
	Q_PROPERTY(QKeySequence keySequence READ keySequence WRITE setKeySequence NOTIFY keySequenceChanged USER true)
	Q_PROPERTY(int maximumSequenceLength READ maximumSequenceLength WRITE setMaximumSequenceLength NOTIFY maximumSequenceLengthChanged)
	Q_PROPERTY(bool modifierRequired READ modifierRequired WRITE setModifierRequired)
	Q_DISABLE_COPY(KeySequenceEdit)

public:
    explicit KeySequenceEdit(QWidget *parent = 0);
    explicit KeySequenceEdit(const QKeySequence &keySequence, QWidget *parent = 0);
    virtual ~KeySequenceEdit() override;

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

protected:
    virtual bool event(QEvent *) override;
    virtual void keyPressEvent(QKeyEvent *) override;
    virtual void keyReleaseEvent(QKeyEvent *)override;
    virtual void timerEvent(QTimerEvent *)override;

private:
    void resetState();
    void finishEditing();

private:
    QLineEdit *  lineEdit_;
    QKeySequence keySequence_;
	QVector<int> keys_;
    int          prevKey_;
    int          releaseTimer_;
	int          maximumSequenceLength_;
	bool         modifierRequired_;
	
};

#endif
