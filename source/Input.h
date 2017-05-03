
#ifndef INPUT_H_
#define INPUT_H_

#include <QString>
#include <cstddef>

class Input {
public:
	Input(const QString *input);
	Input(const Input &other) = default;
	Input& operator=(const Input &rhs) = default;
	
public:
	QChar operator*() const;
	Input& operator++();
	Input operator++(int);
	Input& operator--();
	Input operator--(int);

public:
	Input &operator+=(int n);

public:
	int operator-(const Input &rhs) const;

public:
	bool operator==(const Input &rhs) const;
	bool operator!=(const Input &rhs) const;

public:
	void skipWhitespace();
	void skipWhitespaceNL();
	bool atEnd() const;

public:
	bool match(const QString &s) const;
	int find(const QString &s) const;
	QString segment(int length) const;
	QString segment() const;

public:
	const QString *string() const;
	int index() const;
	
private:
	const QString *string_;
	int            index_;
};

#endif
