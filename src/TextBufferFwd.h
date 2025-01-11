
#ifndef TEXT_BUFFER_FWD_H_
#define TEXT_BUFFER_FWD_H_

#include <cstdint>
#include <string>

template <class Ch = char, class Tr = std::char_traits<Ch>>
class BasicTextBuffer;

using TextBuffer  = BasicTextBuffer<char>;
using UTextBuffer = BasicTextBuffer<uint8_t>;

#endif
