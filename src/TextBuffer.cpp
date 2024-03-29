
#include "TextBuffer.h"
#include "TextBuffer.tcc"

const char *BasicTextBufferBase::ControlCodeTable[32] = {
	"nul", "soh", "stx", "etx", "eot", "enq", "ack", "bel",
	"bs", "ht", "nl", "vt", "np", "cr", "so", "si",
	"dle", "dc1", "dc2", "dc3", "dc4", "nak", "syn", "etb",
	"can", "em", "sub", "esc", "fs", "gs", "rs", "us"};

// Force full instantiation
template class BasicTextBuffer<char>;
template class gap_buffer<char>;
