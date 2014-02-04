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

struct sram_reserve {
	u32 start;
	u32 size;
};

static int sram_probe(struct platform_device *pdev)
{
	void __iomem *virt_base;
	struct sram_dev *sram;
	struct resource *res;
	struct device_node *np = pdev->dev.of_node;
	unsigned long size, cur_start, cur_size;
	int reserved_size = 0;
	struct sram_reserve *rblocks;
	unsigned int nblocks;
	int ret = 0;
	int i;

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

	if (np) {
		reserved_size = of_property_count_u32_elems(np,
							"mmio-sram-reserved");

		/* mmio-sram-reserved is optional, so its absence is no error */
		if (reserved_size < 0)
			reserved_size = 0;

		if (reserved_size % 2) {
			dev_warn(&pdev->dev, "wrong number of arguments in mmio-sram-reserved\n");
			reserved_size = 0;
		}
	}

	/*
	 * We need an additional block to mark the end of the memory region
	 * after the reserved blocks from the dt are processed.
	 */
	nblocks = reserved_size / 2 + 1;
	rblocks = kmalloc((nblocks) * sizeof(*rblocks), GFP_KERNEL);
	if (!rblocks) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	cur_start = 0;
	for (i = 0; i < nblocks - 1; i++) {
		/* checked the property exists above so can skip return vals */
		of_property_read_u32_index(np, "mmio-sram-reserved",
					   2 * i, &rblocks[i].start);
		of_property_read_u32_index(np, "mmio-sram-reserved",
					   2 * i + 1, &rblocks[i].size);

		if (rblocks[i].start < cur_start) {
			dev_err(&pdev->dev,
				"unsorted reserved list (0x%x before current 0x%lx)\n",
				rblocks[i].start, cur_start);
			ret = -EINVAL;
			goto err_chunks;
		}

		if (rblocks[i].start >= size) {
			dev_err(&pdev->dev,
				"reserved block at 0x%x outside the sram size 0x%lx\n",
				rblocks[i].start, size);
			ret = -EINVAL;
			goto err_chunks;
		}

		if (rblocks[i].start + rblocks[i].size > size) {
			dev_warn(&pdev->dev,
				 "reserved block at 0x%x to large, trimming\n",
				 rblocks[i].start);
			rblocks[i].size = size - rblocks[i].start;
		}

		cur_start = rblocks[i].start + rblocks[i].size;

		dev_dbg(&pdev->dev, "found reserved block 0x%x-0x%x\n",
			rblocks[i].start,
			rblocks[i].start + rblocks[i].size);
	}

	/* the last chunk marks the end of the region */
	rblocks[nblocks - 1].start = size;
	rblocks[nblocks - 1].size = 0;

	cur_start = 0;
	for (i = 0; i < nblocks; i++) {
		/* current start is in a reserved block, so continue after it */
		if (rblocks[i].start == cur_start) {
			cur_start = rblocks[i].start + rblocks[i].size;
			continue;
		}

		/*
		 * allocate the space between the current starting
		 * address and the following reserved block, or the
		 * end of the region.
		 */
		cur_size = rblocks[i].start - cur_start;

		dev_dbg(&pdev->dev, "adding chunk 0x%lx-0x%lx\n",
			cur_start, cur_start + cur_size);
		ret = gen_pool_add_virt(sram->pool,
				(unsigned long)virt_base + cur_start,
				res->start + cur_start, cur_size, -1);
		if (ret < 0)
			goto err_chunks;

		/* next allocation after this reserved block */
		cur_start = rblocks[i].start + rblocks[i].size;
	}

	kfree(rblocks);

	platform_set_drvdata(pdev, sram);

	dev_dbg(&pdev->dev, "SRAM pool: %ld KiB @ 0x%p\n", size / 1024, virt_base);

	return 0;

err_chunks:
	kfree(rblocks);
err_alloc:
	if (sram->clk)
		clk_disable_unprepare(sram->clk);
	return ret;
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
