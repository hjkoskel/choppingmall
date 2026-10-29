/* bot.h - a small autopilot for tests and the AUTOPLAY demo build. Returns joystick bits, sets *ev. */
#ifndef BOT_H
#define BOT_H
#include "types.h"
void bot_reset(void);
u8 bot_input(u8 *ev);
#endif
