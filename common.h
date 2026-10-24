#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>

#define Int int
#define S64 ssize_t
#define U8 uint8_t
#define U16 uint16_t
#define U32 uint32_t
#define U64 uint64_t

#define Cast(x) (x)
#define Unused(x) (void)(x)

typedef struct string String;
struct string
{
    const U8 *data;
    Int length;
};

#endif // COMMON_H
