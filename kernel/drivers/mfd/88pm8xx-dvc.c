/*
 * Regulators driver for Marvell 88PM800
 *
 * Copyright (C) 2012 Marvell International Ltd.
 * Joseph(Yossi) Hanin <yhanin@marvell.com>
 * Yi Zhang <yizhang@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/88pm80x.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/ctype.h>

/* BUCK1/4 with DVC[0..3] */
#define PM8xx_BUCK1		(0x3C)
#define PM8xx_BUCK3		(0x41)

#define PM8xx_WAKEUP1		(0x0D)
#define PM8xx_DVC_EN		(1 << 7)

#define BUCK_MIN		600000
#define BUCK_MAX		1587500
#define BUCK_STEP		12500
#define BUCK_MAX_DVC_LVL	4
#define BUCK_MAX_DVC_LVL_16	16
#define DVC_CONTROL_REG		0x4F
#define DVC_SET_ADDR1		(1 << 0)
#define DVC_SET_ADDR2		(1 << 1)

struct pm8xx_dvc {
	struct platform_device *pdev;
	struct regmap *powermap;
	struct regmap *basemap;
	/* to support multi pmic, they have different affected buck */
	u32 affectedbuck;
};

static struct pm8xx_dvc *pm8xx_dvcdata;

static inline int volt_to_reg(int uv)
{
	return (uv - BUCK_MIN) / BUCK_STEP;
}

static inline int reg_to_volt(int regval)
{
	return regval * BUCK_STEP + BUCK_MIN;
}

static int getchip_id(void)
{
	int val, chip_id, ret = 0;

	ret = regmap_read(pm8xx_dvcdata->basemap, 0x0, &val);
	chip_id = val & 0xff;

	return chip_id;
}

