/*
 * devfreq-vpu: Generic Dynamic Voltage and Frequency Scaling (DVFS) Framework
 *		  for vpu Device.
 *
 * Copyright (C) 2010 Marvell International Ltd.
 *	Xiaoguang Chen <chenxg@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/clk.h>
#include <linux/devfreq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/devfreq.h>
#include <linux/platform_data/devfreq-pxa.h>
#include <linux/clk-private.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/time.h>
#include <linux/mutex.h>
#include <linux/pm_qos.h>

#define VPU_FREQ_MAX	8
#define KHZ_TO_HZ	1000

#define DEFAULT_POLLING_MS	(2000)
#define VPU_DEVFREQ_UPTHRESHOLD	85
#define VPU_DEVFREQ_DOWNDIFFERENTIAL	2

DEFINE_MUTEX(vpu_pw_fc_lock);

enum vpu_status_msg {
	VPU_DC_START = 0,
	VPU_DC_STOP,
	VPU_CLK_ON,
	VPU_CLK_OFF,
};

struct vpu_dc_stat_info {
	atomic_t stat_start;/* if stat is required */
	struct timespec prev_ts;/* record last clk status change ts */
	unsigned long busy_time;	/* ms */
};

#define nb_to_devfreq_data(_nb) container_of(_nb, struct vpu_devfreq_data, nb)

struct vpu_devfreq_data {
	struct devfreq *devfreq;
	struct clk *vclk;

	/* VPU frequency table used for platform */
	unsigned int vpu_freq_tbl[VPU_FREQ_MAX]; /* unit Khz */
	unsigned int vpu_freq_tbl_len;

	/* clk notifier for dc state */
	struct notifier_block nb;
	/* used for dc stat for simple ondemand governor */
	struct vpu_dc_stat_info dc_info;
	struct timespec last_pts;/* record last polling ts */
};

#ifdef CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND
static struct devfreq_simple_ondemand_data ondemand_gov_data = {
	.upthreshold = VPU_DEVFREQ_UPTHRESHOLD,
	.downdifferential = VPU_DEVFREQ_DOWNDIFFERENTIAL,
};
#endif

static int vpu_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct platform_device *pdev;
	struct vpu_devfreq_data *data;

	pdev = container_of(dev, struct platform_device, dev);
	data = platform_get_drvdata(pdev);

	*freq = clk_get_rate(data->vclk) / KHZ_TO_HZ;
	return 0;
}

static void find_best_freq(struct vpu_devfreq_data *data,
			   unsigned long *freq, u32 flags)
{
	int i;
	unsigned long temp = *freq;

	u32 *freq_table = data->vpu_freq_tbl;
	u32 len = data->vpu_freq_tbl_len;

	if (*freq < freq_table[0]) {
		*freq = freq_table[0];
		return;
	}
	if (flags & DEVFREQ_FLAG_LEAST_UPPER_BOUND) {
		for (i = 1; i < len; i++)
			if (freq_table[i - 1] <= temp
			    && freq_table[i] > temp) {
				*freq = freq_table[i - 1];
				break;
			}
	} else {
		for (i = 0; freq_table[i]; i++)
			if (freq_table[i] >= temp) {
				*freq = freq_table[i];
				break;
			}
	}

	if (i == len)
		*freq = freq_table[i - 1];
}

static int vpu_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct platform_device *pdev = container_of(dev, struct platform_device,
						    dev);
	struct vpu_devfreq_data *data = platform_get_drvdata(pdev);
	int ret = 0;

	find_best_freq(data, freq, flags);

	mutex_lock(&vpu_pw_fc_lock);
	ret = clk_set_rate(data->vclk, *freq * KHZ_TO_HZ);
	mutex_unlock(&vpu_pw_fc_lock);

	if (!ret)
		*freq = clk_get_rate(data->vclk) / KHZ_TO_HZ;

	return ret;
}

