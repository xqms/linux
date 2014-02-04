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

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include "clk-pll.h"

static DEFINE_SPINLOCK(clk_lock);

struct rockchip_pll_data {
	int mode_shift;
};

struct rockchip_pll_data rk3066a_apll_data = {
	.mode_shift = 0,
};

struct rockchip_pll_data rk3066a_dpll_data = {
	.mode_shift = 4,
};

struct rockchip_pll_data rk3066a_cpll_data = {
	.mode_shift = 8,
};

struct rockchip_pll_data rk3066a_gpll_data = {
	.mode_shift = 12,
};

/* Matches for plls */
static const __initconst struct of_device_id clk_pll_match[] = {
	{ .compatible = "rockchip,rk3066a-apll", .data = &rk3066a_apll_data },
	{ .compatible = "rockchip,rk3066a-dpll", .data = &rk3066a_dpll_data },
	{ .compatible = "rockchip,rk3066a-cpll", .data = &rk3066a_cpll_data },
	{ .compatible = "rockchip,rk3066a-gpll", .data = &rk3066a_gpll_data },
	{}
};

static void __init rockchip_pll_setup(struct device_node *node,
				      struct rockchip_pll_data *data)
{
	struct clk *clk;
	const char *clk_name = node->name;
	const char *clk_parent;
	void __iomem *reg_base;
	void __iomem *reg_mode;
	u32 rate;

	reg_base = of_iomap(node, 0);
	reg_mode = of_iomap(node, 1);

	clk_parent = of_clk_get_parent_name(node, 0);

	pr_debug("%s: adding %s as child of %s\n",
		__func__, clk_name, clk_parent);

/*	clk = rockchip_clk_register_rk3x_pll(clk_name, clk_parent, reg_base,
					     reg_mode, data->mode_shift,
					     &clk_lock);
	if (clk) {
		of_clk_add_provider(node, of_clk_src_simple_get, clk);

		if (!of_property_read_u32(node, "clock-frequency", &rate))
			clk_set_rate(clk, rate);
	} */
}

static void __init rk3066a_apll_init(struct device_node *node, void *data)
{
	rockchip_pll_setup(node, &rk3066a_apll_data);
}
CLK_OF_DECLARE(rk3066a_apll, "rockchip,rk3066a-apll", rk3066a_apll_init);

static void __init rk3066a_dpll_init(struct device_node *node, void *data)
{
	rockchip_pll_setup(node, &rk3066a_dpll_data);
}
CLK_OF_DECLARE(rk3066a_dpll, "rockchip,rk3066a-dpll", rk3066a_dpll_init);

static void __init rk3066a_cpll_init(struct device_node *node, void *data)
{
	rockchip_pll_setup(node, &rk3066a_cpll_data);
}
CLK_OF_DECLARE(rk3066a_cpll, "rockchip,rk3066a-cpll", rk3066a_cpll_init);

static void __init rk3066a_gpll_init(struct device_node *node, void *data)
{
	rockchip_pll_setup(node, &rk3066a_gpll_data);
}
CLK_OF_DECLARE(rk3066a_gpll, "rockchip,rk3066a-gpll", rk3066a_gpll_init);


/*
 * Gate clocks
 */

static void __init rk2928_gate_clk_init(struct device_node *node,
					 void *data)
{
	struct clk_onecell_data *clk_data;
	const char *clk_parent;
	const char *clk_name;
	void __iomem *reg;
	void __iomem *reg_idx;
	int flags;
	int qty;
	int reg_bit;
	int clkflags = CLK_SET_RATE_PARENT;
	int i;

	qty = of_property_count_strings(node, "clock-output-names");
	if (qty < 0) {
		pr_err("%s: error in clock-output-names %d\n", __func__, qty);
		return;
	}

	if (qty == 0) {
		pr_info("%s: nothing to do\n", __func__);
		return;
	}

	reg = of_iomap(node, 0);

	clk_data = kzalloc(sizeof(struct clk_onecell_data), GFP_KERNEL);
	if (!clk_data)
		return;

	clk_data->clks = kzalloc(qty * sizeof(struct clk *), GFP_KERNEL);
	if (!clk_data->clks) {
		kfree(clk_data);
		return;
	}

	flags = CLK_GATE_HIWORD_MASK | CLK_GATE_SET_TO_DISABLE;

	for (i = 0; i < qty; i++) {
		of_property_read_string_index(node, "clock-output-names",
					      i, &clk_name);

		/* ignore empty slots */
		if (!strcmp("reserved", clk_name))
			continue;

		clk_parent = of_clk_get_parent_name(node, i);

		/* keep all gates untouched for now */
		clkflags |= CLK_IGNORE_UNUSED;

		reg_idx = reg + (4 * (i / 16));
		reg_bit = (i % 16);

		clk_data->clks[i] = clk_register_gate(NULL, clk_name,
						      clk_parent, clkflags,
						      reg_idx, reg_bit,
						      flags,
						      &clk_lock);
		WARN_ON(IS_ERR(clk_data->clks[i]));
	}

	clk_data->clk_num = qty;

	of_clk_add_provider(node, of_clk_src_onecell_get, clk_data);
}
CLK_OF_DECLARE(rk2928_gate, "rockchip,rk2928-gate-clk", rk2928_gate_clk_init);