int set_dvc_control_register(unsigned int lvl)
{
	int control_lvl, ret;
	struct regmap *regmap = pm8xx_dvcdata->powermap;

	control_lvl = lvl / 4;
	ret = regmap_update_bits(regmap, DVC_CONTROL_REG,
				DVC_SET_ADDR1 | DVC_SET_ADDR2, control_lvl);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * Example for usage: set buck1 vl1 as 1200mV
 * pm8xx_dvc_setvolt(PM800_ID_BUCK1, 1, 1200 * mV2uV);
 */
int pm8xx_dvc_setvolt(unsigned int buckid, unsigned int lvl, int uv)
{
	struct platform_device *pdev = pm8xx_dvcdata->pdev;
	struct regmap *regmap = pm8xx_dvcdata->powermap;
	int ret = 0, idx;
	u32 affectbuckbits = pm8xx_dvcdata->affectedbuck;
	u32 regbase = PM8xx_BUCK1;
	int chip_id;

	/* buck1 start at 0 */
	if (!(affectbuckbits & (1 << buckid))) {
		dev_err(&pdev->dev, "unsupported buck%d in DVC\n", buckid);
		return -EINVAL;
	}

	if (uv < BUCK_MIN || uv > BUCK_MAX) {
		dev_err(&pdev->dev, "Failed to allocate pm8xx_dvcdata\n");
		return -EINVAL;
	}
	/*check chip id, 88pm860 support more dvc level than other pmic chip*/
	chip_id = getchip_id();

	if (chip_id == CHIP_PM86X_ID_Z3 || chip_id == CHIP_PM86X_ID_A0) {
		/*88pm860 support 16 level dvc*/
		if (lvl >= BUCK_MAX_DVC_LVL_16) {
			dev_err(&pdev->dev, "PM860 DVC lvl out of range\n");
			return -EINVAL;
		}
		/*88pm860 have two DVC control register
		combine with four level registers to support 16 level voltage.
		 need to set the DVC control register*/
		set_dvc_control_register(lvl);

	} else  /*other pmic chip: 800 and 820 support 4 level dvc*/
		if (lvl >= BUCK_MAX_DVC_LVL) {
			dev_err(&pdev->dev, "DVC lvl out of range\n");
			return -EINVAL;
		}

	/*this change is for 88pm860 pmic 16 level dvc support*/
	lvl = lvl % BUCK_MAX_DVC_LVL;

	for (idx = 0; idx < buckid; idx++) {
		if ((affectbuckbits >> idx) & 0x1)
			regbase += BUCK_MAX_DVC_LVL;
		else
			regbase += 1;
	}
	ret = regmap_write(regmap, regbase + lvl, volt_to_reg(uv));
	return ret;
};
EXPORT_SYMBOL(pm8xx_dvc_setvolt);

int pm8xx_dvc_getvolt(unsigned int buckid, unsigned int lvl, int *uv)
{
	struct platform_device *pdev = pm8xx_dvcdata->pdev;
	struct regmap *regmap = pm8xx_dvcdata->powermap;
	int ret = 0, regval = 0, idx;
	u32 affectbuckbits = pm8xx_dvcdata->affectedbuck;
	u32 regbase = PM8xx_BUCK1;
	int chip_id;

	*uv = 0;
	if (!(affectbuckbits & (1 << buckid))) {
		dev_err(&pdev->dev, "unsupported buck%d in DVC\n", buckid);
		return -EINVAL;
	}

	/* check chip id, 88pm860 support more dvc level than other pmic chip */
	chip_id = getchip_id();
	if (chip_id == CHIP_PM86X_ID_Z3 || chip_id == CHIP_PM86X_ID_A0) {
		/* 88pm860 support 16 level dvc */
		if (lvl >= BUCK_MAX_DVC_LVL_16) {
			dev_err(&pdev->dev, "PM860 DVC lvl out of range\n");
			return -EINVAL;
		}
		/*
		 * 88pm860 have two DVC control register
		 * combine with four level registers to support 16 level voltage.
		 * need to set the DVC control register
		 */
		set_dvc_control_register(lvl);

	} else  /* other pmic chip: 800 and 820 support 4 level dvc */
		if (lvl >= BUCK_MAX_DVC_LVL) {
			dev_err(&pdev->dev, "DVC lvl out of range\n");
			return -EINVAL;
		}


	/* this change is for 88pm860 pmic 16 level dvc support */
	lvl = lvl % BUCK_MAX_DVC_LVL;

	for (idx = 0; idx < buckid; idx++) {
		if ((affectbuckbits >> idx) & 0x1)
			regbase += BUCK_MAX_DVC_LVL;
		else
			regbase += 1;
	}
	ret = regmap_read(regmap, regbase + lvl, &regval);
	if (!ret)
		*uv = reg_to_volt(regval);
	return ret;
}
EXPORT_SYMBOL(pm8xx_dvc_getvolt);

/*
 * buck1_info usage:
 * 1. read buck1 voltage
 * >> echo +[level] > /sys/kernel/debug/buck1_info
 * >> cat /sys/kernel/debug/buck1_info
 * 2. set buck1 voltage
 * >> echo +[level] [voltage] > /sys/kernel/debug/buck1_info
 */
static int buck1_dvc_lvl;
static ssize_t pm8xx_buck1_info_read(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	int ret, uv, len = 0;
	char str[100];

	if (buck1_dvc_lvl < 0 || buck1_dvc_lvl > 16) {
		pr_err("dvc level error\n");
		return -EINVAL;
	}

	ret = pm8xx_dvc_getvolt(PM800_ID_BUCK1, buck1_dvc_lvl, &uv);

	if (ret < 0) {
		pr_err("get buck1 dvc level %d error\n", buck1_dvc_lvl);
		return -EINVAL;
	} else
		len = snprintf(str, sizeof(str),
				"buck 1, level %d, voltage %uV\n", buck1_dvc_lvl, uv);

	return simple_read_from_buffer(user_buf, count, ppos, str, len);
}

static ssize_t pm8xx_buck1_info_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	int ret, volt;

	ret = sscanf(user_buf, "+%d %d\n", &buck1_dvc_lvl, &volt);
	if (ret < 1) {
		pr_err("Right format: +[level] [voltage]\n");
		return -EINVAL;
	}

	if (ret == 2)
		pm8xx_dvc_setvolt(PM800_ID_BUCK1, buck1_dvc_lvl, volt);

	pm8xx_dvc_getvolt(PM800_ID_BUCK1, buck1_dvc_lvl, &volt);
	pr_info("buck 1, level %d, voltage %duV\n", buck1_dvc_lvl, volt);

	return count;
}