/* function used for clk duty cycle stat */
static inline long ts2ms(struct timespec cur, struct timespec prev)
{
	return (cur.tv_sec - prev.tv_sec) * MSEC_PER_SEC +
		(cur.tv_nsec - prev.tv_nsec) / NSEC_PER_MSEC;
}

static void vpu_dutycycle_stats(struct clk *clk,
	enum vpu_status_msg msg, struct vpu_dc_stat_info *dc_info)
{
	struct timespec cur_ts, prev_ts;
	long time_ms;
	struct vpu_dc_stat_info *cur = dc_info;

	/* do nothing if no stat operation is issued */
	if (!atomic_read(&cur->stat_start))
		return;

	getnstimeofday(&cur_ts);
	prev_ts = cur->prev_ts;
	time_ms = ts2ms(cur_ts, prev_ts);

	switch (msg) {
	case VPU_DC_START:
		/*  duty cycle stat startclear old stat information */
		cur->prev_ts = cur_ts;
		cur->busy_time = 0;
		break;
	case VPU_DC_STOP:
		/* duty cycle stat stop */
		if (clk->enable_count)
			cur->busy_time += time_ms;
		break;
	case VPU_CLK_ON:
		/* clk switch from off->on */
		cur->prev_ts = cur_ts;
		break;
	case VPU_CLK_OFF:
		/* clk switch from off->on */
		cur->prev_ts = cur_ts;
		cur->busy_time += time_ms;
		break;
	default:
		break;
	}
}

static inline void startstop_vpu_dcstat(struct vpu_devfreq_data *data,
	unsigned int startstop)
{
	struct vpu_dc_stat_info *dcinfo = &data->dc_info;

	if (startstop) {
		atomic_set(&dcinfo->stat_start, 1);
		vpu_dutycycle_stats(data->vclk, VPU_DC_START, dcinfo);
	} else {
		vpu_dutycycle_stats(data->vclk, VPU_DC_STOP, dcinfo);
		atomic_set(&dcinfo->stat_start, 0);
	}
}

static int __maybe_unused vpu_devfreq_get_dev_status(struct device *dev,
		      struct devfreq_dev_status *stat)
{
	struct vpu_devfreq_data *data = dev_get_drvdata(dev);
	struct vpu_dc_stat_info *dcinfo = &data->dc_info;
	unsigned long totalms, busyms;
	struct timespec last_pts;

	vpu_get_cur_freq(dev, &stat->current_frequency);

	/* stop dc stat, calculate workload */
	startstop_vpu_dcstat(data, 0);
	last_pts = data->last_pts;
	getnstimeofday(&data->last_pts);
	stat->total_time = ts2ms(data->last_pts, last_pts);
	stat->busy_time = dcinfo->busy_time;

	busyms = stat->busy_time;
	totalms = stat->total_time;

	/* Ajust the workload calculation here to align with devfreq governor */
	if (busyms >= (1 << 24) || totalms >= (1 << 24)) {
		busyms >>= 7;
		totalms >>= 7;
	}

	dev_dbg(dev, "busy time is %lu\n", stat->busy_time);
	dev_dbg(dev, "total time is %lu\n", stat->total_time);
	dev_dbg(dev, "workload is %d precent, freq %lu\n",
		cal_workload(busyms, totalms), stat->current_frequency);
	/* TD: add trace here */
	/* trace_pxa_vpu_workload(workload, stat->current_frequency); */

	/* restart again, as we only record data of one sample winow */
	startstop_vpu_dcstat(data, 1);
	return 0;
};

