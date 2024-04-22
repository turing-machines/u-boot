#ifndef  BOARD_INFO_H
#define  BOARD_INFO_H

#include <linux/types.h>

#define TP_VER(maj, min, pat)\
    ((maj << 11) | ((min & 0x1F) << 6) | (pat & 0x3F))

#pragma pack(push, 1)
typedef struct tpi_board_info {
    // first two bytes are reserved to not interfere 'unmanaged mode' of the
    // realtek switch.
    uint16_t reserved;
    // crc that is calculated over this struct, starting after this field.
    uint32_t crc32;
    uint16_t hdr_version;
    uint16_t hw_version;
    // days since may 1st 2024. Date when this board was flashed in the factory
    uint16_t factory_date;
    char factory_serial[8];
    char product_name[16];
    char mac[6];
} tpi_board_info;
#pragma pack(pop)

u32 compute_crc(tpi_board_info* info);

#endif // ! BOARD_INFO_H
