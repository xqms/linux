/*
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

#include <asm/div64.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/clk-private.h>

#define RK3X_PLL_MODE_MASK		0x3
#define RK3X_PLL_MODE_SLOW		0x0
#define RK3X_PLL_MODE_NORM		0x1
#define RK3X_PLL_MODE_DEEP		0x2

#define RK3X_PLLCON0_OD_MASK		0xf
#define RK3X_PLLCON0_OD_SHIFT		0
#define RK3X_PLLCON0_NR_MASK		0x3f
#define RK3X_PLLCON0_NR_SHIFT		8

#define RK3X_PLLCON1_NF_MASK		0x1fff
#define RK3X_PLLCON1_NF_SHIFT		0

#define RK3X_PLLCON3_REST		(1 << 5)
#define RK3X_PLLCON3_BYPASS		(1 << 0)

struct rockchip_clk_pll {
	struct clk_hw		hw;
	void __iomem		*reg_base;
	void __iomem		*reg_mode;
	unsigned int		shift_mode;
	spinlock_t		*lock;
};

#define to_clk_pll(_hw) container_of(_hw, struct rockchip_clk_pll, hw)

static unsigned long rk3x_generic_pll_recalc_rate(struct clk_hw *hw,
				unsigned long parent_rate)
{
	struct rockchip_clk_pll *pll = to_clk_pll(hw);
	u32 pll_con0 = readl_relaxed(pll->reg_base);
	u32 pll_con1 = readl_relaxed(pll->reg_base + 0x4);
	u32 pll_con3 = readl_relaxed(pll->reg_base + 0xc);
	u32 mode_con = readl_relaxed(pll->reg_mode) >> pll->shift_mode;
	u64 pll_nf;
	u64 pll_nr;
	u64 pll_no;
	u64 rate64;

	if (pll_con3 & RK3X_PLLCON3_BYPASS) {
		pr_debug("%s: pll %s is bypassed\n", __func__,
			__clk_get_name(hw->clk));
		return parent_rate;
	}

	mode_con &= RK3X_PLL_MODE_MASK;
	if (mode_con != RK3X_PLL_MODE_NORM) {
		pr_debug("%s: pll %s not in normal mode: %d\n", __func__,
			__clk_get_name(hw->clk), mode_con);
		return parent_rate;
	}

	pll_nf = (pll_con1 >> RK3X_PLLCON1_NF_SHIFT);
	pll_nf &= RK3X_PLLCON1_NF_MASK;
	pll_nf++;
	rate64 = (u64)parent_rate * pll_nf;

	pll_nr = (pll_con0 >> RK3X_PLLCON0_NR_SHIFT);
	pll_nr &= RK3X_PLLCON0_NR_MASK;
	pll_nr++;
	do_div(rate64, pll_nr);

	pll_no = (pll_con0 >> RK3X_PLLCON0_OD_SHIFT);
	pll_no &= RK3X_PLLCON0_OD_MASK;
	pll_no++;
	do_div(rate64, pll_no);

	return (unsigned long)rate64;
}

static const struct clk_ops rk3x_generic_pll_clk_ops = {
	.recalc_rate = rk3x_generic_pll_recalc_rate,
};

struct clk * __init rockchip_clk_register_rk3x_pll(const char *name,
			const char *pname, void __iomem *reg_base,
			void __iomem *reg_mode, unsigned int shift_mode,
			spinlock_t *lock)
{
	struct rockchip_clk_pll *pll;
	struct clk *clk;
	struct clk_init_data init;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll) {
		pr_err("%s: could not allocate pll clk %s\n", __func__, name);
		return NULL;
	}

	init.name = name;
	init.ops = &rk3x_generic_pll_clk_ops;
	init.flags = CLK_GET_RATE_NOCACHE;
	init.parent_names = &pname;
	init.num_parents = 1;

	pll->hw.init = &init;
	pll->reg_base = reg_base;
	pll->reg_mode = reg_mode;
	pll->shift_mode = shift_mode;
	pll->lock = lock;

	clk = clk_register(NULL, &pll->hw);
	if (IS_ERR(clk)) {
		pr_err("%s: failed to register pll clock %s\n", __func__,
				name);
		kfree(pll);
	}

	return clk;
}
