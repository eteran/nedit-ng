
#ifndef INPUT_H_
#define INPUT_H_

class QRegularExpression;
class QChar;
class QString;

class Input {
public:
    Input();
	explicit Input(const QString *input);
    Input(const Input &other)          = default;
	Input& operator=(const Input &rhs) = default;
    Input(Input &&other)               = default;
    Input& operator=(Input &&rhs)      = default;

public:
	QChar operator[](int index) const;
	QChar operator*() const;
	Input& operator++();
	Input operator++(int);
	Input& operator--();
	Input operator--(int);

public:
	Input &operator+=(int n);
    Input &operator-=(int n);

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
    void consume(const QString &chars);
    void consume(const QRegularExpression &re);
    bool match(const QString &s);
    bool match(QChar ch);
    bool match(const QRegularExpression &re, QString *m = nullptr);
	int find(const QString &s) const;
	int find(QChar ch) const;
	QString mid(int length) const;
	QString mid() const;
    QString readUntil(QChar ch);
    QChar peek() const;
    QChar read();

public:
	const QString *string() const;
	int index() const;
	
private:
	const QString *string_;
	int            index_;
};

#endif