static int __maybe_unused vpu_devfreq_clk_ntfhandler(struct notifier_block *nb,
		unsigned long msg, void *data)
{
	struct clk_notifier_data *cnd = data;
	struct vpu_devfreq_data *vdata = nb_to_devfreq_data(nb);
	struct device *dev = &vdata->devfreq->dev;

	/*
	 * clock enable/disable notifier used to duty cycle stat
	 * 1) enable clock, pre notify, rate 0->A
	 * 2) disable clock, post notify, rate A->0
	 */

	if (cnd->old_rate && cnd->new_rate)
		return NOTIFY_OK;

	if ((msg & PRE_RATE_CHANGE) && (!cnd->old_rate) && cnd->new_rate) {
		vpu_dutycycle_stats(vdata->vclk, VPU_CLK_ON, &vdata->dc_info);
		dev_dbg(dev, "%s VPU_CLK_ON!\n", __func__);
	} else if ((msg & POST_RATE_CHANGE) &&
			(cnd->old_rate) && (!cnd->new_rate)) {
		vpu_dutycycle_stats(vdata->vclk, VPU_CLK_OFF, &vdata->dc_info);
		dev_dbg(dev, "%s VPU_CLK_OFF!\n", __func__);
	}

	return NOTIFY_OK;
}

static int vpu_devfreq_probe(struct platform_device *pdev)
{
	int err = 0;
	int i = 0;
	struct vpu_devfreq_data *data = NULL;
	struct device *dev = &pdev->dev;
	struct devfreq_frequency_table *tbl;
	unsigned int index, freq_qos = 0;

	/* by default use userspace governor */
	const char *governor_name = "userspace";

	struct devfreq_dev_profile *vpu_devfreq_profile =
		kzalloc(sizeof(struct devfreq_dev_profile), GFP_KERNEL);
	vpu_devfreq_profile->target = vpu_target;

	data = kzalloc(sizeof(struct vpu_devfreq_data), GFP_KERNEL);

	if (data == NULL) {
		dev_err(dev, "Cannot allocate memory for vpu devfreq!\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, data);
	data->vclk = clk_get(dev, NULL);
	if (IS_ERR(data->vclk)) {
		dev_err(dev, "Cannot get clk ptr.\n");
		err = PTR_ERR(data->vclk);
		goto out;
	}

	/* save vpu frequency tbl */
	i = 0;
	of_property_read_u32(pdev->dev.of_node, "marvell,vpu-id", &index);
	tbl = devfreq_frequency_get_table(DEVFREQ_VPU_BASE + index);
	if (tbl) {
		while (tbl->frequency != DEVFREQ_TABLE_END) {
			data->vpu_freq_tbl[i] = tbl->frequency;
			tbl++;
			i++;
		}
		data->vpu_freq_tbl_len = i;
	}
	vpu_devfreq_profile->initial_freq =
			clk_get_rate(data->vclk) / KHZ_TO_HZ;

	/* set the frequency table of devfreq profile */
	if (data->vpu_freq_tbl_len) {
		vpu_devfreq_profile->freq_table = data->vpu_freq_tbl;
		vpu_devfreq_profile->max_state = data->vpu_freq_tbl_len;
		for (i = 0; i < data->vpu_freq_tbl_len; i++)
			opp_add(dev, data->vpu_freq_tbl[i], 1000);
	}

	/*
	 * Initilize the devfreq QoS if freq-qos flag is enabled.
	 * By default, the flag is disabled.
	 */
	freq_qos = 0;

	if (IS_ENABLED(CONFIG_OF)) {
		if (of_property_read_bool(pdev->dev.of_node, "marvell,qos"))
			freq_qos = 1;
	}

	if (freq_qos) {
		if (index == 0) {
			vpu_devfreq_profile->min_qos_type = PM_QOS_VPU_DEVFREQ_MIN;
			vpu_devfreq_profile->max_qos_type = PM_QOS_VPU_DEVFREQ_MAX;
		} else if (index == 1) {
			vpu_devfreq_profile->min_qos_type = PM_QOS_VPU2_DEVFREQ_MIN;
			vpu_devfreq_profile->max_qos_type = PM_QOS_VPU2_DEVFREQ_MAX;
		}
	}

#ifdef CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND
	if (of_property_match_string(pdev->dev.of_node,
		"vpu-governor", "simple_ondemand") >= 0) {

		u32 polling_ms, downdifferential, upthreshold;
		void *governor_data = NULL;

		governor_name = "simple_ondemand";
		governor_data = &ondemand_gov_data;

		/* connect vpu clk to get clk status change notifier */
		data->nb.notifier_call = vpu_devfreq_clk_ntfhandler;
		err = clk_notifier_register(data->vclk, &data->nb);
		if (err) {
			err = 0;
			goto cont;
		}

		if (of_property_read_u32(pdev->dev.of_node, "vpu-polling_ms",
						&polling_ms) < 0)
			vpu_devfreq_profile->polling_ms = DEFAULT_POLLING_MS;
		else
			vpu_devfreq_profile->polling_ms = polling_ms;

		if (of_property_read_u32(pdev->dev.of_node, "vpu-upthreshold",
						&upthreshold) < 0)
			ondemand_gov_data.upthreshold = VPU_DEVFREQ_UPTHRESHOLD;
		else
			ondemand_gov_data.upthreshold = upthreshold;

		if (of_property_read_u32(pdev->dev.of_node,
					"vpu-downdifferential",
						&downdifferential) < 0)
			ondemand_gov_data.downdifferential =
					VPU_DEVFREQ_DOWNDIFFERENTIAL;
		else
			ondemand_gov_data.downdifferential = downdifferential;

		vpu_devfreq_profile->get_dev_status =
					vpu_devfreq_get_dev_status;

		/* start after init */
		startstop_vpu_dcstat(data, 1);
		data->devfreq = devfreq_add_device(dev, vpu_devfreq_profile,
				governor_name, governor_data);
		if (IS_ERR(data->devfreq))
			clk_notifier_unregister(data->vclk, &data->nb);
	} else {
cont:
		governor_name = "userspace";
#endif
		data->devfreq = devfreq_add_device(dev, vpu_devfreq_profile,
				governor_name, NULL);
#ifdef CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND
	}
#endif

	if (IS_ERR(data->devfreq)) {
		err = PTR_ERR(data->devfreq);
		goto out;
	}

	dev_info(dev, "use govenor %s!\n", governor_name);

	/* init default devfreq min_freq and max_freq */
	if (data->vpu_freq_tbl_len) {
		data->devfreq->min_freq = data->devfreq->qos_min_freq =
			data->vpu_freq_tbl[0];
		data->devfreq->max_freq = data->devfreq->qos_max_freq =
			data->vpu_freq_tbl[data->vpu_freq_tbl_len - 1];
	}

#ifdef CONFIG_DEVFREQ_GOV_SIMPLE_ONDEMAND
    if (of_property_match_string(pdev->dev.of_node,
        "vpu-governor", "simple_ondemand") >= 0) {
        u32 min_freq, max_freq;
        if (0 == of_property_read_u32(pdev->dev.of_node, "vpu-min-freq", &min_freq)) {
            data->devfreq->min_freq = min_freq;
        }
        if (0 == of_property_read_u32(pdev->dev.of_node, "vpu-max-freq", &max_freq)) {
            data->devfreq->max_freq = max_freq;
        }
    }
#endif

	return 0;

out:
	kfree(data);
	dev_info(dev, "%s govenor register fails!\n", governor_name);
	return err;
}

static int vpu_devfreq_remove(struct platform_device *pdev)
{
	struct vpu_devfreq_data *data = platform_get_drvdata(pdev);
	devfreq_remove_device(data->devfreq);
	kfree(data);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mmp_devfreq_vpu_dt_match[] = {
	{.compatible = "marvell,devfreq-vpu" },
	{},
};
#endif
static struct platform_driver vpu_devfreq_driver = {
	.probe = vpu_devfreq_probe,
	.remove = vpu_devfreq_remove,
	.driver = {
		   .name = "devfreq-vpu",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(mmp_devfreq_vpu_dt_match),
		   },
};

static int __init vpu_devfreq_init(void)
{
	return platform_driver_register(&vpu_devfreq_driver);
}

static void __init vpu_devfreq_exit(void)
{
	platform_driver_unregister(&vpu_devfreq_driver);
}

module_init(vpu_devfreq_init);
module_exit(vpu_devfreq_exit);

MODULE_DESCRIPTION("vpu devfreq device driver");
MODULE_LICENSE("GPL");
