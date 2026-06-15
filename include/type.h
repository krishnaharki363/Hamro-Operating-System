#ifndef TYPE_H
#define TYPE_H

typedef unsigned int   u32;
typedef          int   s32;
typedef unsigned short u16;
typedef          short s16;
typedef unsigned char  u8;
typedef          char  s8;

#define low_16(val) (u16)((val) & 0xFFFF)
#define high_16(val) (u16)(((val) >> 16) & 0xFFFF)

#endif
