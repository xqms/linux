/*
 * Device Tree support for Rockchip SoCs
 *
 * Copyright (c) 2013 MundoReader S.L.
 * Author: Heiko Stuebner <heiko@sntech.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/of_platform.h>
#include <linux/irqchip.h>
#include <linux/clocksource.h>
#include <linux/clk-provider.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/hardware/cache-l2x0.h>
#include "core.h"

static const char * const rockchip_board_dt_compat[] = {
	"rockchip,rk2928",
	"rockchip,rk3066a",
	"rockchip,rk3066b",
	"rockchip,rk3188",
	NULL,
};

static void __init rockchip_timer_init(void)
{
	of_clk_init(NULL);
	clocksource_of_init();
}

static void __init rockchip_dt_init(void)
{
	rockchip_clocks_apply_init_table();
	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
}

DT_MACHINE_START(ROCKCHIP_DT, "Rockchip Cortex-A9 (Device Tree)")
	.l2c_aux_val	= 0,
	.l2c_aux_mask	= ~0,
	.dt_compat	= rockchip_board_dt_compat,
	.init_time	= rockchip_timer_init,
	.init_machine	= rockchip_dt_init,
MACHINE_END
