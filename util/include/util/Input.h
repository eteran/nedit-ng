
#ifndef INPUT_H_
#define INPUT_H_

#include <QString>
#include <QRegularExpression>
#include <cstddef>

class Input {
public:
	explicit Input(const QString *input);
	Input(const Input &other) = default;
	Input& operator=(const Input &rhs) = default;
	
public:
	QChar operator[](int index) const;
	QChar operator*() const;
	Input& operator++();
	Input operator++(int);
	Input& operator--();
	Input operator--(int);

public:
	Input &operator+=(int n);

public:
	int operator-(const Input &rhs) const;
	Input operator+(int rhs) const;

public:
	bool operator==(const Input &rhs) const;
	bool operator!=(const Input &rhs) const;

public:
	void skipWhitespace();
	void skipWhitespaceNL();
	bool atEnd() const;

public:
	bool match(const QString &s) const;
    bool match(QChar ch) const;
    int matchSize(const QRegularExpression &re) const;
	int find(const QString &s) const;
	int find(QChar ch) const;
	QString mid(int length) const;
	QString mid() const;
    QStringRef midRef(int length) const;
    QStringRef midRef() const;
    int remaining() const;
    QString readUntil(QChar ch);
    QChar read() const;

public:
	const QString *string() const;
	int index() const;
	
private:
	const QString *string_;
	int            index_;
};



#endif
