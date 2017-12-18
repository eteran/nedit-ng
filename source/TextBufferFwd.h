
#ifndef TEXT_BUFFER_FWD_H_
#define TEXT_BUFFER_FWD_H_

#include <string>

template <class Ch = char, class Tr = std::char_traits<Ch>>
class BasicTextBuffer;

using TextBuffer = BasicTextBuffer<char, std::char_traits<char>>;

#endif
