
#include "Substitute.h"
#include "Regex.h"
#include "Common.h"
#include <algorithm>
#include "util/utils.h"

/*
**  SubstituteRE - Perform substitutions after a 'Regex' match.
**
**  This function cleanly shortens results of more than max length to max.
**  To give the caller a chance to react to this the function returns false
**  on any error. The substitution will still be executed.
*/
bool Regex::SubstituteRE(view::string_view source, std::string &dest) const {

    const Regex *re = this;

    char test;

    if (U_CHAR_AT(re->program) != MAGIC) {
        reg_error("damaged Regex passed to 'SubstituteRE'");
        return false;
    }

    auto src = source.begin();
    auto dst = std::back_inserter(dest);

    while (src != source.end()) {

        char c = *src++;

        char chgcase = '\0';
        int paren_no = -1;

        if (c == '\\') {
            // Process any case altering tokens, i.e \u, \U, \l, \L.

            if (*src == 'u' || *src == 'U' || *src == 'l' || *src == 'L') {
                chgcase = *src++;

                if (src == source.end()) {
                    break;
                }

                c = *src++;
            }
        }

        if (c == '&') {
            paren_no = 0;
        } else if (c == '\\') {
            /* Can not pass register variable '&src' to function 'numeric_escape'
               so make a non-register copy that we can take the address of. */

            decltype(src) src_alias = src;

            if ('1' <= *src && *src <= '9') {
                paren_no = *src++ - '0';

            } else if ((test = literal_escape(*src)) != '\0') {
                c = test;
                src++;

            } else if ((test = numeric_escape(*src, &src_alias)) != '\0') {
                c   = test;
                src = src_alias;
                src++;

                /* NOTE: if an octal escape for zero is attempted (e.g. \000), it
                   will be treated as a literal string. */
            } else if (src == source.end()) {
                /* If '\' is the last character of the replacement string, it is
                   interpreted as a literal backslash. */

                c = '\\';
            } else {
                c = *src++; // Allow any escape sequence (This is
            }               // INCONSISTENT with the 'CompileRE'
        }                   // mind set of issuing an error!

        if (paren_no < 0) { // Ordinary character.
            *dst++ = c;
        } else if (re->startp[paren_no] != nullptr && re->endp[paren_no]) {

            /* The tokens \u and \l only modify the first character while the
             * tokens \U and \L modify the entire string. */
            switch(chgcase) {
            case 'u':
                {
                    int count = 0;
                    std::transform(re->startp[paren_no], re->endp[paren_no], dst, [&count](char ch) -> int {
                        if(count++ == 0) {
                            return safe_ctype<toupper>(ch);
                        } else {
                            return ch;
                        }
                    });
                }
                break;
            case 'U':
                std::transform(re->startp[paren_no], re->endp[paren_no], dst, [](char ch) {
                    return safe_ctype<toupper>(ch);
                });
                break;
            case 'l':
                {
                    int count = 0;
                    std::transform(re->startp[paren_no], re->endp[paren_no], dst, [&count](char ch) -> int {
                        if(count++ == 0) {
                            return safe_ctype<tolower>(ch);
                        } else {
                            return ch;
                        }
                    });
                }
                break;
            case 'L':
                std::transform(re->startp[paren_no], re->endp[paren_no], dst, [](char ch) {
                    return safe_ctype<tolower>(ch);
                });
                break;
            default:
                std::copy(re->startp[paren_no], re->endp[paren_no], dst);
                break;
            }

        }
    }

    return true;
}
