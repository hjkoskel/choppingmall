/* mcfont.c - character to glyph number (portable: used by the generator and the C64 program) */
#include "mcfont.h"
u8 glyph_index(char ch) {
  u8 c = (u8)ch;
  if (c >= 48 && c <= 57) return (u8)(c - 48);
  if ((c >= 65 && c <= 90) || (c >= 193 && c <= 218)) return (u8)(10 + (c & 31) - 1); /* ASCII 'A' = 65, PETSCII 'A' = 193: both & 31 = 1 */
  switch (c) {
  case 32: return GL_SPACE;
  case 58: return GL_COLON;
  case 47: return GL_SLASH;
  case 43: return GL_PLUS;
  case 36: return GL_DOLLAR;
  case 33: return GL_BANG;
  case 45: return GL_MINUS;
  case 46: return GL_DOT;
  case 62: return GL_GT;
  default: return GL_QMARK;
  }
}
