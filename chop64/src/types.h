/* types.h - fixed-size types shared by the portable core (cc65 on the C64, gcc on the host).
 * NOTE: int is 16 bits on the C64 and 32 on the host. The core keeps every intermediate within
 * +-32767 (or uses u16 where noted) so both compilers compute identical results. */
#ifndef TYPES_H
#define TYPES_H
#include <stdint.h>
typedef int8_t i8;
typedef uint8_t u8;
typedef int16_t i16;
typedef uint16_t u16;
typedef uint32_t u32;
#endif