static const struct file_operations pm8xx_buck1_info_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= pm8xx_buck1_info_read,
	.write		= pm8xx_buck1_info_write,
};

int pm8xx_dvc_buck3_getvolt(int lvl, u32 *uv)
{
	struct platform_device *pdev = pm8xx_dvcdata->pdev;
	struct regmap *regmap = pm8xx_dvcdata->powermap;

	int ret, val;
	int chip_id;
	u32 regbase = PM8xx_BUCK3;

	chip_id = getchip_id();
	if (chip_id == CHIP_PM86X_ID_Z3 || chip_id == CHIP_PM86X_ID_A0) {
		if (lvl < 0 || lvl >= BUCK_MAX_DVC_LVL_16) {
			dev_err(&pdev->dev, "PM860 DVC lvl out of range\n");
			return -EINVAL;
		}

		set_dvc_control_register(lvl);
		lvl = lvl % BUCK_MAX_DVC_LVL;
		ret = regmap_read(regmap, regbase + lvl, &val);

		if (!ret) {
			val &= 0x7f;
			*uv = reg_to_volt(val);
		}
	} else {
		dev_err(&pdev->dev, "unsupported PMIC chip\n");
		return 0;
	}


	return ret;
}
EXPORT_SYMBOL(pm8xx_dvc_buck3_getvolt);

/*
 * Example for usage: set 1200mv
 * pm8xx_dvc_buck3_setvolt(1200000);
 */
int pm8xx_dvc_buck3_setvolt(u32 uv)
{
	struct platform_device *pdev = pm8xx_dvcdata->pdev;
	struct regmap *regmap = pm8xx_dvcdata->powermap;

	int ret = 0, i, control_lvl;
	u32 regbase = PM8xx_BUCK3;
	int chip_id;

	if (uv < BUCK_MIN || uv > BUCK_MAX) {
		pr_err("voltage is out of range\n");
		return -EINVAL;
	}

	chip_id = getchip_id();

	if (chip_id == CHIP_PM86X_ID_Z3 || chip_id == CHIP_PM86X_ID_A0) {
		for (control_lvl = 0; control_lvl < 4; control_lvl++) {
			ret = regmap_update_bits(regmap, DVC_CONTROL_REG,
						DVC_SET_ADDR1 | DVC_SET_ADDR2,
						control_lvl);
			if (ret < 0)
				return ret;

			for (i = 0; i < 4; i++) {
				ret = regmap_update_bits(regmap, regbase + i,
							0x7f, volt_to_reg(uv));
				if (ret < 0)
					return ret;
			}
		}
	} else {
		dev_err(&pdev->dev, "unsupported PMIC chip\n");
		return 0;
	}

	return ret;
}
EXPORT_SYMBOL(pm8xx_dvc_buck3_setvolt);

/*
 * buck3_info usage:
 * 1. read buck3 voltage
 * $ cat <debugfs>/buck3_info
 * 2. set buck3 voltage, for example, to 1200mV
 * $ echo 1200000 > <debugfs>/buck3_info
 */
static ssize_t pm8xx_buck3_info_read(struct file *file, char __user *user_buf,
				size_t count, loff_t *ppos)
{
	int ret, uv, i, len = 0;
	char str[1000];

	for (i = 0; i < 16; i++) {
		ret = pm8xx_dvc_buck3_getvolt(i, &uv);
		if (ret < 0) {
			return -EINVAL;
		} else {
			len += snprintf(str + len, sizeof(str),
					"buck 3, level %d, voltage %duV\n", i, uv);
		}
	}

	return simple_read_from_buffer(user_buf, count, ppos, str, len);
}

static ssize_t pm8xx_buck3_info_write(struct file *file, const char __user *user_buf,
				size_t count, loff_t *ppos)
{
	int ret, volt;
	ret = sscanf(user_buf, "%d\n", &volt);
	if (ret < 1) {
		pr_err("Right format: echo [voltage] > buck3_info\n");
		return -EINVAL;
	}

	pm8xx_dvc_buck3_setvolt(volt);
	pr_info("buck 3, voltage: %duV\n", volt);

	return count;
}

static const struct file_operations pm8xx_buck3_info_ops = {
	.owner		= THIS_MODULE,
	.open		= simple_open,
	.read		= pm8xx_buck3_info_read,
	.write		= pm8xx_buck3_info_write,
};

