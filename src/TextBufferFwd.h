
#ifndef TEXT_BUFFER_FWD_H_
#define TEXT_BUFFER_FWD_H_

#include <cstdint>
#include <string>

template <class Ch = char, class Tr = std::char_traits<Ch>>
class BasicTextBuffer;

using TextBuffer  = BasicTextBuffer<char>;
// TODO(eteran): it seems that technically, std::char_traits<uint8_t> is not
// supported by the C++ standard, but it works in practice. We will have to
// implement our own traits class for uint8_t if we want to be standard
// compliant.
using UTextBuffer = BasicTextBuffer<uint8_t>;

#endif
