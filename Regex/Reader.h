
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
	 * @param input The string to read from
	 *
	 * @note The string must remain valid for the lifetime of the reader.
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
	 * @brief Determines if the reader has reached the end of the input string.
	 *
	 * @return `true` if the end of the input string has been reached, `false` otherwise.
	 */
	bool eof() const noexcept {
		return index_ == input_.size();
	}

	/**
	 * @brief Returns a character in the string without advancing the position.
	 *
	 * @param n The index of the character to peek at relative to the current point in the input.
	 * @return The character in the string, or '\0' if at the end of the string.
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
	 * @brief Returns the next character in the string without advancing the position.
	 *
	 * @return The next character in the string, or '\0' if at the end of the string.
	 */
	Ch peek() const noexcept {
		if (eof()) {
			return '\0';
		}

		return input_[index_];
	}

	/**
	 * @brief Determines if the next character in the string matches `ch`.
	 *
	 * @return `true` if the next character matches `ch`, `false` otherwise.
	 */
	bool next_is(Ch ch) const noexcept {
		return peek() == ch;
	}

	/**
	 * @brief Determines if the next sequence of characters in the string matches `s`.
	 *
	 * @return `true` if the next sequence matches `s`, `false` otherwise.
	 */
	bool next_is(std::string_view s) const noexcept {
		if (input_.compare(index_, s.size(), s) != 0) {
			return false;
		}

		return true;
	}

	/**
	 * @brief Reads the next character in the string and advances the position.
	 *
	 * @return The next character in the string, or '\0' if at the end of the string.
	 */
	Ch read() noexcept {
		if (eof()) {
			return '\0';
		}

		return input_[index_++];
	}

	/**
	 * @brief Consumes while the next character is in the input set <chars>
	 * and returns the number of consumed characters.
	 *
	 * @param chars A string view containing characters to consume.
	 * @return The number of characters consumed.
	 */
	size_t consume(std::basic_string_view<Ch> chars) noexcept {
		return consume_while([chars](Ch ch) {
			return chars.find(ch) != std::basic_string_view<Ch>::npos;
		});
	}

	/**
	 * @brief Consumes while the next character is whitespace (tab or space)
	 * and returns the number of consumed characters.
	 *
	 * @return The number of whitespace characters consumed.
	 */
	size_t consume_whitespace() noexcept {
		return consume_while([](Ch ch) {
			return (ch == ' ' || ch == '\t');
		});
	}

	/**
	 * @brief Consumes until the end of the input or a character `ch` is found.
	 *
	 * @param ch The character to consume until.
	 * @return The number of characters consumed until `ch` or end of input.
	 */
	size_t consume_until(Ch ch) noexcept {
		size_t count = 0;
		while (!eof()) {
			const Ch next = peek();
			if (next == ch) {
				break;
			}

			++index_;
			++count;
		}
		return count;
	}

	/**
	 * @brief Consumes while a given predicate function returns `true`
	 * and returns the number of consumed characters.
	 *
	 * @param pred A predicate function that takes a character and returns `true` if it should be consumed.
	 * @return The number of characters consumed.
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
	 * @brief If the `ch` matches the char at the current position, consume it and advance the position.
	 *
	 * @param ch The character to match.
	 * @return `true` if the next character matches `ch`, `false` otherwise.
	 */
	bool match(Ch ch) noexcept {
		if (!next_is(ch)) {
			return false;
		}

		++index_;
		return true;
	}

	/**
	 * @brief Puts back the last read character, effectively moving the position back by one.
	 */
	void putback() noexcept {
		if (index_ > 0) {
			--index_;
		}
	}

	/**
	 * @brief If the `s` matches the text at the current position, consume it and advance the position.
	 *
	 * @param s The string to match against the next characters in the input.
	 * @return `true` if the next characters match `s`, `false` otherwise.
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
	 * @param pred A predicate function that takes a character and returns `true` if it should be matched.
	 * @return The matched character if it satisfies the predicate, or '\0' if it does not match.
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
	 * @brief match until end of input or a newline is found.
	 *
	 * @return The matched line, or an empty string if no characters were matched.
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
	 * @brief Matches until the end of the input and returns the string matched.
	 *
	 * @return The matched string, or an empty string if no characters were matched.
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
	 * if the next sequences of characters matches `regex`
	 *
	 * @param regex The regular expression to match against the next characters in the input.
	 * @return The matched string if it satisfies the regex, or an empty optional if it does not match.
	 */
	std::optional<std::basic_string_view<Ch>> match(const std::basic_regex<Ch> &regex) {

		std::basic_string_view<Ch> str = input_.substr(index_);
		std::match_results<typename std::basic_string_view<Ch>::const_iterator> match;

		if (std::regex_search(str.begin(), str.end(), match, regex, std::regex_constants::match_continuous)) {

			auto index = std::distance(str.begin(), match[0].first);
			auto size  = std::distance(match[0].first, match[0].second);
			std::basic_string_view<Ch> m(str.data() + index, static_cast<size_t>(size));

			index_ += m.size();
			return m;
		}

		return {};
	}

	/**
	 * @brief Returns the matching string and advances the position
	 * for each character satisfying the given predicate
	 *
	 * @param pred A predicate function that takes a character and returns `true` if it should be matched.
	 * @return The matched string if it satisfies the predicate, or an empty optional if it does not match.
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
	 * @brief Get the current position in the string
	 *
	 * @return The current index in the string.
	 */
	size_t index() const noexcept {
		return index_;
	}

	/**
	 * @brief Get the current position in the string as a line/column pair.
	 *
	 * @return The current position in the string as a Location object.
	 */
	Location location() const noexcept {
		return location(index_);
	}

	/**
	 * @brief Get the position of `index` in the string as line/column pair
	 *
	 * @param index The index in the string to get the location for.
	 * @return The location of the specified index in the string as a Location object.
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
