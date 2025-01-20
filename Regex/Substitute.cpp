
#include "Substitute.h"
#include "Common.h"
#include "Regex.h"
#include "Util/utils.h"

#include <algorithm>
#include <iterator>

/*
**  SubstituteRE - Perform substitutions after a 'Regex' match.
*/
bool Regex::SubstituteRE(std::string_view source, std::string &dest) const {

	constexpr auto InvalidParenNumber = static_cast<size_t>(-1);

	const Regex *re = this;

	char test;

	if (!re->isValid()) {
		reg_error("damaged Regex passed to 'SubstituteRE'");
		return false;
	}

	auto out = std::back_inserter(dest);
	Reader in(source);

	while(!in.eof()) {
		char ch = in.peek();
		in.read();

		char changeCase = '\0';
		size_t paren_no = InvalidParenNumber;

		if (ch == '\\') {
			// Process any case altering tokens, i.e \u, \U, \l, \L.

			if (in.peek() == 'u' || in.peek() == 'U' || in.peek() == 'l' || in.peek() == 'L') {
				changeCase = in.read();

				if (in.eof()) {
					break;
				}

				ch = in.read();
			}
		}

		if (ch == '&') {
			paren_no = 0;
		} else if (ch == '\\') {
			/* Can not pass register variable '&src' to function 'numeric_escape'
			   so make a non-register copy that we can take the address of. */

			Reader src_alias = in;

			if ('1' <= in.peek() && in.peek() <= '9') {
				paren_no = static_cast<size_t>(in.read() - '0');

			} else if ((test = literal_escape<char>(in.peek())) != '\0') {
				ch = test;
				in.read();

			} else if ((test = numeric_escape<char>(in.peek(), &src_alias)) != '\0') {
				ch = test;
				in = src_alias;
				in.read();

				/* NOTE: if an octal escape for zero is attempted (e.g. \000), it
				   will be treated as a literal string. */
			} else if (in.eof()) {
				/* If '\' is the last character of the replacement string, it is
				   interpreted as a literal backslash. */

				ch = '\\';
			} else {
				ch = in.read(); // Allow any escape sequence (This is
			} // INCONSISTENT with the 'CompileRE'
		} // mind set of issuing an error!

		if (paren_no == InvalidParenNumber) { // Ordinary character.
			*out++ = ch;
		} else if (re->startp[paren_no] != nullptr && re->endp[paren_no]) {

			/* The tokens \u and \l only modify the first character while the
			 * tokens \U and \L modify the entire string. */
			switch (changeCase) {
			case 'u': {
				int count = 0;
				std::transform(re->startp[paren_no], re->endp[paren_no], out, [&count](char ch) -> int {
					if (count++ == 0) {
						return safe_toupper(ch);
					}

					return ch;
				});
			} break;
			case 'U':
				std::transform(re->startp[paren_no], re->endp[paren_no], out, [](char ch) {
					return safe_toupper(ch);
				});
				break;
			case 'l': {
				int count = 0;
				std::transform(re->startp[paren_no], re->endp[paren_no], out, [&count](char ch) -> int {
					if (count++ == 0) {
						return safe_tolower(ch);
					}

					return ch;
				});
			} break;
			case 'L':
				std::transform(re->startp[paren_no], re->endp[paren_no], out, [](char ch) {
					return safe_tolower(ch);
				});
				break;
			default:
				std::copy(re->startp[paren_no], re->endp[paren_no], out);
				break;
			}
		}
	}

	return true;
}
