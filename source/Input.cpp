
#include "Input.h"

/**
 * @brief Input::Input
 * @param input
 */
Input::Input(const QString *input) : string_(input), index_(0) {
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
	if(atEnd()) {
		return QLatin1Char('\0');
	}

	return string_->at(index_);
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
 * @brief Input::match
 * @param s
 * @return
 */
bool Input::match(const QString &s) const {
	if(index_ + s.size() > string_->size()) {
		return false;
	}

	for(int i = 0; i < s.size(); ++i) {
		if(s[i] != string_->at(index_ + i)) {
			return false;
		}
	}

	return true;
}

/**
 * @brief Input::segment
 * @param length
 * @return
 */
QString Input::segment(int length) const {
	return string_->mid(index_, length);
}

/**
 * @brief Input::segment
 * @return
 */
QString Input::segment() const {
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
