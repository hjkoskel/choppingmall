/* jingles.h - the win/lose stingers. Both return the piece length in seconds. */
#ifndef JINGLES_H
#define JINGLES_H
#include "wa/wa.h"
double jingle_win(wa *w, wa_id master);  /* js: jingleWin */
double jingle_lose(wa *w, wa_id master); /* js: jingleLose */
#endif