static int pm8xx_dvc_probe(struct platform_device *pdev)
{
	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm8xx_dvc *dvcdata;
	struct dentry *pm8xx_dvc_buck1, *pm8xx_dvc_buck3;
	int ret = 0, val = 0;

	if (pm8xx_dvcdata)
		return 0;

	dvcdata = devm_kzalloc(&pdev->dev, sizeof(*pm8xx_dvcdata), GFP_KERNEL);
	if (!dvcdata) {
		dev_err(&pdev->dev, "Failed to allocate pm8xx_dvcdata");
		return -ENOMEM;
	}
	dvcdata->basemap = chip->regmap;
	dvcdata->powermap = chip->subchip->regmap_power;
	dvcdata->pdev = pdev;
	platform_set_drvdata(pdev, dvcdata);
	pm8xx_dvcdata = dvcdata;

	/* get pmic affected buck 32bit */
	if (IS_ENABLED(CONFIG_OF)) {
		if (of_property_read_u32(pdev->dev.of_node,
					 "dvc-affected-buckbits",
					 &dvcdata->affectedbuck))
			dev_dbg(&pdev->dev, "Failed to get affectedbuckbits\n");
	} else {
		/* by default only enable buck1 */
		dvcdata->affectedbuck = 0x1;
	}

	/* enable PMIC dvc feature */
	ret = regmap_update_bits(chip->regmap, PM8xx_WAKEUP1,
				 PM8xx_DVC_EN, PM8xx_DVC_EN);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to enable pmic dvc feature!\n");
		devm_kfree(&pdev->dev, dvcdata);
		return ret;
	}
	regmap_read(chip->regmap, PM8xx_WAKEUP1, &val);
	dev_info(&pdev->dev, "DVC power hold enabled %x! DVC buck %x\n",
		 val, dvcdata->affectedbuck);

	pm8xx_dvc_buck1 = debugfs_create_file("buck1_info", S_IRUGO | S_IFREG,
				NULL, NULL, &pm8xx_buck1_info_ops);
	if (!pm8xx_dvc_buck1) {
		pr_err("create buck1_info debugfs error!\n");
		return -ENOENT;
	} else if (pm8xx_dvc_buck1 == ERR_PTR(-ENODEV)) {
		pr_err("CONFIG_DEBUG_FS is not enabled\n");
		return -ENOENT;
	}

	pm8xx_dvc_buck3 = debugfs_create_file("buck3_info", S_IRUGO | S_IFREG,
				NULL, NULL, &pm8xx_buck3_info_ops);
	if (!pm8xx_dvc_buck3) {
		pr_err("create buck3_info debugfs error!\n");
		return -ENOENT;
	} else if (pm8xx_dvc_buck3 == ERR_PTR(-ENODEV)) {
		pr_err("CONFIG_DEBUG_FS is not enabled\n");
		return -ENOENT;
	}

	return 0;
}

static int pm8xx_dvc_remove(struct platform_device *pdev)
{
	struct pm8xx_dvc *dvcdata =
	    (struct pm8xx_dvc *)platform_get_drvdata(pdev);
	devm_kfree(&pdev->dev, dvcdata);
	return 0;
}

static struct of_device_id pm8xx_dvc_of_match[] = {
	{.compatible = "marvell,88pm8xx-dvc"},
	/* add other pmic dvc device name here */
	{},
};

MODULE_DEVICE_TABLE(of, pm8xx_dvc_of_match);

static struct platform_driver pm8xx_dvc_driver = {
	.driver = {
		   .name = "88pm8xx-dvc",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(pm8xx_dvc_of_match),
		   },
	.probe = pm8xx_dvc_probe,
	.remove = pm8xx_dvc_remove,
};

static int __init pm8xx_dvc_init(void)
{
	return platform_driver_register(&pm8xx_dvc_driver);
}

subsys_initcall(pm8xx_dvc_init);

static void __exit pm8xx_dvc_exit(void)
{
	platform_driver_unregister(&pm8xx_dvc_driver);
}

module_exit(pm8xx_dvc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DVC Driver for Marvell 88PM8xx PMIC");
MODULE_ALIAS("platform:88pm8xx-dvc");
