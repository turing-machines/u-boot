// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 Sam Edwards <CFSworks@gmail.com>
 * Copyright (C) 2024 Sven Rademakers <sven@turingpi.com>
 *
 * Early init for the Turing Pi 2 clusterboard.
 */

#include <common.h>
#include <asm/gpio.h>
#include <init.h>
#include <sunxi_gpio.h>

static void turingpi2_ethsw_rst(void)
{
	/*
	 * RTL8370MB switch reset (active-low) on PG13 should be asserted very
	 * early in boot to prevent Ethernet from coming up until the switch
	 * can be configured. The user may be using the Turing Pi 2 as a router
	 * or some other kind of network isolation device.
	 */
	gpio_direction_output(SUNXI_GPG(13), 0);
}

int board_early_init_f(void)
{
	turingpi2_ethsw_rst();

    u32 cookie = readl(TURING_PI2_BOOT_COOKIE_ADDR);
    if (cookie == TURING_PI2_BOOT_COOKIE_FEL) {
        writel(TURING_PI2_BOOT_COOKIE_WARM, TURING_PI2_BOOT_COOKIE_ADDR);
        // signal FEL
        return 1;
    }
    return 0;
}
