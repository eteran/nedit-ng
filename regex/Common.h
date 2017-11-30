
#ifndef COMMON_H_
#define COMMON_H_

#include "Constants.h"
#include <cstdint>

template <class T>
unsigned int U_CHAR_AT(T *p) {
    return static_cast<unsigned int>(*p);
}

template <class T>
inline uint8_t *OPERAND(T *p) {
    return reinterpret_cast<uint8_t *>(p) + NODE_SIZE;
}

template <class T>
inline uint8_t GET_OP_CODE(T *p) {
    return *p;
}


#endif
