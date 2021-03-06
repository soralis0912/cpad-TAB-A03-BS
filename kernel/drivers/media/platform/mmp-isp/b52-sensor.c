/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regulator/driver.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
#include <media/b52-sensor.h>
#include <linux/leds.h>
#include <linux/math64.h>
#include <uapi/media/b52_api.h>
#include <media/b52socisp/b52socisp-mdev.h>
#include <linux/cputype.h>

#include <media/mv_sc2_twsi_conf.h>
#include "ccicv2.h"
#include "plat_cam.h"
#include "power_domain_isp.h"
/* #include <linux/mfd/88pm88x.h> */

static int otp_ctrl = -1;
module_param(otp_ctrl, int, 0644);

#include <linux/i2c.h>
int twsi_read_i2c_bb(u16 addr, u8 reg, u8 *val)
{
	int ret;
	struct i2c_adapter *adapter;
	struct i2c_msg msg[] = {
		{
			.addr	= addr,
			.flags	= 0,
			.len	= 1,
			.buf	= (u8 *)&reg,
		},
		{
			.addr	= addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= val,
		},
	};
	*val = 0;
	adapter = i2c_get_adapter(2);
	if (adapter == NULL)
		return -1;
	ret = i2c_transfer(adapter, msg, 2);
	return 0;
}
int twsi_read_i2c(u16 addr, u16 reg, u16 *val)
{
	int ret;
	struct i2c_adapter *adapter;
	struct i2c_msg msg[] = {
		{
			.addr	= addr,
			.flags	= 0,
			.len	= 2,
			.buf	= (u8 *)&reg,
		},
		{
			.addr	= addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= (u8 *)val,
		},
	};
	*val = 0;
	reg = swab16(reg);
	adapter = i2c_get_adapter(2);
	if (adapter == NULL)
		return -1;
	ret = i2c_transfer(adapter, msg, 2);
	return 0;
}
struct reg_tab_wb {
		u16 reg;
		u8 val;
} __packed;
struct reg_tab_bb {
		u8 reg;
		u8 val;
} __packed;

int twsi_write_i2c(u16 addr, u16 reg, u8 val)
{
	struct i2c_msg msg;
	struct reg_tab_wb buf;
	int ret = 0;
	struct i2c_adapter *adapter;
	reg = swab16(reg);
	buf.reg = reg;
	buf.val = val;
	msg.addr	= addr;
	msg.flags	= 0;
	msg.len		= 3;
	msg.buf		= (u8 *)&buf;
	adapter = i2c_get_adapter(2);
	ret = i2c_transfer(adapter, &msg, 1);
	if (ret < 0)
		return ret;
	return 0;
}
#define DM9714L_DATA_SHIFT 0x4
static int b52_sensor_g_focus_twsi(struct v4l2_subdev *sd, u16 *val)
{
	int ret = 0;
	u8 reg;
	u8 val1;
	struct i2c_adapter *adapter;
	struct b52_sensor_vcm *vcm;
	enum b52_vcm_type vcm_type;
	struct i2c_msg msg = {
			.flags	= I2C_M_RD,
			.len	= 2,
			.buf	= (u8 *)val,
	};
	struct b52_sensor *sensor = to_b52_sensor(sd);

	vcm = sensor->drvdata->module->vcm;
	vcm_type =  vcm->type;
	msg.addr = vcm->attr->addr;
	adapter = i2c_get_adapter(2);
	if (adapter == NULL)
		return -1;
	*val = 0;
	switch (vcm_type) {
	case VCM_DW9714:
		ret = i2c_transfer(adapter, &msg, 1);
		*val = (*val >> DM9714L_DATA_SHIFT) & 0xffff;
		break;
	case VCM_DW9804:
		reg = (u8)vcm->pos_reg_lsb;
		ret = twsi_read_i2c_bb(msg.addr, reg, (u8 *)val);
		*val = *val & 0xff;
		reg = (u8)vcm->pos_reg_msb;
		ret = twsi_read_i2c_bb(msg.addr, reg, &val1);
		*val = ((val1 & 0x3) << 8) | *val;
		break;
	case VCM_DW9718:
		reg = (u8)vcm->pos_reg_lsb;
		ret = twsi_read_i2c_bb(msg.addr, reg, (u8 *)val);
		*val = *val & 0xff;
		reg = (u8)vcm->pos_reg_msb;
		ret = twsi_read_i2c_bb(msg.addr, reg, &val1);
		*val = ((val1 & 0x3) << 8) | *val;
		break;
	default:
		ret = -EIO;
		pr_err("not support current vcm type\n");
	}


	return ret;
}
static int b52_sensor_s_focus_twsi(struct v4l2_subdev *sd, u16 val)
{
	int ret = 0;
	struct i2c_adapter *adapter;
	struct b52_sensor_vcm *vcm;
	struct reg_tab_bb buf;
	enum b52_vcm_type vcm_type;
	u8 val_buf[2];
	struct i2c_msg msg;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (sensor->drvdata->num_module == 0)
		return 0;

	vcm = sensor->drvdata->module->vcm;
	vcm_type =  vcm->type;
	msg.addr = vcm->attr->addr;
	msg.flags = 0;
	msg.len = 2;
	adapter = i2c_get_adapter(2);
	if (adapter == NULL)
		return -1;

	switch (vcm_type) {
	case VCM_DW9714:
		msg.buf = val_buf;
		val = (val << 4) | 0x0f;
		msg.buf[0] = (val>>8) & 0x3f;
		msg.buf[1] = val & 0xff;
		ret = i2c_transfer(adapter, &msg, 1);
		break;
	case VCM_DW9804:
		msg.buf = (u8 *)&buf;
		buf.reg = (u8)vcm->pos_reg_msb;
		buf.val = (u8)((val >> 8) & 0x3);
		ret = i2c_transfer(adapter, &msg, 1);
		buf.reg = (u8)vcm->pos_reg_lsb;
		buf.val = (u8)(val & 0xff);
		ret |= i2c_transfer(adapter, &msg, 1);
		break;
	case VCM_DW9718:
		msg.buf = (u8 *)&buf;
		buf.reg = (u8)vcm->pos_reg_msb;
		buf.val = (u8)((val >> 8) & 0x3);
		ret = i2c_transfer(adapter, &msg, 1);
		buf.reg = (u8)vcm->pos_reg_lsb;
		buf.val = (u8)(val & 0xff);
		ret |= i2c_transfer(adapter, &msg, 1);
		break;
	default:
		ret = 0;
		pr_err("not support current vcm type\n");
	}
	return ret;
}

static int b52_sensor_s_expo(struct v4l2_subdev *sd, u32 expo, u16 vts)
{
	const struct b52_sensor_regs *reg;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (sensor->drvdata->type == SONY_SENSOR)
		expo = (expo >> 4) & 0xFFFF;
	else if (sensor->drvdata->type == SAMSUNG_SENSOR) {
		expo = (expo >> 4);
	}

	reg = &sensor->drvdata->vts_reg;
	b52_sensor_call(sensor, i2c_write, reg->tab[0].reg, vts, reg->num);

	reg = &sensor->drvdata->expo_reg;
	b52_sensor_call(sensor, i2c_write, reg->tab[0].reg, expo, reg->num);

	return 0;
}

static int b52_sensor_s_gain(struct v4l2_subdev *sd, u32 gain)
{
	int max;
	u32 ag, dg;
	const struct b52_sensor_regs *reg;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	reg = &sensor->drvdata->gain_reg[B52_SENSOR_AG];
	max = sensor->drvdata->gain_range[B52_SENSOR_AG].max;
	/* gain_range[B52_SENSOR_AG].max is set to ISP
	* ISP will report the A-Gain value as max *2
	* So, here special handle the max value for Samsung's sensor
	*/
	if (sensor->drvdata->type == SAMSUNG_SENSOR)
		max = (max << 1);

	ag = (gain <= max) ? gain : max;
	ag <<= sensor->drvdata->gain_shift;
	if (sensor->drvdata->type == SONY_SENSOR)
		ag = (256*16 - 256*256/ag + 8)/16;
	b52_sensor_call(sensor, i2c_write, reg->tab[0].reg, ag, reg->num);

	reg = &sensor->drvdata->gain_reg[B52_SENSOR_DG];
	if (reg->tab == NULL) {
		if (gain > max)
			pr_err("gain value is incorrect: %d\n", gain);
		return 0;
	}

	if (gain <= max)
		dg = B52_GAIN_UNIT << 4;
	else
		dg = (gain << 8) / max;
	dg <<= sensor->drvdata->gain_shift;
	if (sensor->drvdata->type != SONY_SENSOR)
		dg = (dg + 8) >> 4;
	b52_sensor_call(sensor, i2c_write, reg->tab[0].reg, dg, reg->num);

	return 0;
}

static int b52_sensor_cmd_read_twsi(struct v4l2_subdev *sd, u16 addr,
		u32 *val, u8 num)
{
	int i;
	int shift = 0;
	struct regval_tab tab[3];
	struct b52_cmd_i2c_data data;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (!sensor || !val || (num == 0)) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	if (num > 3) {
		pr_err("%s, read num too long\n", __func__);
		return -EINVAL;
	}

	data.attr = &sensor->drvdata->i2c_attr[sensor->cur_i2c_idx];
	data.tab = tab;
	data.num = num;
	data.pos = sensor->pos;
	for (i = 0; i < data.num; i++)
		twsi_read_i2c(data.attr->addr, addr + i, &data.tab[i].val);

	if (num == 1) {
		*val = tab[0].val;
	} else if (num == 2) {
		if (data.attr->val_len == I2C_8BIT)
			shift = 8;
		else if (data.attr->val_len == I2C_16BIT)
			shift = 16;
		*val = (tab[0].val << shift) | tab[1].val;
	} else if (num == 3) {
		if (data.attr->val_len == I2C_8BIT)
			shift = 8;
		else {
			pr_err("wrong i2c len for num %d\n", num);
			return -EINVAL;
		}

		*val = (tab[0].val << shift * 2) |
			   (tab[1].val << shift * 1) |
			   (tab[2].val << shift * 0);
	}

	return 0;
}

