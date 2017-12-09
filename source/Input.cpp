
#include "Input.h"
#include <QRegularExpressionMatch>

/**
 * @brief Input::Input
 * @param input
 */
Input::Input(const QString *input) : string_(input), index_(0) {
	Q_ASSERT(input);
}

Input &Input::operator+=(int n) {
	index_ += n;
	if(index_ > string_->size()) {
		index_ = string_->size();
	}

	return *this;
}

/**
 * @brief Input::operator *
 * @return
 */
QChar Input::operator*() const {
    return read();
}

/**
 * @brief Input::read
 * @return
 */
QChar Input::read() const {
    if(atEnd()) {
        return QChar();
    }

    return string_->at(index_);
}

QChar Input::operator[](int index) const {
	if((index_ + index) >= string_->size()) {
        return QChar();
	}

	return string_->at(index_ + index);
}

/**
 * @brief Input::operator ++
 * @return
 */
Input& Input::operator++() {
	if(!atEnd()) {
		++index_;
	}

	return *this;
}

/**
 * @brief Input::operator ++
 * @return
 */
Input Input::operator++(int) {
	Input copy(*this);
	if(!atEnd()) {
		++index_;
	}
	return copy;
}

/**
 * @brief Input::operator --
 * @return
 */
Input& Input::operator--() {
	if(index_ > 0) {
		--index_;
	}

	return *this;
}

/**
 * @brief Input::operator --
 * @return
 */
Input Input::operator--(int) {
	Input copy(*this);
	if(index_ > 0) {
		--index_;
	}
	return copy;
}

/**
 * @brief Input::eof
 * @return
 */
bool Input::atEnd() const {
	return index_ == string_->size();
}

/**
 * @brief Input::consumeWhitespace
 */
void Input::skipWhitespace() {
	while(!atEnd() && (string_->at(index_) == QLatin1Char(' ') || string_->at(index_) == QLatin1Char('\t'))) {
		++index_;
	}
}

void Input::skipWhitespaceNL() {
	while(!atEnd() && (string_->at(index_) == QLatin1Char(' ') || string_->at(index_) == QLatin1Char('\t') || string_->at(index_) == QLatin1Char('\n'))) {
		++index_;
	}
}

/**
 * @brief Input::operator -
 * @param rhs
 * @return
 */
int Input::operator-(const Input &rhs) const {
	Q_ASSERT(string_ == rhs.string_);
	return index_ - rhs.index_;
}

Input Input::operator+(int rhs) const {
	Input next = *this;
	next += rhs;
	return next;
}

/**
 * @brief Input::operator ==
 * @param rhs
 * @return
 */
bool Input::operator==(const Input &rhs) const {
	return string_ == rhs.string_ && index_ == rhs.index_;
}

/**
 * @brief Input::operator !=
 * @param rhs
 * @return
 */
bool Input::operator!=(const Input &rhs) const {
	return string_ != rhs.string_ || index_ != rhs.index_;
}

/**
 * @brief Input::matchSize
 * @param re
 * @return
 */
int Input::matchSize(const QRegularExpression &re) const {
    QRegularExpressionMatch match = re.match(string_, index_, QRegularExpression::NormalMatch, QRegularExpression::AnchoredMatchOption);
    if(match.hasMatch()) {
        return match.captured(0).size();
    }

    return 0;
}

/**
 * @brief Input::match
 * @param s
 * @return
 */
bool Input::match(const QString &s) const {
	if(index_ + s.size() > string_->size()) {
		return false;
	}

	return string_->midRef(index_, s.size()) == s;
}

/**
 * @brief Input::match
 * @param ch
 * @return
 */
bool Input::match(QChar ch) const {
    if(index_ >= string_->size()) {
        return false;
    }

    return string_->at(index_) == ch;
}

/**
 * @brief Input::midRef
 * @param length
 * @return
 */
QStringRef Input::midRef(int length) const {
    return string_->midRef(index_, length);
}

/**
 * @brief Input::midRef
 * @return
 */
QStringRef Input::midRef() const {
    return string_->midRef(index_);
}

/**
 * @brief Input::mid
 * @param length
 * @return
 */
QString Input::mid(int length) const {
	return string_->mid(index_, length);
}

/**
 * @brief Input::mid
 * @return
 */
QString Input::mid() const {
	return string_->mid(index_);
}

/**
 * @brief Input::find
 * @param s
 * @return
 */
int Input::find(const QString &s) const {
	return string_->indexOf(s, index_);
}

/**
 * @brief Input::find
 * @param ch
 * @return
 */
int Input::find(QChar ch) const {
	return string_->indexOf(ch, index_);
}

/**
 * @brief Input::index
 * @return
 */
int Input::index() const {
	return index_;
}

/**
 * @brief Input::string
 * @return
 */
const QString *Input::string() const {
	return string_;
}

/**
 * @brief Input::remaining
 * @return
 */
int Input::remaining() const {
    return string_->size() - index_;
}

/**
 * @brief Input::readUntil
 * @param ch
 * @return
 */
QString Input::readUntil(QChar ch) {
    QString result;

    while(!atEnd()) {
        QChar c = string_->at(index_);
        if(c == ch) {
            break;
        }

        result.append(c);
        ++index_;
    }

    return result;
}
