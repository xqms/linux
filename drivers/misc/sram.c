/*
 * Generic on-chip SRAM allocation driver
 *
 * Copyright (C) 2012 Philipp Zabel, Pengutronix
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/genalloc.h>

#define SRAM_GRANULARITY	32

struct sram_dev {
	struct gen_pool *pool;
	struct clk *clk;
};

static int sram_probe(struct platform_device *pdev)
{
	void __iomem *virt_base;
	struct sram_dev *sram;
	struct resource *res;
	unsigned long size;
	const __be32 *reserved_list = NULL;
	int reserved_size = 0;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	virt_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(virt_base))
		return PTR_ERR(virt_base);

	size = resource_size(res);

	sram = devm_kzalloc(&pdev->dev, sizeof(*sram), GFP_KERNEL);
	if (!sram)
		return -ENOMEM;

	sram->clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(sram->clk))
		sram->clk = NULL;
	else
		clk_prepare_enable(sram->clk);

	sram->pool = devm_gen_pool_create(&pdev->dev, ilog2(SRAM_GRANULARITY), -1);
	if (!sram->pool)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		reserved_list = of_get_property(pdev->dev.of_node,
						"mmio-sram-reserved",
						&reserved_size);
		if (reserved_list) {
			reserved_size /= sizeof(*reserved_list);
			if (!reserved_size || reserved_size % 2) {
				dev_warn(&pdev->dev, "wrong number of arguments in mmio-sram-reserved\n");
				reserved_list = NULL;
			}
		}
	}

	if (!reserved_list) {
		ret = gen_pool_add_virt(sram->pool, (unsigned long)virt_base,
					res->start, size, -1);
		if (ret < 0) {
			if (sram->clk)
				clk_disable_unprepare(sram->clk);
			return ret;
		}
	} else {
		unsigned int cur_start = 0;
		unsigned int cur_size;
		unsigned int rstart;
		unsigned int rsize;
		int i;

		for (i = 0; i < reserved_size; i += 2) {
			/* get the next reserved block */
			rstart = be32_to_cpu(*reserved_list++);
			rsize = be32_to_cpu(*reserved_list++);

			/* catch unsorted list entries */
			if (rstart < cur_start) {
				dev_err(&pdev->dev, "unsorted reserved list (0x%x before current 0x%x)\n",
					rstart, cur_start);
				if (sram->clk)
					clk_disable_unprepare(sram->clk);
				return -EINVAL;
			}

			dev_dbg(&pdev->dev, "found reserved block 0x%x-0x%x\n",
				 rstart, rstart + rsize);

			/* current start is in a reserved block */
			if (rstart <= cur_start) {
				cur_start = rstart + rsize;
				continue;
			}

			/*
			 * allocate the space between the current starting
			 * address and the following reserved block
			 */
			cur_size = rstart - cur_start;

			dev_dbg(&pdev->dev, "adding chunk 0x%x-0x%x\n",
				 cur_start, cur_start + cur_size);
			ret = gen_pool_add_virt(sram->pool,
					(unsigned long)virt_base + cur_start,
					res->start + cur_start, cur_size, -1);
			if (ret < 0) {
				if (sram->clk)
					clk_disable_unprepare(sram->clk);
				return ret;
			}

			/* next allocation after this reserved block */
			cur_start = rstart + rsize;
		}

		/* allocate the space after the last reserved block */
		if (cur_start < size) {
			cur_size = size - cur_start;

			dev_dbg(&pdev->dev, "adding chunk 0x%x-0x%x\n",
				 cur_start, cur_start + cur_size);
			ret = gen_pool_add_virt(sram->pool,
					(unsigned long)virt_base + cur_start,
					res->start + cur_start, cur_size, -1);
		}
	}

	platform_set_drvdata(pdev, sram);

	dev_dbg(&pdev->dev, "SRAM pool: %ld KiB @ 0x%p\n", size / 1024, virt_base);

	return 0;
}

static int sram_remove(struct platform_device *pdev)
{
	struct sram_dev *sram = platform_get_drvdata(pdev);

	if (gen_pool_avail(sram->pool) < gen_pool_size(sram->pool))
		dev_dbg(&pdev->dev, "removed while SRAM allocated\n");

	gen_pool_destroy(sram->pool);

	if (sram->clk)
		clk_disable_unprepare(sram->clk);

	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id sram_dt_ids[] = {
	{ .compatible = "mmio-sram" },
	{}
};
#endif

static struct platform_driver sram_driver = {
	.driver = {
		.name = "sram",
		.of_match_table = of_match_ptr(sram_dt_ids),
	},
	.probe = sram_probe,
	.remove = sram_remove,
};

static int __init sram_init(void)
{
	return platform_driver_register(&sram_driver);
}

postcore_initcall(sram_init);
