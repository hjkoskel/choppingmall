/* config.h - constants shared by the whole port. Source of truth: index.html. */
#ifndef CONFIG_H
#define CONFIG_H

#define SCREEN_W 960 /* js: W */
#define SCREEN_H 760 /* js: H */
#define MALL_CX 480.0 /* js: CX */
#define MALL_CY 395.0 /* js: CY */
#define MALL_AO 300.0 /* js: AO (outer wall apothem) */
#define MALL_AI 150.0 /* js: AI (inner shop block apothem) */
#define TAU 6.283185307179586 /* js: Math.PI * 2 */
#define PI_D 3.141592653589793

/* Fixed pool capacities (no malloc in per-frame code). Maxima measured by running the real
 * HTML game through 20 bot traces (tools/golden/trace_js.js prints them): bullets 6, robot
 * bolts 1, molotovs 1, fires 4, particles 275. Capacities are 10x or more above that; a full
 * pool simply drops new items. */
#define CAP_PARTS 4096
#define CAP_BUL 256
#define CAP_RB 32
#define CAP_MOLS 16
#define CAP_FIRES 32
#define CAP_OB 18

#endif
