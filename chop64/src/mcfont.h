/* mcfont.h - glyph numbering of the multicolour-bitmap text font (3x5 pixels in a 4x8 multicolour cell).
 * The glyph images themselves are generated into assets_gen.h (ASSET_MCFONT, 8 bytes per glyph). */
#ifndef MCFONT_H
#define MCFONT_H
#include "types.h"
enum { GL_SPACE = 36, GL_COLON = 37, GL_SLASH = 38, GL_PLUS = 39, GL_DOLLAR = 40, GL_BANG = 41, GL_MINUS = 42,
       GL_DOT = 43, GL_GT = 44, GL_HEART = 45, GL_SOLID = 46, GL_QMARK = 47, GL_COUNT = 48 };
/* digits 0..9 -> 0..9, letters A..Z -> 10..35. Accepts ASCII and cc65's PETSCII ('A' arrives as 193). */
u8 glyph_index(char c);
#endif