static int b52_sensor_cmd_write_twsi(struct v4l2_subdev *sd, u16 addr,
		u32 val, u8 num)
{
	int shift = 0, i;
	struct regval_tab tab[3];
	const struct b52_sensor_i2c_attr *attr;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (!sensor || num == 0) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	attr = &sensor->drvdata->i2c_attr[sensor->cur_i2c_idx];

	switch (num) {
	case 1:
		tab[0].reg = addr;
		tab[0].val = val;
		break;
	case 2:
		if (attr->val_len == I2C_8BIT)
			shift = 8;
		else if (attr->val_len == I2C_16BIT)
			shift = 16;

		tab[0].reg = addr;
		tab[0].val = (val >> shift) & ((1 << shift) - 1);

		tab[1].reg = addr + 1;
		tab[1].val = val & ((1 << shift) - 1);
		break;
	case 3:
		if (attr->val_len == I2C_8BIT)
			shift = 8;
		else {
			pr_err("wrong i2c len for num %d\n", num);
			return -EINVAL;
		}

		tab[0].reg = addr;
		tab[0].val = (val >> shift * 2) & ((1 << shift) - 1);

		tab[1].reg = addr + 1;
		tab[1].val = (val >> shift * 1) & ((1 << shift) - 1);

		tab[2].reg = addr + 2;
		tab[2].val = (val >> shift * 0) & ((1 << shift) - 1);
		break;
	default:
		pr_err("%s, write num no correct\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < num; i++)
		twsi_write_i2c(attr->addr, tab[i].reg, tab[i].val);

	return 0;
}
static int b52_sensor_g_cur_fmt_twsi(struct v4l2_subdev *sd,
	struct b52_cmd_i2c_data *data)
{
	int i;
	struct b52_sensor *sensor = to_b52_sensor(sd);
	if (!data) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&sensor->lock);

	data->attr = &sensor->drvdata->i2c_attr[sensor->cur_i2c_idx];
	data->tab  = sensor->mf_regs.tab;
/*not write sensor setting in set_format command*/
/*	data->num  = sensor->mf_regs.num; */
	data->num  = 0;
	data->pos  = sensor->pos;
	for (i = 0; i < data->num; i++)
		twsi_write_i2c(data->attr->addr,
				data->tab[i].reg,
				data->tab[i].val);
	mutex_unlock(&sensor->lock);

	return 0;
}

static int __b52_sensor_cmd_write_twsi(const struct b52_sensor_i2c_attr
		*i2c_attr, const struct b52_sensor_regs *regs, u8 pos)
{
	struct b52_cmd_i2c_data data;
	int i;

	if (!i2c_attr || !regs) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	if (regs->num == 0)
		return 0;

	data.attr = i2c_attr;
	data.tab  = regs->tab;
	data.num  = regs->num;
	data.pos  = pos;
	for (i = 0; i < data.num; i++)
		twsi_write_i2c(data.attr->addr,
				data.tab[i].reg,
				data.tab[i].val);
	/*FIXME:how to get completion*/
	return 0;
}

static int __b52_sensor_cmd_write(const struct b52_sensor_i2c_attr
		*i2c_attr, const struct b52_sensor_regs *regs, u8 pos)
{
	struct b52_cmd_i2c_data data;

	if (!i2c_attr || !regs) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	if (regs->num == 0)
		return 0;

	data.attr = i2c_attr;
	data.tab  = regs->tab;
	data.num  = regs->num;
	data.pos  = pos;

	/*FIXME:how to get completion*/
	return b52_cmd_write_i2c(&data);
}
static int b52_sensor_cmd_write(struct v4l2_subdev *sd, u16 addr,
		u32 val, u8 num)
{
	int shift = 0;
	struct regval_tab tab[3];
	struct b52_sensor_regs regs;
	const struct b52_sensor_i2c_attr *attr;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (!sensor || num == 0) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	attr = &sensor->drvdata->i2c_attr[sensor->cur_i2c_idx];
	if (num > 3 || (num == 3 && attr->val_len == I2C_16BIT)) {
		pr_err("%s, write num %d too long\n", __func__, num);
		return -EINVAL;
	}

	if (attr->val_len == I2C_8BIT)
		shift = 8;
	else if (attr->val_len == I2C_16BIT)
		shift = 16;

	switch (num) {
	case 1:
		tab[0].reg = addr;
		tab[0].val = val;
		break;
	case 2:
		tab[0].reg = addr;
		tab[0].val = (val >> shift) & ((1 << shift) - 1);

		tab[1].reg = addr + 1;
		tab[1].val = val & ((1 << shift) - 1);
		break;
	case 3:
		tab[0].reg = addr;
		tab[0].val = (val >> shift * 2) & ((1 << shift) - 1);

		tab[1].reg = addr + 1;
		tab[1].val = (val >> shift * 1) & ((1 << shift) - 1);

		tab[2].reg = addr + 2;
		tab[2].val = (val >> shift * 0) & ((1 << shift) - 1);
		break;
	default:
		pr_err("%s, write num no correct\n", __func__);
		return -EINVAL;
	}

	regs.tab = tab;
	regs.num = num;

	return __b52_sensor_cmd_write(attr, &regs, sensor->pos);
}

static int b52_sensor_cmd_read(struct v4l2_subdev *sd, u16 addr,
		u32 *val, u8 num)
{
	int ret;
	int shift = 0;
	int i;
	struct regval_tab tab[3];
	const struct b52_sensor_i2c_attr *attr;
	struct b52_cmd_i2c_data data;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (!sensor || !val || (num == 0)) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	attr = &sensor->drvdata->i2c_attr[sensor->cur_i2c_idx];
	if (num > 3 || (num == 3 && attr->val_len == I2C_16BIT)) {
		pr_err("%s, read num %d too long\n", __func__, num);
		return -EINVAL;
	}

	for (i = 0; i < num; i++)
		tab[i].reg = addr + i;
	data.attr = attr;
	data.tab = tab;
	data.num = num;
	data.pos = sensor->pos;

	ret = b52_cmd_read_i2c(&data);
	if (ret)
		return ret;

	if (data.attr->val_len == I2C_8BIT)
		shift = 8;
	else if (data.attr->val_len == I2C_16BIT)
		shift = 16;

	switch (num) {
	case 1:
		*val = tab[0].val;
		break;
	case 2:
		*val = (tab[0].val << shift) | tab[1].val;
		break;
	case 3:
		*val =	(tab[0].val << shift * 2) |
			(tab[1].val << shift * 1) |
			(tab[2].val << shift * 0);
		break;
	default:
		pr_err("%s, read num no correct\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int b52_sensor_g_cur_fmt(struct v4l2_subdev *sd,
	struct b52_cmd_i2c_data *data)
{
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (!data) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&sensor->lock);

	data->attr = &sensor->drvdata->i2c_attr[sensor->cur_i2c_idx];
	data->tab  = sensor->mf_regs.tab;
	data->num  = sensor->mf_regs.num;
	data->pos  = sensor->pos;

	mutex_unlock(&sensor->lock);

	return 0;
}

struct b52_sensor *b52_get_sensor(struct media_entity *entity)
{
	struct v4l2_subdev *sd;
	struct media_device *mdev = entity->parent;
	struct media_entity_graph graph;
	struct media_entity *sensor_entity = NULL;

	mutex_lock(&mdev->graph_mutex);
	media_entity_graph_walk_start(&graph, entity);
	while ((entity = media_entity_graph_walk_next(&graph)))
		if (entity->type == MEDIA_ENT_T_V4L2_SUBDEV_SENSOR) {
			sensor_entity = entity;
			break;
		}
	mutex_unlock(&mdev->graph_mutex);

	if (!sensor_entity) {
		pr_err("sensor entity not found\n");
		return NULL;
	}

	sd = container_of(sensor_entity, struct v4l2_subdev, entity);
	return to_b52_sensor(sd);
}
EXPORT_SYMBOL(b52_get_sensor);



/*only used for detect sensor, not download the FW*/
static int b52_sensor_isp_read(const struct b52_sensor_i2c_attr *attr,
		u16 reg, u16 *val, u8 pos)
{
	if (!attr || !val) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}
	if (pxa1928_is_a0())
		return twsi_read_i2c(attr->addr, reg, val);
	else
		return b52_isp_read_i2c(attr, reg, val, pos);
}

static int b52_vcm_init(struct b52_sensor_vcm *vcm)
{
	int ret = 0;
	int written_num = 0;
	struct i2c_adapter *adapter;
	struct reg_tab_bb buf;
	enum b52_vcm_type vcm_type;
	struct i2c_msg msg;

	if (!vcm)
		return 0;

	vcm_type =  vcm->type;
	msg.addr = vcm->attr->addr;
	msg.flags = 0;
	msg.len = 2;
	adapter = i2c_get_adapter(2);
	if (adapter == NULL) {
		pr_err("%s: Unable to get adapter\n", __func__);
		return -1;
	}

	switch (vcm_type) {
	case VCM_DW9714:
		break;
	case VCM_DW9804:
		msg.buf = (u8 *)&buf;
		while (written_num < vcm->init.num) {
			buf.reg = (u8)vcm->init.tab[written_num].reg;
			buf.val = (u8)vcm->init.tab[written_num].val;
			ret = i2c_transfer(adapter, &msg, 1);
			if (ret < 0)
				return ret;
			written_num++;
		}
		break;
	case VCM_DW9718:
		msg.buf = (u8 *)&buf;
		while (written_num < vcm->init.num) {
			buf.reg = (u8)vcm->init.tab[written_num].reg;
			buf.val = (u8)vcm->init.tab[written_num].val;
			ret |= i2c_transfer(adapter, &msg, 1);
			written_num++;
		}
		break;
	default:
		pr_err("not support current vcm type\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int b52_sensor_init(struct v4l2_subdev *sd)
{
	int ret = 0;
	int num = 0;
	int written_num = 0;
	struct sensor_otp user_otp;
	struct b52_sensor_regs regs;
	const struct b52_sensor_i2c_attr *attr;
	struct b52_sensor_module *module;
	struct b52_sensor_vcm *vcm;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (!sensor) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	attr = &sensor->drvdata->i2c_attr[sensor->cur_i2c_idx];
	regs.tab = sensor->drvdata->global_setting.tab;

	while (written_num < sensor->drvdata->global_setting.num) {
		if (likely(regs.tab[num].reg != SENSOR_MDELAY ||
				regs.tab[num].val != SENSOR_MDELAY)) {
			num++;
			if (likely(written_num + num <
					sensor->drvdata->global_setting.num))
				continue;
		}

		regs.num = num;
		if (pxa1928_is_a0())
			ret = __b52_sensor_cmd_write_twsi(attr, &regs, sensor->pos);
		else
			ret = __b52_sensor_cmd_write(attr, &regs, sensor->pos);
		if (ret)
			return ret;

		if (unlikely(regs.tab[num].reg == SENSOR_MDELAY &&
				regs.tab[num].val == SENSOR_MDELAY)) {
			msleep(regs.tab[num].mask);
			num++;
		}

		written_num += num;
		regs.tab += written_num;
		num = 0;
	}

	b52_sensor_call(sensor, get_pixel_rate,
			&sensor->pixel_rate, sensor->mclk);
	pr_debug("sensor pxiel rate %d\n", sensor->pixel_rate);

	if (sensor->csi.calc_dphy)
		b52_sensor_call(sensor, get_dphy_desc,
				&sensor->csi.dphy_desc, sensor->mclk);

	if (otp_ctrl != -1)
		sensor->otp.otp_ctrl = otp_ctrl;

	if (sensor->drvdata->ops->update_otp) {
		user_otp.otp_type = SENSOR_TO_SENSOR;
		sensor->otp.user_otp =  &user_otp;
		b52_sensor_call(sensor, update_otp, &sensor->otp);
	}

	module = sensor->drvdata->module;
	for (num = 0; num < sensor->drvdata->num_module; num++)
		if (sensor->otp.module_id == module->id)
			break;

	if (num < sensor->drvdata->num_module)
		sensor->cur_mod_id = num;
	else
		sensor->cur_mod_id = -ENODEV;

	if (pxa1928_is_a0()) {
		if (sensor->drvdata->num_module != 0) {
			vcm = sensor->drvdata->module->vcm;
			/* init vcm setting */
			ret = b52_vcm_init(vcm);
			if (ret < 0)
				pr_err("%s, error param\n", __func__);
		}
	}

	ret = v4l2_ctrl_handler_setup(&sensor->ctrls.ctrl_hdl);
	if (ret < 0)
		pr_err("%s: setup hadnler failed\n", __func__);
	return ret;
}
#if 0
/* FIXME add detect vcm in future */
static int b52_sensor_detect_vcm(struct v4l2_subdev *sd)
{
	int i;
	u16 val;
	int ret;
	const struct b52_sensor_vcm *vcm;
	const struct b52_sensor_regs *id;
	const struct b52_sensor_i2c_attr *attr;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (!sensor) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}
	if (!sensor->drvdata->module) {
		pr_err("sensor support internal vcm\n");
		return 0;
	}

	vcm = sensor->drvdata->module->vcm;
	id = &vcm->id;

	attr = vcm->attr;

	for (i = 0; i < id->num; i++) {
		ret = b52_sensor_isp_read(attr, id->tab[i].reg,
				&val, sensor->pos);

		if (ret || val != id->tab[i].val) {
			pr_err("detect %s failed\n", vcm->name);
			pr_err("val: got %x, req %x\n", val, id->tab[i].val);
			return -ENODEV;
		}
	}

	pr_info("sensor external vcm: %s detected\n", vcm->name);
	return 0;
}
#endif
static inline int __detect_sensor(const struct b52_sensor_regs *id,
			const struct b52_sensor_i2c_attr *attr, u8 pos)
{
	int i;
	u16 val;
	int ret;

	for (i = 0; i < id->num; i++) {
		ret = b52_sensor_isp_read(attr, id->tab[i].reg, &val, pos);

		if (ret || val != id->tab[i].val) {
			pr_debug("val: got %x, req %x\n", val, id->tab[i].val);
			return -ENODEV;
		}
	}

	return 0;
}

static int b52_sensor_detect_sensor(struct v4l2_subdev *sd)
{
	int i;
	int ret;
	const struct b52_sensor_regs *id;
	const struct b52_sensor_i2c_attr *attr;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (!sensor) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	id = &sensor->drvdata->id;
	for (i = 0; i < sensor->drvdata->num_i2c_attr; i++) {
		attr = &sensor->drvdata->i2c_attr[i];
		ret = __detect_sensor(id, attr, sensor->pos);
		if (!ret)
			goto sensor_detected;
	}

	pr_err("detect %s failed\n", sensor->drvdata->name);
	return ret;

sensor_detected:
	sensor->cur_i2c_idx = i;
	pr_info("sensor %s detected, i2c addr 0x%x\n",
			sensor->drvdata->name, sensor->drvdata->i2c_attr[i].addr);

	return 0;
}

static int board_revision = 1;
inline void b52_sensor_set_board_revision(int rev)
{
	board_revision = rev;
}
EXPORT_SYMBOL(b52_sensor_set_board_revision);

int b52_sensor_power_on(struct v4l2_subdev *sd)
{
	int ret	= 0;
	struct sensor_power *power;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	power = (struct sensor_power *) &(sensor->power);

	if (power->pwdn >= 0)
		gpio_request(power->pwdn, "CAM_ENABLE_LOW");
	if (power->rst >= 0)
		gpio_request(power->rst, "CAM_RESET_LOW");

	if (power->avdd_2v8) {
		regulator_set_voltage(power->avdd_2v8,
				2800000, 2800000);
		ret = regulator_enable(power->avdd_2v8);
		if (ret < 0)
			return ret;
	}

	if (power->pwdn >= 0)
		gpio_direction_output(power->pwdn, !power->pwdn_value);
	if (power->dovdd_1v8) {
		regulator_set_voltage(power->dovdd_1v8,
				1800000, 1800000);
		ret = regulator_enable(power->dovdd_1v8);
		if (ret < 0)
			goto dovdd_err;
	}

	if (power->dvdd_1v2) {
		regulator_set_voltage(power->dvdd_1v2,
				1200000, 1200000);
		ret = regulator_enable(power->dvdd_1v2);
		if (ret < 0)
			goto dvdd_err;
	}

	if (power->af_2v8) {
		regulator_set_voltage(power->af_2v8,
				2800000, 2800000);
		ret = regulator_enable(power->af_2v8);
		if (ret < 0)
			goto af_err;
	}

	if (power->rst >= 0) {
		gpio_direction_output(power->rst, power->rst_value);
		usleep_range(100, 120);
		gpio_direction_output(power->rst, !power->rst_value);
	}
	return 0;

af_err:
	if (power->dvdd_1v2)
		regulator_disable(power->dvdd_1v2);
dvdd_err:
	if (power->dovdd_1v8)
		regulator_disable(power->dovdd_1v8);
dovdd_err:
	if (power->avdd_2v8)
		regulator_disable(power->avdd_2v8);

	return ret;
}
static int b52_sensor_get_power(struct v4l2_subdev *sd)
{
	int ret;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct b52_sensor *sensor = to_b52_sensor(sd)
	struct device_node *pdata_np, *power_np;

	pdata_np = (struct device_node *)client->dev.of_node;
	power_np = of_get_child_by_name(pdata_np, "sensor_power");

	sensor->power.pwdn = of_get_named_gpio(power_np, "pwdn-gpios", 0);
	if (sensor->power.pwdn < 0)
		pr_err("%x: of_get_named_gpio failed\n", __LINE__);
	else {
		ret = of_property_read_u32(power_np,
			"pwdn-validvalue", &sensor->power.pwdn_value);
		if (ret < 0) {
			pr_err("%x: of_get_gpio_value failed\n", __LINE__);
			return -ENODEV;
		}
	}

	sensor->power.rst = of_get_named_gpio(power_np, "reset-gpios", 0);
	if (sensor->power.rst < 0)
		pr_err("%x: of_get_named_gpio failed\n", __LINE__);
	else {
		ret = of_property_read_u32(power_np,
			"reset-validvalue", &sensor->power.rst_value);
		if (ret < 0) {
			pr_err("%x: of_get_gpio_value failed\n", __LINE__);
			return -ENODEV;
		}
	}
	sensor->power.af_2v8 = devm_regulator_get(&client->dev, "af_2v8");
	if (IS_ERR(sensor->power.af_2v8)) {
		dev_warn(&client->dev, "Failed to get regulator af_2v8\n");
		sensor->power.af_2v8 = NULL;
	}

	if (board_revision == 1) {
		sensor->power.avdd_2v8 = devm_regulator_get(&client->dev, "avdd_2v8");
		if (IS_ERR(sensor->power.avdd_2v8)) {
			dev_warn(&client->dev, "Failed to get regulator avdd_2v8\n");
			sensor->power.avdd_2v8 = NULL;
		}
	} else {
		sensor->power.avdd_2v8 = devm_regulator_get(&client->dev, "avdd_2v8v4");
		if (IS_ERR(sensor->power.avdd_2v8)) {
			dev_warn(&client->dev, "Failed to get regulator avdd_2v8v4\n");
			sensor->power.avdd_2v8 = NULL;
		}
	}
	sensor->power.dovdd_1v8 = devm_regulator_get(&client->dev, "dovdd_1v8");
	if (IS_ERR(sensor->power.dovdd_1v8)) {
		dev_warn(&client->dev, "Failed to get regulator dovdd_1v8\n");
		sensor->power.dovdd_1v8 = NULL;
	}
	sensor->power.dvdd_1v2 = devm_regulator_get(&client->dev, "dvdd_1v2");
	if (IS_ERR(sensor->power.dvdd_1v2)) {
		dev_warn(&client->dev, "Failed to get regulator dvdd_1v2\n");
		sensor->power.dvdd_1v2 = NULL;
	}

	return 0;
}

static int b52_sensor_put_power(struct v4l2_subdev *sd)
{
	struct b52_sensor *sensor = to_b52_sensor(sd)
	if (sensor->power.avdd_2v8)
		devm_regulator_put(sensor->power.avdd_2v8);
	if (sensor->power.dvdd_1v2)
		devm_regulator_put(sensor->power.dvdd_1v2);
	if (sensor->power.af_2v8)
		devm_regulator_put(sensor->power.af_2v8);
	if (sensor->power.dovdd_1v8)
		devm_regulator_put(sensor->power.dovdd_1v8);
	return 0;
}

static int b52_sensor_g_vcm_info(struct v4l2_subdev *sd,
		struct b52_sensor_vcm *vcm)
{
	struct b52_sensor *sensor = to_b52_sensor(sd);
	if (sensor->cur_mod_id < 0) {
		pr_err("%s: no module, no vcm\n", __func__);
		return -ENODEV;
	}
	if (sensor->drvdata->num_module == 0)
		return 1;
	else
		*vcm = *sensor->drvdata->module[sensor->cur_mod_id].vcm;

	return 0;
}

static int __maybe_unused b52_sensor_s_vcm_lp(struct v4l2_subdev *sd)
{
	int ret;
	struct b52_sensor_vcm vcm;
	struct b52_sensor_i2c_attr attr  = {
		.reg_len = I2C_8BIT,
		.val_len = I2C_8BIT,
	};
	int vcm_type;
	struct b52_cmd_i2c_data data;
	struct regval_tab tab[2];
	struct b52_sensor *sensor = to_b52_sensor(sd)
	ret = b52_sensor_g_vcm_info(sd, &vcm);
	if (ret)
		return ret;
	vcm_type = vcm.type;
	attr.addr = vcm.attr->addr;
	data.attr = &attr;
	data.tab = tab;
	data.num = 1;
	data.pos = sensor->pos;
	tab[0].mask = 0xff;
	tab[1].mask = 0xff;
	switch (vcm_type) {
	case VCM_DW9714:
		tab[0].reg = 0x80;
		tab[0].val = 0x00;
		break;
	case VCM_DW9804:
		tab[0].reg = 0x03;
		tab[0].val = 0x00;
		tab[1].reg = 0x04;
		tab[1].val = 0x00;
		data.num = 2;
		break;
	case VCM_DW9718:
		tab[0].reg = 0x00;
		tab[0].val = 0x01;
		break;
	default:
		pr_err("not support current vcm type\n");
		ret = -EINVAL;
		break;
	}
	b52_cmd_write_i2c(&data);
	return 0;
}

static int b52_sensor_gain_to_iso(struct v4l2_subdev *sd,
		u32 gain, u32 *iso)
{
	const struct v4l2_fract *f;
	struct b52_sensor *sensor = to_b52_sensor(sd);
	f = &sensor->drvdata->gain2iso_ratio;

	if (!f->denominator) {
		pr_err("%s: f->denominator is zero\n", __func__);
		return -EINVAL;
	}

	*iso = gain * f->numerator / f->denominator;
	return 0;
}

static int b52_sensor_iso_to_gain(struct v4l2_subdev *sd,
		u32 iso, u32 *gain)
{
	const struct v4l2_fract *f;
	struct b52_sensor *sensor = to_b52_sensor(sd);
	f = &sensor->drvdata->gain2iso_ratio;

	if (!f->numerator) {
		pr_err("%s: f->numerator is zero\n", __func__);
		return -EINVAL;
	}

	*gain = iso * f->denominator / f->numerator;
	return 0;
}

static int b52_sensor_to_expo_line(struct v4l2_subdev *sd,
		u32 time, u32 *lines)
{
	/*
	 * time unit: 100 us according to v4l2
	 */
	u32 us = time * 100;
	u8 i;
	u64 temp;
	struct b52_sensor *sensor = to_b52_sensor(sd);
	i = sensor->cur_res_idx;

	if (!lines) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	temp = us * (u64)(sensor->pixel_rate);
	temp = div_u64(temp, sensor->drvdata->res[i].hts);
	*lines = (u32)div_u64(temp + 500000, 1000000);

	pr_debug("%u us, line %u, hts %u, pixle rate %u\n", us, *lines,
		sensor->drvdata->res[i].hts, sensor->pixel_rate);
	return 0;
}

static int b52_sensor_to_expo_time(struct v4l2_subdev *sd,
		u32 *time, u32 lines)
{
	/*time unit: 100 us according to v4l2*/
	u32 line_time;
	u8 i;
	struct b52_sensor *sensor = to_b52_sensor(sd);
	i = sensor->cur_res_idx;

	if (!time) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	line_time = sensor->drvdata->res[i].hts * 100000 /
		(sensor->pixel_rate / 10000);
	/*line_time unit: 1ns*/
	*time = lines * line_time / 100000;

	pr_debug("%s: %d 100us, %d line\n", __func__, *time, lines);
	return 0;
}

static int b52_sensor_g_cur_fps(struct v4l2_subdev *sd,
	struct v4l2_fract *fps)
{
	u32 i;
	int ret;
	u32 vts;
	u32 size;
	const struct b52_sensor_regs *reg;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (!fps) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	i = sensor->cur_res_idx;
	reg = &sensor->drvdata->vts_reg;

	ret = b52_sensor_call(sensor, i2c_read,
			reg->tab->reg, &vts, reg->num);
	if (ret) {
		pr_err("%s: read vts failed\n", __func__);
		return ret;
	}

	size = vts * sensor->drvdata->res[i].hts;

	fps->numerator = (sensor->pixel_rate * 10) / (size / 10);
	fps->denominator = 100;

	return 0;
}

#define BANDING_STEP_50HZ	0
#define BANDING_STEP_60HZ	1
static u32 __cal_band_step(int hz, u32 pixel_rate, u32 hts)
{
	u32 banding_step;

	if (hz == BANDING_STEP_50HZ)
		banding_step = pixel_rate/100/hts;
	else if (hz == BANDING_STEP_60HZ)
		banding_step = pixel_rate/120/hts;
	else
		return 0;

	return banding_step;
}
static int b52_sensor_g_band_step(struct v4l2_subdev *sd,
		u16 *band_50hz, u16 *band_60hz)
{
	int i;
	struct b52_sensor *sensor = to_b52_sensor(sd);
	i = sensor->cur_res_idx;

	*band_50hz = __cal_band_step(BANDING_STEP_50HZ,
		sensor->pixel_rate,
		sensor->drvdata->res[i].hts);
	*band_60hz = __cal_band_step(BANDING_STEP_60HZ,
		sensor->pixel_rate,
		sensor->drvdata->res[i].hts);

	return 0;
}

static int b52_sensor_s_flip(struct v4l2_subdev *sd,
		int hflip, int on)
{
	u32 val;
	int ret;
	const struct b52_sensor_regs *reg;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (hflip)
		reg = &sensor->drvdata->hflip;
	else
		reg = &sensor->drvdata->vflip;

	ret = b52_sensor_call(sensor, i2c_read,
			reg->tab->reg, &val, reg->num);
	if (ret)
		return ret;

	val &= ~(reg->tab->mask);

	/*
	 * reg->mask: the bit controls the flip. Like 0x2, bit1 controls.
	 * reg->tab->val:original value to get the correct orientation.
	 * Set the related bit to inverse value when flip. For example:
	 * original value is 0x4(bit2), change bit2 to 0 when flip.
	 */
	if (on)
		val |= ((~(reg->tab->val)) & (reg->tab->mask));
	else
		val |= reg->tab->val;

	ret = b52_sensor_call(sensor, i2c_write,
			reg->tab->reg, val, reg->num);

	return ret;
}

static int b52_sensor_g_param_range(struct v4l2_subdev *sd,
		int type, u16 *min, u16 *max)
{
	const struct b52_sensor_data *data;
	struct b52_sensor *sensor = to_b52_sensor(sd);
	data = sensor->drvdata;

	if (!min || !max) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	switch (type) {
	case B52_SENSOR_GAIN:
		*min = data->gain_range[B52_SENSOR_AG].min;
		*max = data->gain_range[B52_SENSOR_AG].max;
		if (data->gain_range[B52_SENSOR_DG].min != 0) {
			*min = *min * data->gain_range[B52_SENSOR_DG].min / B52_GAIN_UNIT;
			*max = *max * data->gain_range[B52_SENSOR_DG].max / B52_GAIN_UNIT;
		}
		break;

	case B52_SENSOR_AGAIN:
		*min = data->gain_range[B52_SENSOR_AG].min;
		*max = data->gain_range[B52_SENSOR_AG].max;
		break;

	case B52_SENSOR_DGAIN:
		*min = data->gain_range[B52_SENSOR_DG].min;
		*max = data->gain_range[B52_SENSOR_DG].max;
		break;

	case B52_SENSOR_EXPO:
		*min = data->expo_range.min;
		*max = data->expo_range.max;
		break;

	case B52_SENSOR_FRACTIONALEXPO:
		*min = data->frationalexp_range.min;
		*max = data->frationalexp_range.max;
		break;

	case B52_SENSOR_VTS:
		*min = data->vts_range.min;
		*max = data->vts_range.max;
		break;

	case B52_SENSOR_REQ_VTS:
		*min = data->res[sensor->cur_res_idx].min_vts;
		*max = data->res[sensor->cur_res_idx].min_vts;
		break;

	case B52_SENSOR_REQ_HTS:
		*min = data->res[sensor->cur_res_idx].hts;
		*max = data->res[sensor->cur_res_idx].hts;
		break;

	case B52_SENSOR_FOCUS:
		*min = data->focus_range.min;
		*max = data->focus_range.max;
		break;

	default:
		pr_err("%s: wrong type\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int b52_sensor_g_sensor_attr(struct v4l2_subdev *sd,
		struct b52_sensor_i2c_attr *attr)
{
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (!attr) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	*attr = sensor->drvdata->i2c_attr[sensor->cur_i2c_idx];

	return 0;
}

static int b52_sensor_g_aecagc_reg(struct v4l2_subdev *sd,
		int type, struct b52_sensor_regs *reg)
{
	const struct b52_sensor_data *data;
	struct b52_sensor *sensor = to_b52_sensor(sd);
	data = sensor->drvdata;

	switch (type) {
	case B52_SENSOR_AGAIN:
		*reg = data->gain_reg[B52_SENSOR_AG];
		break;

	case B52_SENSOR_DGAIN:
		*reg = data->gain_reg[B52_SENSOR_DG];
		break;

	case B52_SENSOR_EXPO:
		*reg = data->expo_reg;
		break;

	case B52_SENSOR_VTS:
		*reg = data->vts_reg;
		break;

	case B52_SENSOR_FRACTIONALEXPO:
		*reg = data->frationalexp_reg;
		break;

	default:
		pr_err("%s: wrong type: %d\n", __func__, type);
		return -EINVAL;
	}

	if (!reg->tab || (reg->num == 0)) {
		pr_err("%s: reg is null\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int b52_sensor_g_csi(struct v4l2_subdev *sd,
		struct mipi_csi2 *csi)
{
	struct b52_sensor *sensor = to_b52_sensor(sd);

	*csi = sensor->csi;

	return  0;
}
static struct b52_sensor_ops b52_sensor_def_ops_host = {
	.init          = b52_sensor_init,
	.i2c_read      = b52_sensor_cmd_read_twsi,
	.i2c_write     = b52_sensor_cmd_write_twsi,
	.g_cur_fmt     = b52_sensor_g_cur_fmt_twsi,
	.s_focus       = b52_sensor_s_focus_twsi,
	.g_focus       = b52_sensor_g_focus_twsi,
	.s_expo        = b52_sensor_s_expo,
	.s_gain        = b52_sensor_s_gain,
	.get_power     = b52_sensor_get_power,
	.put_power     = b52_sensor_put_power,
	.detect_sensor = b52_sensor_detect_sensor,
	/* .detect_vcm    = b52_sensor_detect_vcm,*/
	.g_vcm_info    = b52_sensor_g_vcm_info,
	.g_cur_fps     = b52_sensor_g_cur_fps,
	.g_param_range = b52_sensor_g_param_range,
	.g_aecagc_reg  = b52_sensor_g_aecagc_reg,
	.g_sensor_attr = b52_sensor_g_sensor_attr,
	.g_band_step   = b52_sensor_g_band_step,
	.g_csi         = b52_sensor_g_csi,
	.s_flip        = b52_sensor_s_flip,
	.gain_to_iso   = b52_sensor_gain_to_iso,
	.iso_to_gain   = b52_sensor_iso_to_gain,
	.to_expo_line  = b52_sensor_to_expo_line,
	.to_expo_time  = b52_sensor_to_expo_time,
};

static struct b52_sensor_ops b52_sensor_def_ops_sccb = {
	.init          = b52_sensor_init,
	.i2c_read      = b52_sensor_cmd_read,
	.i2c_write     = b52_sensor_cmd_write,
	.g_cur_fmt     = b52_sensor_g_cur_fmt,
	.get_power     = b52_sensor_get_power,
	.put_power     = b52_sensor_put_power,
	.detect_sensor = b52_sensor_detect_sensor,
	/* .detect_vcm    = b52_sensor_detect_vcm,*/
	.g_vcm_info    = b52_sensor_g_vcm_info,
	.g_cur_fps     = b52_sensor_g_cur_fps,
	.g_param_range = b52_sensor_g_param_range,
	.g_aecagc_reg  = b52_sensor_g_aecagc_reg,
	.g_sensor_attr = b52_sensor_g_sensor_attr,
	.g_band_step   = b52_sensor_g_band_step,
	.g_csi         = b52_sensor_g_csi,
	.s_flip        = b52_sensor_s_flip,
	.gain_to_iso   = b52_sensor_gain_to_iso,
	.iso_to_gain   = b52_sensor_iso_to_gain,
	.to_expo_line  = b52_sensor_to_expo_line,
	.to_expo_time  = b52_sensor_to_expo_time,
};


static int b52_sensor_set_defalut(struct b52_sensor *sensor)
{
	if (!sensor) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	if (pxa1928_is_a0())
		sensor->ops = b52_sensor_def_ops_host;
	else
		sensor->ops = b52_sensor_def_ops_sccb;

	if (!sensor->drvdata->ops->get_pixel_rate) {
		pr_err("error: get_pixel_rate not defined\n");
		return -EINVAL;
	} else {
		sensor->ops.get_pixel_rate =
			sensor->drvdata->ops->get_pixel_rate;
	}

	if (sensor->csi.calc_dphy) {
		if (!sensor->drvdata->ops->get_dphy_desc) {
			pr_err("error: get_dphy not defined\n");
			return -EINVAL;
		} else
			sensor->ops.get_dphy_desc =
				sensor->drvdata->ops->get_dphy_desc;
	}

	if (!sensor->drvdata->ops->update_otp) {
		pr_err("error: update_otp not defined\n");
		return -EINVAL;
	} else {
		sensor->ops.update_otp =
			sensor->drvdata->ops->update_otp;
		/*Add OTP control for Sensor
		* default value is  "V4L2_CID_SENSOR_OTP_CONTROL_WB
		*	 | V4L2_CID_SENSOR_OTP_CONTROL_LENC"
		*/
		sensor->otp.otp_ctrl = 3;
		sensor->otp.otp_type = SENSOR_TO_SENSOR;
	}
	return 0;
}

static int b52_sensor_s_power(struct v4l2_subdev *sd, int on)
{
	int ret = 0;
	struct sensor_power *power;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	power = (struct sensor_power *) &(sensor->power);

	if (on) {
		if (power->ref_cnt++ > 0)
			return 0;

		ret = plat_tune_isp(1);
		if (ret < 0) {
			pr_err("b52 sensor power on isp failed\n");
			goto tune_isp_err;
		}
		/*set sensor mclk to CSI before sensor power on*/
		clk_set_rate(sensor->clk, sensor->mclk);
		clk_prepare_enable(sensor->clk);

		if (sensor->i2c_dyn_ctrl) {
			/*this W/R for a0, will remove when don't support a0*/
			if (pxa1928_is_a0()) {
				ret = sc2_select_pins_state(sensor->pos - 1,
					SC2_PIN_ST_TWSI, SC2_MOD_B52ISP);
				if (ret < 0) {
					pr_err("b52 sensor i2c pin is not configured\n");
					goto st_err;
				}
			} else {
				ret = sc2_select_pins_state(sensor->pos - 1,
					SC2_PIN_ST_SCCB, SC2_MOD_B52ISP);
				if (ret < 0) {
					pr_err("b52 sensor i2c pin is not configured\n");
					goto st_err;
				}
			}
		}

		ret = sensor->drvdata->ops->power_on(sd);
		if (ret)
			goto pwr_err;
	} else {
		if (WARN_ON(power->ref_cnt == 0))
			return -EINVAL;

		if (--power->ref_cnt > 0)
			return 0;
#if 0
		if (get_b52fw_loaded())
			b52_sensor_s_vcm_lp(sd);
#endif

		if (power->rst >= 0)
			gpio_direction_output(power->rst, power->rst_value);
		if (power->dvdd_1v2)
			regulator_disable(power->dvdd_1v2);
		if (power->avdd_2v8)
			regulator_disable(power->avdd_2v8);
		/*
		 * Keep PWDN always on as defined in spec
		 * if (power->pwdn >= 0)
		 *	gpio_direction_output(power->pwdn, power->pwdn_value);
		 */

		if (power->dovdd_1v8)
			regulator_disable(power->dovdd_1v8);
		if (power->af_2v8)
			regulator_disable(power->af_2v8);
		if (power->rst >= 0)
			gpio_free(power->rst);
		if (power->pwdn >= 0)
			gpio_free(power->pwdn);

		if (sensor->i2c_dyn_ctrl) {
			ret = sc2_select_pins_state(sensor->pos - 1,
					SC2_PIN_ST_GPIO, SC2_MOD_B52ISP);
			if (ret < 0)
				pr_err("b52 sensor gpio pin is not configured\n");
		}
		/*set mclk to 0 when sensor power closed.*/
		clk_disable_unprepare(sensor->clk);

		WARN_ON(plat_tune_isp(0) < 0);
	}

	return 0;

pwr_err:
	if (sensor->i2c_dyn_ctrl)
		sc2_select_pins_state(sensor->pos - 1,
				SC2_PIN_ST_GPIO, SC2_MOD_B52ISP);
st_err:
	clk_disable_unprepare(sensor->clk);
	WARN_ON(plat_tune_isp(0) < 0);
tune_isp_err:
	power->ref_cnt--;
	return ret;
}

static int b52_sensor_s_stream(struct v4l2_subdev *sd, int enable)
{
	const struct b52_sensor_regs *regs;
	const struct b52_sensor_i2c_attr *attr;
	struct b52_sensor *sensor = to_b52_sensor(sd)

	attr = &sensor->drvdata->i2c_attr[sensor->cur_i2c_idx];

	if (enable)
		regs = &sensor->drvdata->streamon;
	else
		regs = &sensor->drvdata->streamoff;
	if (pxa1928_is_a0())
		return __b52_sensor_cmd_write_twsi(attr, regs, sensor->pos);
	else
		return __b52_sensor_cmd_write(attr, regs, sensor->pos);
}
static enum v4l2_mbus_pixelcode b52_sensor_get_real_mbus(
	    struct v4l2_subdev *sd, enum v4l2_mbus_pixelcode code)
{
	int hflip;
	int vflip;
	enum v4l2_mbus_pixelcode new_code;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	if (!sensor->drvdata->flip_change_phase)
		return code;

	hflip = v4l2_ctrl_g_ctrl(sensor->ctrls.hflip);
	vflip = v4l2_ctrl_g_ctrl(sensor->ctrls.vflip);

	switch (code) {
	case V4L2_MBUS_FMT_SBGGR10_1X10:
		if (hflip && vflip)
			new_code = V4L2_MBUS_FMT_SRGGB10_1X10;
		else if (hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SGBRG10_1X10;
		else if (!hflip && vflip)
			new_code = V4L2_MBUS_FMT_SGRBG10_1X10;
		else if (!hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SBGGR10_1X10;
		break;

	case V4L2_MBUS_FMT_SGBRG10_1X10:
		if (hflip && vflip)
			new_code = V4L2_MBUS_FMT_SGRBG10_1X10;
		else if (hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SBGGR10_1X10;
		else if (!hflip && vflip)
			new_code = V4L2_MBUS_FMT_SRGGB10_1X10;
		else if (!hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SGBRG10_1X10;
		break;

	case V4L2_MBUS_FMT_SGRBG10_1X10:
		if (hflip && vflip)
			new_code = V4L2_MBUS_FMT_SGBRG10_1X10;
		else if (hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SRGGB10_1X10;
		else if (!hflip && vflip)
			new_code = V4L2_MBUS_FMT_SBGGR10_1X10;
		else if (!hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SGRBG10_1X10;
		break;

	case V4L2_MBUS_FMT_SRGGB10_1X10:
		if (hflip && vflip)
			new_code = V4L2_MBUS_FMT_SBGGR10_1X10;
		else if (hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SGRBG10_1X10;
		else if (!hflip && vflip)
			new_code = V4L2_MBUS_FMT_SGBRG10_1X10;
		else if (!hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SRGGB10_1X10;
		break;

	case V4L2_MBUS_FMT_SBGGR8_1X8:
		if (hflip && vflip)
			new_code = V4L2_MBUS_FMT_SRGGB8_1X8;
		else if (hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SGBRG8_1X8;
		else if (!hflip && vflip)
			new_code = V4L2_MBUS_FMT_SGRBG8_1X8;
		else if (!hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SBGGR8_1X8;
		break;

	case V4L2_MBUS_FMT_SGBRG8_1X8:
		if (hflip && vflip)
			new_code = V4L2_MBUS_FMT_SGRBG8_1X8;
		else if (hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SBGGR8_1X8;
		else if (!hflip && vflip)
			new_code = V4L2_MBUS_FMT_SRGGB8_1X8;
		else if (!hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SGBRG8_1X8;
		break;

	case V4L2_MBUS_FMT_SGRBG8_1X8:
		if (hflip && vflip)
			new_code = V4L2_MBUS_FMT_SGBRG8_1X8;
		else if (hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SRGGB8_1X8;
		else if (!hflip && vflip)
			new_code = V4L2_MBUS_FMT_SBGGR8_1X8;
		else if (!hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SGRBG8_1X8;
		break;

	case V4L2_MBUS_FMT_SRGGB8_1X8:
		if (hflip && vflip)
			new_code = V4L2_MBUS_FMT_SBGGR8_1X8;
		else if (hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SGRBG8_1X8;
		else if (!hflip && vflip)
			new_code = V4L2_MBUS_FMT_SGBRG8_1X8;
		else if (!hflip && !vflip)
			new_code = V4L2_MBUS_FMT_SRGGB8_1X8;
		break;

	default:
		pr_err("Not support mbus phase change of [h/v]flip\n");
		new_code = code;
		break;
	}

	return new_code;
}

static int b52_sensor_enum_mbus_code(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_mbus_code_enum *code)
{
	const struct b52_sensor_data *data;
	struct b52_sensor *sensor = to_b52_sensor(sd)

	data = sensor->drvdata;

	if (code->pad > 0 || code->index > data->num_mbus_fmt) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	code->code = b52_sensor_get_real_mbus(sd,
		data->mbus_fmt[code->index].mbus_code);

	return 0;
}

static int b52_sensor_enum_frame_size(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_frame_size_enum *fse)
{
	int i;
	const struct b52_sensor_data *data;
	struct b52_sensor *sensor = to_b52_sensor(sd)
	data = sensor->drvdata;

	if (fse->pad > 0 || fse->index > data->num_res) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < data->num_mbus_fmt; i++)
		if (fse->code == b52_sensor_get_real_mbus(sd,
			data->mbus_fmt[i].mbus_code))
			break;

	if (i >= data->num_mbus_fmt) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	fse->min_width = data->res[fse->index].width;
	fse->max_width = data->res[fse->index].width;
	fse->min_height = data->res[fse->index].height;
	fse->max_height = data->res[fse->index].height;

	return 0;
}

static int b52_sensor_enum_frame_interval(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_frame_interval_enum *fie)
{

	int i;
	u32 size;
	const struct b52_sensor_data *data;
	struct b52_sensor *sensor = to_b52_sensor(sd)
	data = sensor->drvdata;

	if (fie->pad > 0 || fie->index > data->num_res) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < data->num_mbus_fmt; i++)
		if (fie->code == b52_sensor_get_real_mbus(sd,
			data->mbus_fmt[i].mbus_code))
			break;

	if (i >= data->num_mbus_fmt) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < data->num_res; i++)
		if (fie->width == data->res[i].width &&
			fie->height == data->res[i].height)
			break;

	if (i >= data->num_res) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	size = data->res[i].min_vts * data->res[i].hts;

	fie->interval.numerator = (size * 10) /
			(sensor->pixel_rate / 1000);
	fie->interval.denominator = 10000;

	return 0;
}

static int b52_sensor_get_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_format *format)
{
	int ret = 0;
	const struct b52_sensor_data *data;
	struct b52_sensor *sensor = to_b52_sensor(sd);
	data = sensor->drvdata;

	if (format->pad > 0) {
		pr_err("%s, error pad num\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&sensor->lock);

	switch (format->which) {
	case V4L2_SUBDEV_FORMAT_TRY:
		/*FIXME*/
		format->format = *v4l2_subdev_get_try_format(fh, 0);
		break;
	case V4L2_SUBDEV_FORMAT_ACTIVE:
		sensor->mf.code = b52_sensor_get_real_mbus(sd,
				data->mbus_fmt[sensor->cur_mbus_idx].mbus_code);
		format->format = sensor->mf;
		break;
	default:
		ret = -EINVAL;
		pr_err("%s, error format->which\n", __func__);
		goto error;
	}

error:
	mutex_unlock(&sensor->lock);
	return ret;
}

static int b52_sensor_set_fmt(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh,
		struct v4l2_subdev_format *format)
{
	int i;
	int j;
	struct b52_sensor_regs *mf_regs;
	const struct b52_sensor_data *data;
	struct v4l2_mbus_framefmt *mf = &format->format;
	struct b52_sensor *sensor = to_b52_sensor(sd);
	data = sensor->drvdata;
	mf_regs = &sensor->mf_regs;

	if (format->pad > 0) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < data->num_mbus_fmt; i++)
		if (mf->code == b52_sensor_get_real_mbus(sd,
			data->mbus_fmt[i].mbus_code))
			break;

	if (i >= data->num_mbus_fmt) {
		pr_info("%s: mbus code not match\n", __func__);
		i = 0;
	};

	mf->code = b52_sensor_get_real_mbus(sd, data->mbus_fmt[i].mbus_code);
	mf->colorspace = data->mbus_fmt[i].colorspace;

	for (j = 0; j < data->num_res; j++)
		if (mf->width == data->res[j].width &&
			mf->height == data->res[j].height)
			break;

	if (j >= data->num_res) {
		pr_info("%s: frame size not match\n", __func__);
		j = 0;
	}

	mf->width  = data->res[j].width;
	mf->height = data->res[j].height;
	mf->field  = V4L2_FIELD_NONE;


	if (format->which == V4L2_SUBDEV_FORMAT_ACTIVE) {
		int size = sizeof(*mf_regs->tab);

		mutex_lock(&sensor->lock);

		if (mf->code != sensor->mf.code ||
			mf->width != sensor->mf.width ||
			mf->height != sensor->mf.height) {
			memcpy(mf_regs->tab,
					data->mbus_fmt[i].regs.tab,
					size * data->mbus_fmt[i].regs.num);
			mf_regs->num = data->mbus_fmt[i].regs.num;

			memcpy(mf_regs->tab + mf_regs->num,
					data->res[j].regs.tab,
					size * data->res[j].regs.num);
			mf_regs->num += data->res[j].regs.num;
		}

		sensor->mf = *mf;
		sensor->cur_mbus_idx = i;
		sensor->cur_res_idx = j;

		mutex_unlock(&sensor->lock);
	}

	return 0;
}

static int b52_sensor_g_skip_top_lines(struct v4l2_subdev *sd, u32 *lines)
{
	struct b52_sensor *sensor = to_b52_sensor(sd);
	*lines = sensor->drvdata->skip_top_lines;

	return 0;
}

static int b52_sensor_g_skip_frames(struct v4l2_subdev *sd, u32 *frames)
{
	struct b52_sensor *sensor = to_b52_sensor(sd);

	*frames = sensor->drvdata->skip_frames;

	return 0;
}
static int b52_sensor_sd_open(struct v4l2_subdev *sd,
				struct v4l2_subdev_fh *fh)
{
	int ret;
	/*FIXME: not need put power on here*/
	ret = v4l2_subdev_call(sd, core, s_power, 1);

	return ret;
}

static int b52_sensor_sd_close(struct v4l2_subdev *sd,
		struct v4l2_subdev_fh *fh)
{
	int ret;
	/*FIXME: not need put power off here*/
	ret = v4l2_subdev_call(sd, core, s_power, 0);

	return ret;
}

static int b52_sensor_link_setup(struct media_entity *entity,
			  const struct media_pad *local,
			  const struct media_pad *remote, u32 flags)
{
	return 0;
}

static struct v4l2_subdev_video_ops b52_sensor_video_ops = {
	.s_stream = b52_sensor_s_stream,
};

static const struct v4l2_subdev_pad_ops b52_sensor_pad_ops = {
	.enum_mbus_code      = b52_sensor_enum_mbus_code,
	.enum_frame_size     = b52_sensor_enum_frame_size,
	.enum_frame_interval = b52_sensor_enum_frame_interval,
	.get_fmt             = b52_sensor_get_fmt,
	.set_fmt             = b52_sensor_set_fmt,
};

struct v4l2_subdev_sensor_ops b52_sensor_sensor_ops = {
	.g_skip_top_lines = b52_sensor_g_skip_top_lines,
	.g_skip_frames    = b52_sensor_g_skip_frames,
};

static int b52_sensor_reinit(struct b52_sensor *sensor)
{
	int cnt = 0;
	int ori_cnt;
	int ret = 0;
	const struct b52_sensor_regs *regs;
	const struct b52_sensor_i2c_attr *attr;

	if (!sensor)
		return -EINVAL;

	ori_cnt = sensor->power.ref_cnt;
	if (!ori_cnt) {
		pr_err("%s, not pwr on, no need to re-init\n", __func__);
		return 0;
	}

	cnt = ori_cnt;
	while (cnt--)
		v4l2_subdev_call(&sensor->sd, core, s_power, 0);
	msleep(100);
	cnt = ori_cnt;
	while (cnt--) {
		ret |= v4l2_subdev_call(&sensor->sd, core, s_power, 1);
		if (ret != 0)
			pr_err("%s, unable to pwr on\n", __func__);
	}

	b52_sensor_call(sensor, init);

	regs = &sensor->drvdata->streamon;
	attr = &sensor->drvdata->i2c_attr[sensor->cur_i2c_idx];
	ret |= __b52_sensor_cmd_write(attr, regs, sensor->pos);

	return ret;
}

static int b52_sensor_esd_status(struct b52_sensor *sensor, int *status)
{
	int i;
	int ret;
	u32 val;
	const struct b52_sensor_regs *esd;

	if (!sensor || !status)
		return -EINVAL;

	esd = &sensor->drvdata->esd;

	if (!esd->num) {
		*status = SENSOR_ESD_ST_UNKNOWN;
		return 0;
	}

	for (i = 0; i < esd->num; i++) {
		ret = b52_sensor_call(sensor, i2c_read,
					  esd->tab[i].reg, &val, 1);

		if (ret || val != esd->tab[i].val) {
			*status = SENSOR_ESD_ST_DAMAGED;
			return 0;
		}
	}

	*status = SENSOR_ESD_ST_WORKING;

	return 0;
}

static long b52_sensor_ioctl(struct v4l2_subdev *sd,
				unsigned int cmd, void *arg)
{
	int ret;
	struct b52_sensor *sensor = to_b52_sensor(sd);

	switch (cmd) {
	case VIDIOC_PRIVATE_B52ISP_SENSOR_OTP:
		sensor->otp.user_otp = (struct sensor_otp *)arg;
		sensor->otp.otp_type = sensor->otp.user_otp->otp_type;
		ret = b52_sensor_call(sensor, update_otp, &sensor->otp);
		sensor->otp.user_otp = NULL;
		break;
	case VIDIOC_PRIVATE_B52ISP_SENSOR_REINIT:
		ret = b52_sensor_reinit(sensor);
		break;
	case VIDIOC_PRIVATE_B52ISP_SENSOR_ESD_STATUS:
		ret = b52_sensor_esd_status(sensor, (int *)arg);
		break;
	default:
		pr_err("unknown compat ioctl '%c', dir=%d, #%d (0x%08x)\n",
			_IOC_TYPE(cmd), _IOC_DIR(cmd), _IOC_NR(cmd), cmd);
		return -ENXIO;
	}

	return ret;
}
#ifdef CONFIG_COMPAT
/*FIXME: need to refine return val*/
static int b52_usercopy(struct v4l2_subdev *sd,
		unsigned int cmd, void *arg)
{
	char	sbuf[128];
	void    *mbuf = NULL;
	void	*parg = arg;
	long	err  = -EINVAL;

	/*  Copy arguments into temp kernel buffer  */
	if (_IOC_DIR(cmd) != _IOC_NONE) {
		if (_IOC_SIZE(cmd) <= sizeof(sbuf)) {
			parg = sbuf;
		} else {
			/* too big to allocate from stack */
			mbuf = kmalloc(_IOC_SIZE(cmd), GFP_KERNEL);
			if (NULL == mbuf)
				return -ENOMEM;
			parg = mbuf;
		}

		err = -EFAULT;
		if (_IOC_DIR(cmd) & _IOC_WRITE) {
			unsigned int n = _IOC_SIZE(cmd);

			if (copy_from_user(parg, (void __user *)arg, n))
				goto out;

			/* zero out anything we don't copy from userspace */
			if (n < _IOC_SIZE(cmd))
				memset((u8 *)parg + n, 0, _IOC_SIZE(cmd) - n);
		} else {
			/* read-only ioctl */
			memset(parg, 0, _IOC_SIZE(cmd));
		}
	}

	/* Handles IOCTL */
	err = v4l2_subdev_call(sd, core, ioctl, cmd, parg);
	if (err == -ENOIOCTLCMD)
		err = -ENOTTY;
	if (_IOC_DIR(cmd) & _IOC_READ) {
		unsigned int n = _IOC_SIZE(cmd);
		if (copy_to_user((void __user *)arg, parg, n))
			goto out;
	}
out:
	kfree(mbuf);
	return err;
}

struct sensor_otp32 {
	enum OTP_TYPE	otp_type;
	__u16	lsc_otp_len;
	__u16	wb_otp_len;
	__u16	vcm_otp_len;
	__u16	module_data_len;
	compat_caddr_t	crc_status;
	compat_caddr_t	otp_data;
	compat_caddr_t	module_data;
	compat_caddr_t	full_otp;
	__u16	full_otp_offset;
	__u16	full_otp_len;
	compat_caddr_t	read_otp_len;
};

#define VIDIOC_PRIVATE_B52ISP_SENSOR_OTP32 \
	_IOWR('V', BASE_VIDIOC_PRIVATE + 11, struct sensor_otp32)

static int get_sensor_otp32(struct sensor_otp *kp,
		struct sensor_otp32 __user *up)
{
	u32 tmp1, tmp2, tmp3, tmp4, tmp5;
	if (!access_ok(VERIFY_READ, up, sizeof(struct sensor_otp32)) ||
			get_user(tmp1, &up->otp_data) ||
			get_user(tmp2, &up->module_data) ||
			get_user(tmp3, &up->full_otp) ||
			get_user(tmp4, &up->read_otp_len) ||
			get_user(tmp5, &up->crc_status) ||
			get_user(kp->otp_type, &up->otp_type) ||
			get_user(kp->lsc_otp_len, &up->lsc_otp_len) ||
			get_user(kp->wb_otp_len, &up->wb_otp_len) ||
			get_user(kp->vcm_otp_len, &up->vcm_otp_len) ||
			get_user(kp->module_data_len, &up->module_data_len) ||
			get_user(kp->full_otp_offset, &up->full_otp_offset) ||
			get_user(kp->full_otp_len, &up->full_otp_len))
		return -EFAULT;
	kp->otp_data = compat_ptr(tmp1);
	kp->module_data = compat_ptr(tmp2);
	kp->full_otp = compat_ptr(tmp3);
	kp->read_otp_len = compat_ptr(tmp4);
	kp->crc_status = compat_ptr(tmp5);
	return 0;
}
static long b52_sensor_ioctl32(struct v4l2_subdev *sd,
				unsigned int cmd, void *arg)
{
	int ret = 0;
	struct sensor_otp karg;
	int compatible_arg = 1;

	switch (cmd) {
	case VIDIOC_PRIVATE_B52ISP_SENSOR_OTP32:
		cmd = VIDIOC_PRIVATE_B52ISP_SENSOR_OTP;
		get_sensor_otp32(&karg, arg);
		compatible_arg = 0;
		break;
	case VIDIOC_PRIVATE_B52ISP_SENSOR_REINIT:
	case VIDIOC_PRIVATE_B52ISP_SENSOR_ESD_STATUS:
		break;
	default:
		pr_err("unknown compat ioctl '%c', dir=%d, #%d (0x%08x)\n",
			_IOC_TYPE(cmd), _IOC_DIR(cmd), _IOC_NR(cmd), cmd);
		return -ENXIO;
	}

	if (compatible_arg)
		ret = b52_usercopy(sd, cmd, arg);
	else {
		mm_segment_t old_fs = get_fs();

		set_fs(KERNEL_DS);
		ret = b52_usercopy(sd, cmd, (void *)&karg);
		set_fs(old_fs);
	}

	return ret;
}
#endif
static struct v4l2_subdev_core_ops b52_sensor_core_ops = {
	.s_power = b52_sensor_s_power,
	.ioctl	= b52_sensor_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl32 = b52_sensor_ioctl32,
#endif
};
static struct v4l2_subdev_ops b52_sensor_sd_ops = {
	.core   = &b52_sensor_core_ops,
	.pad    = &b52_sensor_pad_ops,
	.video  = &b52_sensor_video_ops,
	.sensor = &b52_sensor_sensor_ops,
};

static const struct v4l2_subdev_internal_ops b52_sensor_sd_internal_ops = {
	.open  = b52_sensor_sd_open,
	.close = b52_sensor_sd_close,
};

static const struct media_entity_operations sensor_media_ops = {
	.link_setup = b52_sensor_link_setup,
};

static int b52_detect_sensor(struct b52_sensor *sensor)
{
	int ret;

	if (!sensor) {
		pr_err("%s, error param\n", __func__);
		return -EINVAL;
	}

	ret = b52_sensor_call(sensor, get_power);
	if (ret)
		return ret;

	ret = v4l2_subdev_call(&sensor->sd, core, s_power, 1);
	if (ret) {
		pr_err("%s, sensor power up error\n", __func__);
		goto error;
	}

	ret = b52_sensor_call(sensor, detect_sensor);

#if 0
	/* FIXME support detect sensor success while VCM detect error */
	ret |= b52_sensor_call(sensor, detect_vcm);
#endif
	v4l2_subdev_call(&sensor->sd, core, s_power, 0);
	if (ret)
		goto error;

	return 0;

error:
	b52_sensor_call(sensor, put_power);
	return ret;
}

/*FIXME: refine the min/max*/
#define FLASH_TIMEOUT_MIN		100000	/* us */
#define FLASH_TIMEOUT_MAX		100000
#define FLASH_TIMEOUT_STEP		50000

#define FLASH_INTENSITY_MIN         100 /* mA */
#define FLASH_INTENSITY_STEP		20
#define FLASH_INTENSITY_MAX         100 /* mA */

#define TORCH_INTENSITY_MIN         100  /* mA */
#define TORCH_INTENSITY_MAX         100
#define TORCH_INTENSITY_STEP        20

static int b52_sensor_config_flash(
		struct b52_sensor_flash *flash)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int b52_sensor_set_flash(
		struct b52_sensor_flash *flash, int on, int pos)
{
	if (pos == 0)
		ledtrig_flash_ctrl(on);
	/*TBD add front camera flash support */
	flash->flash_status = on;

	return 0;
}

static int b52_sensor_set_torch(
		struct b52_sensor_flash *flash, int on, int pos)
{
	if (pos == 0)
		ledtrig_torch_ctrl(on);
	/*TBD add front camera torch support */
	flash->flash_status = on;

	return 0;
}
static int b52_sensor_get_flash_duration(
		struct b52_sensor_flash *flash, int *value)
{
	return 0;
	/* return get_flash_duration(value); */
}
static int b52_sensor_g_ctrl(struct v4l2_ctrl *ctrl)
{
	int i;
	struct b52_sensor_flash *flash;
	struct b52_sensor *sensor = container_of(
			ctrl->handler, struct b52_sensor, ctrls.ctrl_hdl);
	flash = &sensor->flash;

	switch (ctrl->id) {
	case V4L2_CID_HBLANK:
		i = sensor->cur_res_idx;
		ctrl->val = sensor->drvdata->res[i].hts -
			sensor->drvdata->res[i].width;
		break;

	case V4L2_CID_VBLANK:
		i = sensor->cur_res_idx;
		ctrl->val = sensor->drvdata->res[i].min_vts -
			sensor->drvdata->res[i].height;
		break;

	case V4L2_CID_PIXEL_RATE:
		ctrl->val64 = sensor->pixel_rate;
		break;

	case V4L2_CID_FLASH_FAULT:
		break;

	case V4L2_CID_FLASH_STROBE_STATUS:
		ctrl->val = flash->flash_status;
		break;

	case V4L2_CID_PRIVATE_FLASH_DURATION:
		return b52_sensor_get_flash_duration(flash, &ctrl->val);
		break;

	default:
		pr_err("%s: ctrl not support\n", __func__);
		return -EINVAL;
	}

	pr_debug("G_CTRL %08x:%d\n", ctrl->id, ctrl->val);

	return 0;
}

static int b52_sensor_s_ctrl(struct v4l2_ctrl *ctrl)
{
	int sensor_pos;
	struct b52_sensor_flash *flash;
	struct b52_sensor *sensor = container_of(
			ctrl->handler, struct b52_sensor, ctrls.ctrl_hdl);
	flash = &sensor->flash;
	sensor_pos = sensor->sensor_pos;

	/*FIXME: implement flash config and set function*/
	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
		b52_sensor_call(sensor, s_flip, 0, ctrl->val);
		break;

	case V4L2_CID_HFLIP:
		b52_sensor_call(sensor, s_flip, 1, ctrl->val);
		break;

	case V4L2_CID_FLASH_LED_MODE:
		flash->led_mode = ctrl->val;
		b52_sensor_config_flash(flash);
		break;

	case V4L2_CID_FLASH_STROBE_SOURCE:
		flash->strobe_source = ctrl->val;
		b52_sensor_config_flash(flash);
		break;

	case V4L2_CID_FLASH_STROBE:
		if (flash->led_mode == V4L2_FLASH_LED_MODE_FLASH)
			b52_sensor_set_flash(flash, 1, sensor_pos);
		else if (flash->led_mode == V4L2_FLASH_LED_MODE_TORCH)
			b52_sensor_set_torch(flash, 1, sensor_pos);
		else
			return -EBUSY;
		break;

	case V4L2_CID_FLASH_STROBE_STOP:
		if (flash->led_mode == V4L2_FLASH_LED_MODE_FLASH)
			b52_sensor_set_flash(flash, 0, sensor_pos);
		else if (flash->led_mode == V4L2_FLASH_LED_MODE_TORCH)
			b52_sensor_set_torch(flash, 0, sensor_pos);
		else
			return -EBUSY;
		break;

	case V4L2_CID_FLASH_TIMEOUT:
		flash->timeout = ctrl->val;

		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH)
			break;
		break;

	case V4L2_CID_FLASH_INTENSITY:
		flash->flash_current = (ctrl->val - FLASH_INTENSITY_MIN)
				     / FLASH_INTENSITY_STEP;

		if (flash->led_mode != V4L2_FLASH_LED_MODE_FLASH)
			break;

		b52_sensor_config_flash(flash);
		break;

	case V4L2_CID_FLASH_TORCH_INTENSITY:
		/*FIXME*/
		flash->torch_current = (ctrl->val - TORCH_INTENSITY_MIN)
			/ TORCH_INTENSITY_STEP;

		if (flash->led_mode != V4L2_FLASH_LED_MODE_TORCH)
			break;

		b52_sensor_config_flash(flash);
		break;

	case V4L2_CID_PRIVATE_SENSOR_OTP_CONTROL:
		sensor->otp.otp_ctrl = ctrl->val;
		break;

	default:
		pr_err("%s: ctrl %x not support\n", __func__, ctrl->id);
		return -EINVAL;
	}

	return 0;
}

static struct v4l2_ctrl_ops b52_sensor_ctrl_ops = {
	.g_volatile_ctrl = b52_sensor_g_ctrl,
	.s_ctrl          = b52_sensor_s_ctrl,
};

static struct v4l2_ctrl_config b52_sensor_otp_ctrl_cfg = {
	.ops = &b52_sensor_ctrl_ops,
	.id = V4L2_CID_PRIVATE_SENSOR_OTP_CONTROL,
	.name = "B52 Sensor OTP Control",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = V4L2_CID_SENSOR_OTP_CONTROL_NONE,
	.max = V4L2_CID_SENSOR_OTP_CONTROL_WB | V4L2_CID_SENSOR_OTP_CONTROL_LENC,
	.step = 1,
	.def = V4L2_CID_SENSOR_OTP_CONTROL_WB | V4L2_CID_SENSOR_OTP_CONTROL_LENC,
};


static struct v4l2_ctrl_config b52_sensor_flash_duration_cfg = {
	.ops = &b52_sensor_ctrl_ops,
	.id = V4L2_CID_PRIVATE_FLASH_DURATION,
	.name = "B52 Flash durtation",
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 0xffff,
	.step = 1,
	.def = 1,
};

static int b52_sensor_init_ctrls(struct b52_sensor *sensor)
{
	int i;
	u32 min = 0;
	u32 max = 0;
	struct v4l2_ctrl *ctrl;
	struct b52isp_sensor_ctrls *ctrls;
	struct b52_sensor_flash *flash = &sensor->flash;
	const struct b52_sensor_data *data = sensor->drvdata;

	ctrls = &sensor->ctrls;

	v4l2_ctrl_handler_init(&ctrls->ctrl_hdl, 19);

	ctrls->hflip = v4l2_ctrl_new_std(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_HFLIP, 0, 1, 1, 0);

	ctrls->vflip = v4l2_ctrl_new_std(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_VFLIP, 0, 1, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_ANALOGUE_GAIN,
			data->gain_range[B52_SENSOR_AG].min,
			data->gain_range[B52_SENSOR_AG].max, 1,
			data->gain_range[B52_SENSOR_AG].min);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_READ_ONLY;

/*FIXME: use vts not vb*/
	ctrl = v4l2_ctrl_new_std(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_VBLANK, data->vts_range.min,
			data->vts_range.max, 1, data->vts_range.min);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_READ_ONLY;

	ctrl = v4l2_ctrl_new_std(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_FOCUS_ABSOLUTE, data->focus_range.min,
			data->focus_range.max, 1, data->focus_range.min);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_READ_ONLY;

	if (sensor->drvdata->num_res > 0)
		min = max = sensor->drvdata->res[0].hts -
			sensor->drvdata->res[0].width;
	for (i = 1; i < sensor->drvdata->num_res; i++) {
		min = min_t(u32, min, sensor->drvdata->res[i].hts
				- sensor->drvdata->res[i].width);
		max = max_t(u32, max, sensor->drvdata->res[i].hts
				- sensor->drvdata->res[i].width);
	}
	ctrl = v4l2_ctrl_new_std(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_HBLANK, min, max, 1, min);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_READ_ONLY;

	ctrl = v4l2_ctrl_new_std(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_PIXEL_RATE, 0, 0, 1, 0);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_new_std_menu(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_FLASH_LED_MODE, 2, ~7,
			V4L2_FLASH_LED_MODE_NONE);

	flash->strobe_source = V4L2_FLASH_STROBE_SOURCE_SOFTWARE;
	v4l2_ctrl_new_std_menu(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_FLASH_STROBE_SOURCE, 0, ~1,
			V4L2_FLASH_STROBE_SOURCE_SOFTWARE);

	v4l2_ctrl_new_std(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_FLASH_STROBE, 0, 1, 1, 0);

	v4l2_ctrl_new_std(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_FLASH_STROBE_STOP, 0, 1, 1, 0);

	ctrl = v4l2_ctrl_new_std(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_FLASH_STROBE_STATUS, 0, 1, 1, 0);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_READ_ONLY;

	flash->timeout = FLASH_TIMEOUT_MIN;
	v4l2_ctrl_new_std(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_FLASH_TIMEOUT, FLASH_TIMEOUT_MIN,
			FLASH_TIMEOUT_MAX, FLASH_TIMEOUT_STEP,
			FLASH_TIMEOUT_MIN);

	flash->flash_current = FLASH_INTENSITY_MIN;
	v4l2_ctrl_new_std(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_FLASH_INTENSITY, FLASH_INTENSITY_MIN,
			FLASH_INTENSITY_MAX, FLASH_INTENSITY_STEP,
			FLASH_INTENSITY_MIN);

	v4l2_ctrl_new_std(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_FLASH_TORCH_INTENSITY,
			TORCH_INTENSITY_MIN, TORCH_INTENSITY_MAX,
			TORCH_INTENSITY_STEP,
			TORCH_INTENSITY_MIN);

	ctrl = v4l2_ctrl_new_std(&ctrls->ctrl_hdl,
			&b52_sensor_ctrl_ops,
			V4L2_CID_FLASH_FAULT, 0,
			V4L2_FLASH_FAULT_OVER_VOLTAGE |
			V4L2_FLASH_FAULT_TIMEOUT |
			V4L2_FLASH_FAULT_OVER_TEMPERATURE |
			V4L2_FLASH_FAULT_SHORT_CIRCUIT, 0, 0);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE |
			V4L2_CTRL_FLAG_READ_ONLY;

	v4l2_ctrl_new_custom(&ctrls->ctrl_hdl,
				&b52_sensor_otp_ctrl_cfg, NULL);

	ctrl = v4l2_ctrl_new_custom(&ctrls->ctrl_hdl, &b52_sensor_flash_duration_cfg, NULL);
	if (ctrl != NULL)
		ctrl->flags |= V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY;

	sensor->sd.ctrl_handler = &ctrls->ctrl_hdl;

	return ctrls->ctrl_hdl.error;
}

static int b52_sensor_alloc_fmt_regs(struct b52_sensor *sensor)
{
	int i;
	u32 reg_num = 0;
	u32 total_reg_num = 0;

	for (i = 0; i < sensor->drvdata->num_res; i++)
		reg_num = max_t(u32, reg_num,
			sensor->drvdata->res[i].regs.num);

	total_reg_num = reg_num;
	reg_num = 0;

	for (i = 0; i < sensor->drvdata->num_mbus_fmt; i++)
		reg_num = max_t(u32,
			reg_num, sensor->drvdata->mbus_fmt[i].regs.num);

	total_reg_num += reg_num;

	sensor->mf_regs.tab = devm_kzalloc(sensor->dev,
		total_reg_num * sizeof(struct regval_tab), GFP_KERNEL);

	if (!sensor->mf_regs.tab) {
		pr_err("%s failed\n", __func__);
		return -ENOMEM;
	}

	return 0;
}

static const struct of_device_id fimc_is_sensor_of_match[];
static const struct of_device_id b52_sensor_of_match[];

static int b52_sensor_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret = 0;
	struct device *dev = &client->dev;
	struct b52_sensor *sensor;
	const struct of_device_id *of_id;
	struct v4l2_subdev *sd;
	struct device_node *np = dev->of_node;

	of_id = of_match_node(b52_sensor_of_match, dev->of_node);
	if (!of_id)
		return -ENODEV;

	sensor = devm_kzalloc(dev, sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;

	sensor->drvdata = of_id->data;
	sensor->dev = dev;


	if (of_get_property(np, "sc2-i2c-dyn-ctrl", NULL))
		sensor->i2c_dyn_ctrl = 1;

	ret = of_property_read_u32(np, "sensor-pos", (u32 *)&sensor->pos);
	if (ret < 0) {
		dev_err(dev, "failed to get sensor position, errno %d\n", ret);
		return ret;
	}

	ret = of_property_read_u32(np, "mclk", &sensor->mclk);
	if (ret < 0) {
		dev_err(dev, "failed to get mclk, errno %d\n", ret);
		return ret;
	}

	sensor->clk = devm_clk_get(dev, "SC2MCLK");
	if (IS_ERR(sensor->clk)) {
		dev_err(dev, "failed to get SC2MCLK clock\n");
		return -ENODEV;
	}

	sensor->csi.dphy_desc.nr_lane = sensor->drvdata->nr_lane;
	if (!sensor->csi.dphy_desc.nr_lane) {
		dev_err(dev, "the csi lane number is zero\n");
		return -EINVAL;
	}
	sensor->csi.calc_dphy = sensor->drvdata->calc_dphy;
	if (!sensor->csi.calc_dphy) {
		ret = of_property_read_u32(np, "dphy3", &sensor->csi.dphy[0]);
		if (ret < 0) {
			dev_err(dev, "failed to dphy3, errno %d\n", ret);
			return ret;
		}
		ret = of_property_read_u32(np, "dphy5", &sensor->csi.dphy[1]);
		if (ret < 0) {
			dev_err(dev, "failed to dphy5, errno %d\n", ret);
			return ret;
		}
		ret = of_property_read_u32(np, "dphy6", &sensor->csi.dphy[2]);
		if (ret < 0) {
			dev_err(dev, "failed to dphy6, errno %d\n", ret);
			return ret;
		}
	}

	sd = &sensor->sd;
	v4l2_i2c_subdev_init(sd, client, &b52_sensor_sd_ops);
	sd->internal_ops = &b52_sensor_sd_internal_ops;
	snprintf(sd->name, sizeof(sd->name), sensor->drvdata->name);
	sensor->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	b52_sensor_set_defalut(sensor);

	ret = b52_detect_sensor(sensor);
	if (ret)
		return ret;

	ret = b52_sensor_alloc_fmt_regs(sensor);
	if (ret)
		return ret;

	ret = b52_sensor_init_ctrls(sensor);
	if (ret)
		goto error;

	mutex_init(&sensor->lock);

	sensor->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->entity.ops = &sensor_media_ops;
	sd->entity.type = MEDIA_ENT_T_V4L2_SUBDEV_SENSOR;
	ret = media_entity_init(&sd->entity, 1, &sensor->pad, 0);
	if (ret)
		goto error;

	return ret;
#if 0
	sensor->mf.code = data->mbus_fmt[0].mbus_code;
	sensor->mf.colorspace = data->mbus_fmt[0].colorspace;
	sensor->mf.width = data->res[0].width;
	sensor->mf.height = data->res[0].height;
	sensor->mf.filed = V4L2_FIELD_NONE;
#endif
error:
	v4l2_ctrl_handler_free(&sensor->ctrls.ctrl_hdl);

	return ret;
}

static int b52_sensor_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct b52_sensor *sensor = to_b52_sensor(sd);

	b52_sensor_call(sensor, put_power);
	v4l2_ctrl_handler_free(&sensor->ctrls.ctrl_hdl);
	media_entity_cleanup(&sd->entity);
	v4l2_device_unregister_subdev(sd);
	devm_kfree(sensor->dev, sensor);

	return 0;
}

#define DRIVER_NAME "b52-sensor"
static const struct i2c_device_id b52_sensor_ids[] = {
	{ }
};

static const struct of_device_id b52_sensor_of_match[] = {
#ifdef CONFIG_B52_CAMERA_IMX219
	{
		.compatible	= "marvell,imx219",
		.data       = &b52_imx219,
	},
#endif
#ifdef CONFIG_B52_CAMERA_OV5642
	{
		.compatible	= "marvell,ov5642",
		.data       = &b52_ov5642,
	},
#endif
#ifdef CONFIG_B52_CAMERA_OV13850
	{
		.compatible	= "marvell,ov13850",
		.data       = &b52_ov13850,
	},
#endif
#ifdef CONFIG_B52_CAMERA_OV13850R2A
	{
		.compatible	= "marvell,ov13850r2a",
		.data       = &b52_ov13850r2a,
	},
#endif

#ifdef CONFIG_B52_CAMERA_OV8858R1A
	{
		.compatible = "marvell,ov8858r1a",
		.data = &b52_ov8858r1a,
	},
#endif

#ifdef CONFIG_B52_CAMERA_OV8858R2A
	{
		.compatible = "marvell,ov8858r2a",
		.data = &b52_ov8858r2a,
	},
#endif

#ifdef CONFIG_B52_CAMERA_OV5648
	{
		.compatible = "marvell,ov5648",
		.data = &b52_ov5648,
	},
#endif
#ifdef CONFIG_B52_CAMERA_OV2680
		{
			.compatible = "marvell,ov2680",
			.data = &b52_ov2680,
		},
#endif
#ifdef CONFIG_B52_CAMERA_S5K5E3
		{
			.compatible = "marvell,s5k5e3",
			.data = &b52_s5k5e3,
		},
#endif
#ifdef CONFIG_B52_CAMERA_S5K5E2
		{
			.compatible = "marvell,s5k5e2",
			.data = &b52_s5k5e2,
		},
#endif
#ifdef CONFIG_B52_CAMERA_OV5670FF
	{
		.compatible = "marvell,ov5670ff",
		.data = &b52_ov5670ff,
	},
#endif
#ifdef CONFIG_B52_CAMERA_OV5670AF
	{
		.compatible = "marvell,ov5670af",
		.data = &b52_ov5670af,
	},
#endif
#ifdef CONFIG_B52_CAMERA_OV2740
	{
		.compatible = "marvell,ov2740",
		.data = &b52_ov2740,
	},
#endif

	{  }
};

const struct b52_sensor_data *memory_sensor_match(char *sensor_name)
{
	int i;

	if (!sensor_name) {
		pr_err("%s, parameter is NULL\n", __func__);
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(b52_sensor_of_match); i++)
		if (!strcmp(sensor_name, b52_sensor_of_match[i].compatible))
			return b52_sensor_of_match[i].data;

	return NULL;
}
EXPORT_SYMBOL(memory_sensor_match);

static struct i2c_driver b52_sensor_driver = {
	.driver = {
		.of_match_table	= b52_sensor_of_match,
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
	},
	.probe		= b52_sensor_probe,
	.remove		= b52_sensor_remove,
	.id_table	= b52_sensor_ids,
};

module_i2c_driver(b52_sensor_driver);

MODULE_DESCRIPTION("A common b52 sensor driver");
MODULE_AUTHOR("Jianle Wang <wangjl@marvell.com>");
MODULE_LICENSE("GPL");
