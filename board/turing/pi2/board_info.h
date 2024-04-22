#ifndef  BOARD_INFO_H
#define  BOARD_INFO_H

#define TURING_PI2_LATCH_STATE_ADDR 0x0709010c
#define TURING_PI2_BOOT_COOKIE_ADDR 0x07090108
#define TURING_PI2_BOOT_COOKIE_WARM 0x32695054 /* "TPi2" ASCII */
#define TURING_PI2_BOOT_COOKIE_FEL  0x5aa5a55a

#define TP_VER(maj, min, pat)\
    ((maj << 11) | ((min & 0x1F) << 6) | (pat & 0x3F))

#endif // ! BOARD_INFO_H
