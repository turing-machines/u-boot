// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 Sam Edwards <CFSworks@gmail.com>
 * Copyright (C) 2024 Sven Rademakers <sven@turingpi.com>
 *
 * Early init for the Turing Pi 2 clusterboard.
 */

#include <asm/gpio.h>
#include <asm/io.h>
#include <bloblist.h>
#include <board_info.h>
#include <common.h>
#include <i2c.h>
#include <init.h>
#include <sunxi_gpio.h>

#define TURING_PI2_LATCH_STATE_ADDR 0x0709010c
#define TURING_PI2_BOOT_COOKIE_ADDR 0x07090108
#define TURING_PI2_BOOT_COOKIE_WARM 0x32695054 /* "TPi2" ASCII */
#define TURING_PI2_BOOT_COOKIE_FEL 0x5aa5a55a
#define LOG_DEBUG
#define DEBUG
/*
 * RTL8370MB switch reset (active-low) on PG13 should be asserted very
 * early in boot to prevent Ethernet from coming up until the switch
 * can be configured. The user may be using the Turing Pi 2 as a router
 * or some other kind of network isolation device.
 */
static void turingpi2_ethsw_rst(u16 tpi_version) {
  if (tpi_version < TP_VER(2, 5, 0)) {
    gpio_direction_output(SUNXI_GPG(13), 0);
  } else {
    gpio_direction_output(SUNXI_GPG(3), 0);
  }
}

static void init_latches(u16 tpi_version) {
  if (tpi_version > TP_VER(2, 5, 0)) {
    gpio_direction_output(SUNXI_GPD(7), 0);
    gpio_direction_output(SUNXI_GPD(6), 0);
    gpio_direction_output(SUNXI_GPD(5), 0);
    gpio_direction_output(SUNXI_GPD(4), 0);
    // store new state inside latch_state_addr
    u16 latch_state = readl(TURING_PI2_LATCH_STATE_ADDR);
    writel(latch_state & ~0xF, TURING_PI2_LATCH_STATE_ADDR);
  }
}

static int board_info_from_eeprom(tpi_board_info *info) {
  uint chip = 0x50;
  uint addr = 0;
  int res = i2c_read(chip, addr, 1, (uint8_t *)info, sizeof(tpi_board_info));
  if (res) {
    printf("Error: reading EEPROM %d\n", res);
    return res;
  }

  return 0;
}

u32 compute_crc(tpi_board_info *info) {
  int info_offset = offsetof(tpi_board_info, hdr_version);
  u32 crc = crc32(0, (void *)info + info_offset,
                  sizeof(tpi_board_info) - info_offset);
  return ((crc & 0x000000FF) << 24) | ((crc & 0x0000FF00) << 8) |
         ((crc & 0x00FF0000) >> 8) | ((crc & 0xFF000000) >> 24);
}

#if CONFIG_IS_ENABLED(BLOBLIST)
tpi_board_info *setup_bloblist(void) {
  int init_res = bloblist_init();
  if (init_res) {
    printf("bloblist init err 0x%x\n", init_res);
    return NULL;
  }

  void *board_tag =
      bloblist_add(BLOBLISTT_U_BOOT_SPL_HANDOFF, sizeof(tpi_board_info), 0);
  if (!board_tag) {
    printf("no space for board_info in bloblist\n");
  }
  return board_tag;
}
#endif

int tp_board_init(void) {
  u32 cookie = readl(TURING_PI2_BOOT_COOKIE_ADDR);
  if (cookie == TURING_PI2_BOOT_COOKIE_FEL) {
    writel(TURING_PI2_BOOT_COOKIE_WARM, TURING_PI2_BOOT_COOKIE_ADDR);
    // signal FEL
    return 1;
  }

  tpi_board_info *info = NULL;

#if CONFIG_IS_ENABLED(BLOBLIST)
  info = setup_bloblist();
#else
  tpi_board_info stack;
  info = &stack;
#endif

  u16 version = TP_VER(2, 4, 0);
  int result = -ENOENT;
  if (info)
    result = board_info_from_eeprom(info);

  u32 crc = compute_crc(info);
  if (result || crc != info->crc32) {
    printf("Error(%x): invalid board info, defaulting to version 0x%x. crc=%x "
           "expected=%x ver=%x\n",
           result, version, info->crc32, crc, info->hw_version);
    info->hw_version = version;
  } else {
    version = info->hw_version;
  }

  if (cookie != TURING_PI2_BOOT_COOKIE_WARM) {
    turingpi2_ethsw_rst(version);
    init_latches(version);
    writel(TURING_PI2_BOOT_COOKIE_WARM, TURING_PI2_BOOT_COOKIE_ADDR);
  }

#if CONFIG_IS_ENABLED(BLOBLIST)
  bloblist_finish();
#endif
  return 0;
}

#if CONFIG_IS_ENABLED(OF_CONTROL)
/// This method determines which device tree to load. The main
/// difference between 2.4 and 2.5 concerning the bootloader is the difference
/// in flash size. If we load the wrong dt, it can mean booting will fail as
/// the ubi mount cannot be mounted properly. ( due to not all pages being
/// visible of the flash)
int board_fit_config_name_match(const char *name) {
#if CONFIG_IS_ENABLED(BLOBLIST)
  int init_res = bloblist_maybe_init();
  if (init_res != 0) {
    debug("Error initializing bloblist: %d\n", init_res);
    return -ENODATA;
  }

  // Get the blob data
  tpi_board_info *info =
      (tpi_board_info *)bloblist_find(BLOBLISTT_U_BOOT_SPL_HANDOFF, 0);

  if (!info) {
    debug("Error: spl handoff blob not found\n");
    return -ENODATA;
  }

  int v2_4_match = info->hw_version == TP_VER(2, 4, 0) && strstr(name, "-v2.4");
  int v2_5_match = info->hw_version >= TP_VER(2, 5, 0) && strstr(name, "-v2.5");
  if (v2_4_match || v2_5_match) {
    return 0;
  }
  return -EINVAL;
#endif
}
#endif
