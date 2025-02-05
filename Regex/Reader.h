
#ifndef READER_H_
#define READER_H_

#include <cassert>
#include <cstddef>
#include <optional>
#include <regex>
#include <stack>
#include <string_view>

template <class Ch>
class BasicReader {
public:
	struct Location {
		size_t line;
		size_t column;
	};

public:
	/**
	 * @brief Construct a new Basic Reader object for lexing a string
	 *
	 * @param input
	 */
	explicit BasicReader(std::basic_string_view<Ch> input) noexcept
		: input_(input) {
	}

	BasicReader()                                  = default;
	BasicReader(const BasicReader &other)          = default;
	BasicReader &operator=(const BasicReader &rhs) = default;
	~BasicReader()                                 = default;

public:
	/**
	 * @brief Returns true if the reader is at the end of the stream
	 *
	 * @return bool
	 */
	bool eof() const noexcept {
		return index_ == input_.size();
	}

	/**
	 * @brief Returns the next character in the string without advancing the position
	 *
	 * @return Ch
	 */
	Ch peek(size_t n) const noexcept {
		if (eof()) {
			return '\0';
		}

		std::basic_string_view<Ch> m = input_.substr(index_);
		if (n >= m.size()) {
			return '\0';
		}

		return m[n];
	}

	/**
	 * @brief Returns the next character in the string without advancing the position
	 *
	 * @return Ch
	 */
	Ch peek() const noexcept {
		if (eof()) {
			return '\0';
		}

		return input_[index_];
	}

	/**
	 * @brief returns true if the next character matches <ch>
	 *
	 * @return bool
	 */
	bool next_is(Ch ch) const noexcept {
		return peek() == ch;
	}

	/**
	 * @brief returns true if the next characters matches <s>
	 *
	 * @return bool
	 */
	bool next_is(std::string_view s) const noexcept {
		if (input_.compare(index_, s.size(), s) != 0) {
			return false;
		}

		return true;
	}

	/**
	 * @brief Returns the next character in the string and advances the position
	 *
	 * @return Ch
	 */
	Ch read() noexcept {
		if (eof()) {
			return '\0';
		}

		return input_[index_++];
	}

	/**
	 * @brief Consumes while the next character is in the input set <chars>
	 * and returns the number of consumed characters
	 *
	 * @param chars
	 * @return size_t
	 */
	size_t consume(std::basic_string_view<Ch> chars) noexcept {
		return consume_while([chars](Ch ch) {
			return chars.find(ch) != std::basic_string_view<Ch>::npos;
		});
	}

	/**
	 * @brief Consumes while the next character is whitespace (tab or space)
	 * and returns the number of consumed characters
	 *
	 * @return size_t
	 */
	size_t consume_whitespace() noexcept {
		return consume_while([](Ch ch) {
			return (ch == ' ' || ch == '\t');
		});
	}

	/**
	 * @brief Consumes while a given predicate function returns true
	 * and returns the number of consumed characters
	 *
	 * @param pred
	 * @return size_t
	 */
	template <class Pred>
	size_t consume_while(Pred pred) noexcept(noexcept(pred)) {
		size_t count = 0;
		while (!eof()) {
			const Ch ch = peek();
			if (!pred(ch)) {
				break;
			}

			++index_;
			++count;
		}
		return count;
	}

	/**
	 * @brief Returns true and advances the position
	 * if the next character matches <ch>
	 *
	 * @param ch
	 * @return bool
	 */
	bool match(Ch ch) noexcept {
		if (!next_is(ch)) {
			return false;
		}

		++index_;
		return true;
	}

	void putback() noexcept {
		if (index_ > 0) {
			--index_;
		}
	}

	/**
	 * @brief Returns true and advances the position
	 * if the next sequences of characters matches <s>
	 *
	 * @param s
	 * @return bool
	 */
	bool match(std::basic_string_view<Ch> s) noexcept {
		if (!next_is(s)) {
			return false;
		}

		index_ += s.size();
		return true;
	}

	/**
	 * @brief Matches a single character if it matches the given predicate
	 *
	 * @param pred
	 * @return Ch
	 */
	template <class Pred>
	Ch match_if(Pred pred) noexcept {
		Ch ch = peek();
		if (pred(ch)) {
			++index_;
			return ch;
		}

		return '\0';
	}

	/**
	 * @brief match until end of input or a newline is found
	 *
	 * @return std::basic_string_view<Ch>
	 */
	std::basic_string_view<Ch> match_line() noexcept {

		size_t start  = index_;
		size_t length = consume_while([](char ch) {
			return ch != '\r' && ch != '\n';
		});

		match('\r');
		match('\n');

		if (length == 0) {
			return {};
		}

		return input_.substr(start, length);
	}

	/**
	 * @brief Matches until the end of the input and returns the string matched
	 *
	 * @return std::optional<std::basic_string_view<Ch>>
	 */
	std::optional<std::basic_string_view<Ch>> match_any() {
		if (eof()) {
			return {};
		}

		std::basic_string_view<Ch> m = input_.substr(index_);
		index_ += m.size();
		return m;
	}

	/**
	 * @brief Returns the matching string and advances the position
	 * if the next sequences of characters matches <regex>
	 *
	 * @param s
	 * @return bool
	 */
	std::optional<std::basic_string_view<Ch>> match(const std::basic_regex<Ch> &regex) {
		std::match_results<const Ch *> matches;

		const Ch *first = &input_[index_];
		const Ch *last  = &input_[input_.size()];

		if (std::regex_search(first, last, matches, regex, std::regex_constants::match_continuous)) {
			std::basic_string_view<Ch> m(matches[0].first, matches[0].second - matches[0].first);
			index_ += m.size();
			return m;
		}

		return {};
	}

	/**
	 * @brief Returns the matching string and advances the position
	 * for each character satisfying the given predicate
	 *
	 * @param pred
	 * @return std::optional<std::basic_string_view<Ch>>
	 */
	template <class Pred>
	std::optional<std::basic_string_view<Ch>> match_while(Pred pred) {

		size_t start = index_;
		while (!eof()) {
			const Ch ch = peek();
			if (!pred(ch)) {
				break;
			}

			++index_;
		}

		std::basic_string_view<Ch> m(&input_[start], index_ - start);
		if (!m.empty()) {
			return m;
		}

		return {};
	}

	/**
	 * @brief Returns the current position in the string
	 *
	 * @return size_t
	 */
	size_t index() const noexcept {
		return index_;
	}

	/**
	 * @brief Returns the current position in the string as a line/column pair
	 *
	 * @return Location
	 */
	Location location() const noexcept {
		return location(index_);
	}

	/**
	 * @brief Returns the position of <index> in the string as line/column pair
	 *
	 * @param index
	 * @return Location
	 */
	Location location(size_t index) const noexcept {
		size_t line = 1;
		size_t col  = 1;

		if (index < input_.size()) {

			for (size_t i = 0; i < index; ++i) {
				if (input_[i] == '\n') {
					++line;
					col = 1;
				} else {
					++col;
				}
			}
		}

		return Location{line, col};
	}

private:
	std::basic_string_view<Ch> input_;
	size_t index_ = 0;
};

using Reader = BasicReader<char>;

#endif
