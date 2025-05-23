
#include "Util/Input.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QString>

/**
 * @brief Constructs an Input object from a QString pointer.
 *
 * @param input A QString that represents the input string.
 */
Input::Input(const QString *input)
	: string_(input), index_(0) {
	Q_ASSERT(input);
}

/**
 * @brief Advances the input index by a specified number of characters.
 *
 * @param n The number of characters to advance the index by.
 * @return Areference to the current Input object.
 */
Input &Input::operator+=(int n) {
	index_ += n;
	if (index_ > string_->size()) {
		index_ = string_->size();
	}

	return *this;
}

/**
 * @brief Decreases the input index by a specified number of characters.
 *
 * @param n The number of characters to decrease the index by.
 * @return Areference to the current Input object.
 */
Input &Input::operator-=(int n) {
	index_ -= n;
	if (index_ < 0) {
		index_ = 0;
	}

	return *this;
}

/**
 * @brief Returns the current character at the input index without advancing the index.
 *
 * @return The current character at the input index.
 *
 * @note If the index is out of bounds, it returns an empty QChar.
 */
QChar Input::operator*() const {
	return peek();
}

/**
 * @brief Returns the current character at the input index without advancing the index.
 *
 * @return The current character at the input index, or an empty QChar if at the end.
 */
QChar Input::peek() const {
	if (atEnd()) {
		return {};
	}

	return string_->at(index_);
}

/**
 * @brief Reads the current character at the input index and advances the index by one.
 *
 * @return The current character at the input index, or an empty QChar if at the end.
 */
QChar Input::read() {
	if (atEnd()) {
		return {};
	}

	QChar ch = string_->at(index_);

	++index_;

	return ch;
}

/**
 * @brief Reads the character at the given index relative to the current input index.
 *
 * @param index The relative index to read from the current input index.
 * @return The character at the specified index, or an empty QChar if out of bounds.
 */
QChar Input::operator[](int index) const {
	if ((index_ + index) >= string_->size()) {
		return {};
	}

	return string_->at(index_ + index);
}

/**
 * @brief Pre increment operator for Input.
 *
 * @return Areference to the current Input object.
 */
Input &Input::operator++() {
	if (!atEnd()) {
		++index_;
	}

	return *this;
}

/**
 * @brief Post increment operator for Input.
 *
 * @return Acopy of the current Input object before incrementing the index.
 */
Input Input::operator++(int) {
	const Input copy(*this);
	if (!atEnd()) {
		++index_;
	}
	return copy;
}

/**
 * @brief Pre decrement operator for Input.
 *
 * @return Areference to the current Input object.
 */
Input &Input::operator--() {
	if (index_ > 0) {
		--index_;
	}

	return *this;
}

/**
 * @brief Post decrement operator for Input.
 *
 * @return Acopy of the current Input object before decrementing the index.
 */
Input Input::operator--(int) {
	const Input copy(*this);
	if (index_ > 0) {
		--index_;
	}
	return copy;
}

/**
 * @brief Checks if the input has reached the end of the string.
 *
 * @return `true` if the input index is at the end of the string, `false` otherwise.
 */
bool Input::atEnd() const {
	return index_ == string_->size();
}

/**
 * @brief Skips whitespace characters (spaces and tabs) in the input string.
 */
void Input::skipWhitespace() {
	while (!atEnd() && (string_->at(index_) == QLatin1Char(' ') || string_->at(index_) == QLatin1Char('\t'))) {
		++index_;
	}
}

/**
 * @brief Skips whitespace characters (spaces, tabs, and newlines) in the input string.
 */
void Input::skipWhitespaceNL() {
	while (!atEnd() && (string_->at(index_) == QLatin1Char(' ') || string_->at(index_) == QLatin1Char('\t') || string_->at(index_) == QLatin1Char('\n'))) {
		++index_;
	}
}

/**
 * @brief Subtracts the index of another Input object from the current Input object's index.
 *
 * @param rhs The Input object to subtract from the current Input object's index.
 * @return The difference in indices between the two Input objects.
 */
int Input::operator-(const Input &rhs) const {
	Q_ASSERT(string_ == rhs.string_);
	return index_ - rhs.index_;
}

/**
 * @brief Adds an integer to the current Input object's index.
 *
 * @param rhs The integer to add to the current Input object's index.
 * @return Anew Input object with the updated index.
 */
Input Input::operator+(int rhs) const {
	Input next = *this;
	next += rhs;
	return next;
}

/**
 * @brief Subtracts an integer from the current Input object's index.
 *
 * @param rhs The integer to subtract from the current Input object's index.
 * @return Anew Input object with the updated index.
 */
Input Input::operator-(int rhs) const {
	Input next = *this;
	next -= rhs;
	return next;
}

/**
 * @brief Checks if two Input objects are equal.
 *
 * @param rhs The Input object to compare with the current Input object.
 * @return `true` if both Input objects have the same string and index, `false` otherwise.
 */
bool Input::operator==(const Input &rhs) const {
	return string_ == rhs.string_ && index_ == rhs.index_;
}

/**
 * @brief Checks if two Input objects are not equal.
 *
 * @param rhs The Input object to compare with the current Input object.
 * @return `true` if either the string or index of the two Input objects differ, `false` otherwise.
 */
bool Input::operator!=(const Input &rhs) const {
	return string_ != rhs.string_ || index_ != rhs.index_;
}

/**
 * @brief Consumes characters from the input string until a character not in the specified set is found.
 *
 * @param chars A string containing the characters to consume.
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
 * @brief Consumes characters from the input string that match a given regular expression.
 *
 * @param re The regular expression to match against the input string.
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
 * @brief Matches the input string against a regular expression and advances the index if successful.
 *
 * @param re The regular expression to match against the input string.
 * @param m An optional pointer to a QString where the captured match will be stored.
 * @return `true` if the match is successful, `false` otherwise.
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
 * @brief Matches the input string against a specific string and advances the index if successful.
 *
 * @param s The string to match against the input string.
 * @return `true` if the match is successful, `false` otherwise.
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
 * @brief Matches the input string against a specific character and advances the index if successful.
 *
 * @param ch The character to match against the input string.
 * @return `true` if the match is successful, `false` otherwise.
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
 * @brief Returns a substring starting from the current index with a specified length.
 *
 * @param length The length of the substring to return.
 * @return The substring starting from the current index with the specified length.
 */
QString Input::mid(int length) const {
	return string_->mid(index_, length);
}

/**
 * @brief Returns a substring starting from the current index to the end of the string.
 *
 * @return The substring starting from the current index to the end of the string.
 */
QString Input::mid() const {
	return string_->mid(index_);
}

/**
 * @brief Finds the index of a substring in the input string starting from the current index.
 *
 * @param s The substring to find in the input string.
 * @return The index of the substring in the input string, or -1 if not found.
 */
int Input::find(const QString &s) const {
	return string_->indexOf(s, index_);
}

/**
 * @brief Finds the index of a specific character in the input string starting from the current index.
 *
 * @param ch The character to find in the input string.
 * @return The index of the character in the input string, or -1 if not found.
 */
int Input::find(QChar ch) const {
	return string_->indexOf(ch, index_);
}

/**
 * @brief Returns the index of the current input position.
 *
 * @return The index of the current input position in the input string.
 */
int Input::index() const {
	return index_;
}

/**
 * @brief Returns a pointer to the input string.
 *
 * @return Apointer to the input string.
 */
const QString *Input::string() const {
	return string_;
}

/**
 * @brief Reads characters from the input string until a specified character is encountered.
 *
 * @param ch The character to read until.
 * @return The characters read until the specified character.
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
