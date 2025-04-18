
#include "Util/Input.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QString>

/**
 * @brief
 *
 * @param input
 */
Input::Input(const QString *input)
	: string_(input), index_(0) {
	Q_ASSERT(input);
}

/**
 * @brief
 *
 * @param n
 * @return
 */
Input &Input::operator+=(int n) {
	index_ += n;
	if (index_ > string_->size()) {
		index_ = string_->size();
	}

	return *this;
}

Input &Input::operator-=(int n) {
	index_ -= n;
	if (index_ < 0) {
		index_ = 0;
	}

	return *this;
}

/**
 * @brief
 *
 * @return
 */
QChar Input::operator*() const {
	return peek();
}

/**
 * @brief
 *
 * @return
 */
QChar Input::peek() const {
	if (atEnd()) {
		return {};
	}

	return string_->at(index_);
}

QChar Input::read() {
	if (atEnd()) {
		return {};
	}

	QChar ch = string_->at(index_);

	++index_;

	return ch;
}

QChar Input::operator[](int index) const {
	if ((index_ + index) >= string_->size()) {
		return {};
	}

	return string_->at(index_ + index);
}

/**
 * @brief
 *
 * @return
 */
Input &Input::operator++() {
	if (!atEnd()) {
		++index_;
	}

	return *this;
}

/**
 * @brief
 *
 * @return
 */
Input Input::operator++(int) {
	const Input copy(*this);
	if (!atEnd()) {
		++index_;
	}
	return copy;
}

/**
 * @brief
 *
 * @return
 */
Input &Input::operator--() {
	if (index_ > 0) {
		--index_;
	}

	return *this;
}

/**
 * @brief
 *
 * @return
 */
Input Input::operator--(int) {
	const Input copy(*this);
	if (index_ > 0) {
		--index_;
	}
	return copy;
}

/**
 * @brief
 *
 * @return
 */
bool Input::atEnd() const {
	return index_ == string_->size();
}

/**
 * @brief
 */
void Input::skipWhitespace() {
	while (!atEnd() && (string_->at(index_) == QLatin1Char(' ') || string_->at(index_) == QLatin1Char('\t'))) {
		++index_;
	}
}

/**
 * @brief
 */
void Input::skipWhitespaceNL() {
	while (!atEnd() && (string_->at(index_) == QLatin1Char(' ') || string_->at(index_) == QLatin1Char('\t') || string_->at(index_) == QLatin1Char('\n'))) {
		++index_;
	}
}

/**
 * @brief
 *
 * @param rhs
 * @return
 */
int Input::operator-(const Input &rhs) const {
	Q_ASSERT(string_ == rhs.string_);
	return index_ - rhs.index_;
}

/**
 * @brief
 *
 * @param rhs
 * @return
 */
Input Input::operator+(int rhs) const {
	Input next = *this;
	next += rhs;
	return next;
}

/**
 * @brief
 *
 * @param rhs
 * @return
 */
Input Input::operator-(int rhs) const {
	Input next = *this;
	next -= rhs;
	return next;
}

/**
 * @brief
 *
 * @param rhs
 * @return
 */
bool Input::operator==(const Input &rhs) const {
	return string_ == rhs.string_ && index_ == rhs.index_;
}

/**
 * @brief
 *
 * @param rhs
 * @return
 */
bool Input::operator!=(const Input &rhs) const {
	return string_ != rhs.string_ || index_ != rhs.index_;
}

/**
 * @brief
 *
 * @param chars
 */
void Input::consume(const QString &chars) {

	while (!atEnd()) {
		const QChar ch = peek();

		if (chars.indexOf(ch) == -1) {
			break;
		}

		read();
	}
}

/**
 * @brief
 *
 * @param re
 */
void Input::consume(const QRegularExpression &re) {
	const QRegularExpressionMatch match = re.match(
		*string_,
		index_,
		QRegularExpression::NormalMatch,
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		QRegularExpression::AnchorAtOffsetMatchOption
#else
		QRegularExpression::AnchoredMatchOption
#endif
	);

	if (match.hasMatch()) {
		const QString cap = match.captured(0);
		index_ += cap.size();
	}
}

/**
 * @brief
 *
 * @param re
 * @param m
 * @return
 */
bool Input::match(const QRegularExpression &re, QString *m) {

	const QRegularExpressionMatch match = re.match(
		*string_,
		index_,
		QRegularExpression::NormalMatch,
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		QRegularExpression::AnchorAtOffsetMatchOption
#else
		QRegularExpression::AnchoredMatchOption
#endif
	);
	if (match.hasMatch()) {

		const QString cap = match.captured(0);

		if (m) {
			*m = cap;
		}

		index_ += cap.size();
		return true;
	}

	return false;
}

/**
 * @brief
 *
 * @param s
 * @return
 */
bool Input::match(const QString &s) {
	if (index_ + s.size() > string_->size()) {
		return false;
	}

	// TODO(eteran): can be potentially optimized to not use a copy
	if (string_->mid(index_, s.size()) == s) {
		index_ += s.size();
		return true;
	}

	return false;
}

/**
 * @brief
 *
 * @param ch
 * @return
 */
bool Input::match(QChar ch) {
	if (index_ >= string_->size()) {
		return false;
	}

	if (string_->at(index_) == ch) {
		++index_;
		return true;
	}

	return false;
}

/**
 * @brief
 *
 * @param length
 * @return
 */
QString Input::mid(int length) const {
	return string_->mid(index_, length);
}

/**
 * @brief
 *
 * @return
 */
QString Input::mid() const {
	return string_->mid(index_);
}

/**
 * @brief
 *
 * @param s
 * @return
 */
int Input::find(const QString &s) const {
	return string_->indexOf(s, index_);
}

/**
 * @brief
 *
 * @param ch
 * @return
 */
int Input::find(QChar ch) const {
	return string_->indexOf(ch, index_);
}

/**
 * @brief
 *
 * @return
 */
int Input::index() const {
	return index_;
}

/**
 * @brief
 *
 * @return
 */
const QString *Input::string() const {
	return string_;
}

/**
 * @brief
 *
 * @param ch
 * @return
 */
QString Input::readUntil(QChar ch) {
	QString result;

	while (!atEnd()) {
		const QChar c = string_->at(index_);
		if (c == ch) {
			break;
		}

		result.append(c);
		++index_;
	}

	return result;
}
