/*
 *  linux/drivers/clk/mmp/simple_dip.c
 *
 *  Xiaoguang Chen <chenxg@marvell.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/pm_qos.h>
#include <linux/notifier.h>
#include <linux/devfreq.h>
#include <linux/of.h>
#include <linux/platform_data/devfreq-pxa.h>
#include <asm-generic/uaccess.h>

#define DRV_NAME	"simple_dip"
#define MISCDEV_MINOR	100
#define MAX_NUM		10
#define CLK_CPU		"cpu"
#define CLK_DDR		"ddr"

#define	DIP_START	1
#define	DIP_END		0

enum {
	PXA1088,
	PXA1L88,
	PXA1928,
	PXA1U88,
};

enum comp {
	CPU,
	DDR,
	LCD,
};

struct comp_info {
	int comp_id;
	unsigned int max_freq;
	unsigned int min_freq;
};

struct comp_info_array {
	unsigned int comp_num;
	struct comp_info *comp_info;
};

struct dip_info {
	struct miscdevice misc_dev;
	struct comp_info_array comp_info_array;
	struct clk *cpu;
	struct clk *ddr;
	struct pm_qos_request cpu_min_qos;
	struct pm_qos_request cpu_max_qos;
	struct pm_qos_request ddr_min_qos;
	struct pm_qos_request ddr_max_qos;
};

static struct comp_info comp_info_temp[MAX_NUM];

static BLOCKING_NOTIFIER_HEAD(dip_notifier_list);
static struct dip_info *dip_info;
static unsigned int plat;

static int dip_notifier(unsigned int state, void *val)
{
	return blocking_notifier_call_chain(&dip_notifier_list, state, val);
}

int dip_register_notifier(struct notifier_block *nb, unsigned int list)
{
	return blocking_notifier_chain_register(&dip_notifier_list, nb);
}
EXPORT_SYMBOL(dip_register_notifier);

static long dip_misc_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int i;
	struct comp_info_array temp_array;
	unsigned int rate = 0;

	struct comp_info_array *array = &dip_info->comp_info_array;
	if (cmd == DIP_START) {
		if (copy_from_user
		    (&temp_array, argp, sizeof(struct comp_info_array)))
			return -EFAULT;

		if (MAX_NUM < temp_array.comp_num)
			temp_array.comp_num = MAX_NUM;

		array->comp_num = temp_array.comp_num;

		if (copy_from_user(array->comp_info, temp_array.comp_info,
				   sizeof(struct comp_info) * array->comp_num))
			return -EFAULT;

		for (i = 0; i < array->comp_num; i++) {
			if (array->comp_info[i].comp_id == CPU) {
				if (PXA1928 != plat) {
					pm_qos_update_request(&dip_info->cpu_min_qos,
							array->comp_info[i].
							min_freq);
					pm_qos_update_request(&dip_info->cpu_max_qos,
							array->comp_info[i].
							max_freq);
				}
			} else if (array->comp_info[i].comp_id == DDR) {
				if (PXA1928 != plat) {
					pm_qos_update_request(&dip_info->ddr_min_qos,
							array->comp_info[i].
							min_freq);
					pm_qos_update_request(&dip_info->ddr_max_qos,
							array->comp_info[i].
							max_freq);
				}
			} else if (array->comp_info[i].comp_id == LCD) {
				rate = array->comp_info[i].min_freq;
			}
		}
		dip_notifier(cmd, &rate);
		return 0;
	} else if (cmd == DIP_END) {
		dip_notifier(cmd, &rate);
		if (PXA1928 != plat) {
			pm_qos_update_request(&dip_info->cpu_min_qos,
					PM_QOS_DEFAULT_VALUE);
			pm_qos_update_request(&dip_info->cpu_max_qos, INT_MAX);
			pm_qos_update_request(&dip_info->ddr_min_qos,
					PM_QOS_DEFAULT_VALUE);
			pm_qos_update_request(&dip_info->ddr_max_qos, INT_MAX);
		}
		return 0;
	}

	return -ENOSYS;
}

static const struct file_operations simple_dip_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = dip_misc_ioctl,
};

static const struct of_device_id simple_dip_dt_match[] = {
	{.compatible = "marvell,simple-dip" },
	{},
};
MODULE_DEVICE_TABLE(of, simple_dip_dt_match);

static int simple_dip_probe(struct platform_device *dev)
{
	int ret = 0;
	struct device_node *np = dev->dev.of_node;

	dip_info = kzalloc(sizeof(struct dip_info), GFP_KERNEL);
	if (!dip_info)
		return -ENOMEM;

	of_property_read_u32(np, "plat", &plat);
	dip_info->misc_dev.minor = MISCDEV_MINOR;
	dip_info->misc_dev.name = DRV_NAME;
	dip_info->misc_dev.fops = &simple_dip_fops;
	dip_info->comp_info_array.comp_info = comp_info_temp;
	dip_info->cpu = clk_get(NULL, CLK_CPU);
	if (IS_ERR(dip_info->cpu)) {
		pr_err("cannot get clk(cpu)\n");
		goto err_free_dip;
	}

	/*
	 * FIXME: PXA1928 do not need to set constraint because
	 * PLL3 is dedicated for LCD, not for CPU or DDR.but pxa1L88's
	 * PLL3 is shared to LCD&DDR, so it need constraint.
	 */
	if (PXA1928 != plat) {
		dip_info->cpu_min_qos.name = "dip_cpu_min";
		pm_qos_add_request(&dip_info->cpu_min_qos, PM_QOS_CPUFREQ_MIN,
				PM_QOS_DEFAULT_VALUE);
		dip_info->cpu_max_qos.name = "dip_cpu_max";
		pm_qos_add_request(&dip_info->cpu_max_qos, PM_QOS_CPUFREQ_MAX,
				INT_MAX);

		dip_info->ddr = clk_get(NULL, CLK_DDR);
		if (IS_ERR(dip_info->ddr)) {
			pr_err("cannot get clk(ddr)\n");
			goto err_free_dip;
		}
		dip_info->ddr_min_qos.name = "dip_ddr_min";
		pm_qos_add_request(&dip_info->ddr_min_qos, PM_QOS_DDR_DEVFREQ_MIN,
				PM_QOS_DEFAULT_VALUE);
		dip_info->ddr_max_qos.name = "dip_ddr_max";
		pm_qos_add_request(&dip_info->ddr_max_qos, PM_QOS_DDR_DEVFREQ_MAX,
				INT_MAX);
	}
	/* register misc device */
	ret = misc_register(&dip_info->misc_dev);
	if (ret < 0) {
		dev_err(&dev->dev, "misc_register() for minor %d failed\n",
			MISCDEV_MINOR);
		goto err_free_dip;
	}
	platform_set_drvdata(dev, dip_info);
	return 0;

err_free_dip:
	platform_set_drvdata(dev, NULL);
	kfree(dip_info);
	return ret;
}

static int simple_dip_remove(struct platform_device *dev)
{
	struct dip_info *dip_info = platform_get_drvdata(dev);

	misc_deregister(&dip_info->misc_dev);
	platform_set_drvdata(dev, NULL);
	kfree(dip_info);

	return 0;
}

static struct platform_driver simple_dip_driver = {
	.probe = simple_dip_probe,
	.remove = simple_dip_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = DRV_NAME,
		   .of_match_table = of_match_ptr(simple_dip_dt_match),
		   },
};

module_platform_driver(simple_dip_driver);

MODULE_DESCRIPTION("Simple DIP driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS_MISCDEV(MISCDEV_MINOR);
MODULE_AUTHOR("Xiaoguang Chen <chenxg@marvell.com>");
