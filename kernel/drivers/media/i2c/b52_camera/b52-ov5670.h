/* Marvell ISP OV5670 Driver
 *
 * Copyright (C) 2009-2014 Marvell International Ltd.
 *
 * Based on mt9v011 -Micron 1/4-Inch VGA Digital Image OV5670
 *
 * Copyright (c) 2009 Mauro Carvalho Chehab (mchehab@redhat.com)
 * This code is placed under the terms of the GNU General Public License v2
 */

#ifndef	B52_OV5670_H
#define	B52_OV5670_H

#include <media/b52-sensor.h>

/*chang by customer's typical value*/
#define rg_ratio_typical 0x138
#define bg_ratio_typical 0x12D

/* raw10_default */
struct regval_tab ov5670_res_init[] = {
	{0x0103, 0x01},
	{0x0100, 0x00},
	{0x0300, 0x04},
	{0x0301, 0x00},
	{0x0302, 0x62},/*MIPI bit rate 849Mbps/lane*/
	{0x0303, 0x00},
	{0x0304, 0x03},
	{0x0305, 0x01},
	{0x0306, 0x01},
	{0x030a, 0x00},
	{0x030b, 0x00},
	{0x030c, 0x00},
	{0x030d, 0x1c},/*for sclk 104Mhz*/
	{0x030e, 0x00},
	{0x030f, 0x06},
	{0x0312, 0x01},
	{0x3000, 0x00},
	{0x3002, 0x21},
	{0x3005, 0xf0},
	{0x3007, 0x00},
	{0x3015, 0x0f},
	{0x3018, 0x32},
	{0x301a, 0xf0},
	{0x301b, 0xf0},
	{0x301c, 0xf0},
	{0x301d, 0xf0},
	{0x301e, 0xf0},
	{0x3030, 0x00},
	{0x3031, 0x0a},
	{0x303c, 0xff},
	{0x303e, 0xff},
	{0x3040, 0xf0},
	{0x3041, 0x00},
	{0x3042, 0xf0},
	{0x3106, 0x11},
	{0x3500, 0x00},
	{0x3501, 0x7b},
	{0x3502, 0x00},
	{0x3503, 0x30},
	{0x3504, 0x03},
	{0x3505, 0x80},
	{0x3507, 0x03},
	{0x3508, 0x07},
	{0x3509, 0x80},
	{0x350e, 0x04},
	{0x350f, 0x00},
	{0x3510, 0x00},
	{0x3511, 0x02},
	{0x3512, 0x00},
	{0x3601, 0xc8},
	{0x3610, 0x88},
	{0x3612, 0x48},
	{0x3614, 0x5b},
	{0x3615, 0x96},
	{0x3621, 0xd0},
	{0x3622, 0x00},
	{0x3623, 0x00},
	{0x3633, 0x13},
	{0x3634, 0x13},
	{0x3635, 0x13},
	{0x3636, 0x13},
	{0x3645, 0x13},
	{0x3646, 0x82},
	{0x3650, 0x00},
	{0x3652, 0xff},
	{0x3655, 0x20},
	{0x3656, 0xff},
	{0x365a, 0xff},
	{0x365e, 0xff},
	{0x3668, 0x00},
	{0x366a, 0x07},
	{0x366e, 0x10},
	{0x366d, 0x00},
	{0x366f, 0x80},
	{0x3700, 0x28},
	{0x3701, 0x10},
	{0x3702, 0x3a},
	{0x3703, 0x19},
	{0x3704, 0x10},
	{0x3705, 0x00},
	{0x3706, 0x66},
	{0x3707, 0x08},
	{0x3708, 0x34},
	{0x3709, 0x40},
	{0x370a, 0x01},
	{0x370b, 0x1b},
	{0x3714, 0x24},
	{0x371a, 0x3e},
	{0x3733, 0x00},
	{0x3734, 0x00},
	{0x373a, 0x05},
	{0x373b, 0x06},
	{0x373c, 0x0a},
	{0x373f, 0xa0},
	{0x3755, 0x00},
	{0x3758, 0x00},
	{0x375b, 0x0e},
	{0x3766, 0x5f},
	{0x3768, 0x00},
	{0x3769, 0x22},
	{0x3773, 0x08},
	{0x3774, 0x1f},
	{0x3776, 0x06},
	{0x37a0, 0x88},
	{0x37a1, 0x5c},
	{0x37a7, 0x88},
	{0x37a8, 0x70},
	{0x37aa, 0x88},
	{0x37ab, 0x48},
	{0x37b3, 0x66},
	{0x37c2, 0x04},
	{0x37c5, 0x00},
	{0x37c8, 0x00},
	{0x3800, 0x00},
	{0x3801, 0x0c},
	{0x3802, 0x00},
	{0x3803, 0x04},
	{0x3804, 0x0a},
	{0x3805, 0x33},
	{0x3806, 0x07},
	{0x3807, 0xa3},
	{0x3808, 0x0a},
	{0x3809, 0x20},
	{0x380a, 0x07},
	{0x380b, 0x98},
	{0x380c, 0x06},
	{0x380d, 0x8c},
	{0x380e, 0x07},
	{0x380f, 0xfd},
	{0x3811, 0x04},
	{0x3813, 0x02},
	{0x3814, 0x01},
	{0x3815, 0x01},
	{0x3816, 0x00},
	{0x3817, 0x00},
	{0x3818, 0x00},
	{0x3819, 0x00},
	{0x3820, 0x80},
	{0x3821, 0x46},
	{0x3822, 0x48},
	{0x3826, 0x00},
	{0x3827, 0x08},
	{0x382a, 0x01},
	{0x382b, 0x01},
	{0x3830, 0x08},
	{0x3836, 0x02},
	{0x3837, 0x00},
	{0x3838, 0x10},
	{0x3841, 0xff},
	{0x3846, 0x48},
	{0x3861, 0x00},
	{0x3862, 0x04},
	{0x3863, 0x06},
	{0x3a11, 0x01},
	{0x3a12, 0x78},
	{0x3b00, 0x00},
	{0x3b02, 0x00},
	{0x3b03, 0x00},
	{0x3b04, 0x00},
	{0x3b05, 0x00},
	{0x3c00, 0x89},
	{0x3c01, 0xab},
	{0x3c02, 0x01},
	{0x3c03, 0x00},
	{0x3c04, 0x00},
	{0x3c05, 0x03},
	{0x3c06, 0x00},
	{0x3c07, 0x05},
	{0x3c0c, 0x00},
	{0x3c0d, 0x00},
	{0x3c0e, 0x00},
	{0x3c0f, 0x00},
	{0x3c40, 0x00},
	{0x3c41, 0xa3},
	{0x3c43, 0x7d},
	{0x3c45, 0xd7},
	{0x3c47, 0xfc},
	{0x3c50, 0x05},
	{0x3c52, 0xaa},
	{0x3c54, 0x71},
	{0x3c56, 0x80},
	{0x3d85, 0x17},
	{0x3f03, 0x00},
	{0x3f0a, 0x00},
	{0x3f0b, 0x00},
	{0x4001, 0x60},
	{0x4009, 0x0d},
	{0x4020, 0x00},
	{0x4021, 0x00},
	{0x4022, 0x00},
	{0x4023, 0x00},
	{0x4024, 0x00},
	{0x4025, 0x00},
	{0x4026, 0x00},
	{0x4027, 0x00},
	{0x4028, 0x00},
	{0x4029, 0x00},
	{0x402a, 0x00},
	{0x402b, 0x00},
	{0x402c, 0x00},
	{0x402d, 0x00},
	{0x402e, 0x00},
	{0x402f, 0x00},
	{0x4040, 0x00},
	{0x4041, 0x03},
	{0x4042, 0x00},
	{0x4043, 0x7A},/*1/1.05 x (0x80)*/
	{0x4044, 0x00},
	{0x4045, 0x7A},
	{0x4046, 0x00},
	{0x4047, 0x7A},
	{0x4048, 0x00},
	{0x4049, 0x7A},
	{0x4303, 0x00},
	{0x4307, 0x30},
	{0x4500, 0x58},
	{0x4501, 0x04},
	{0x4502, 0x40},
	{0x4503, 0x10},
	{0x4508, 0xaa},
	{0x4509, 0xaa},
	{0x450a, 0x00},
	{0x450b, 0x00},
	{0x4600, 0x01},
	{0x4601, 0x03},
	{0x4700, 0xa4},
	{0x4800, 0x6c},
	{0x4816, 0x53},
	{0x481f, 0x40},
	{0x4837, 0x12},/*MIPI global timing*/
	{0x5000, 0x56},
	{0x5001, 0x01},
	{0x5002, 0x28},
	{0x5004, 0x0c},
	{0x5006, 0x0c},
	{0x5007, 0xe0},
	{0x5008, 0x01},
	{0x5009, 0xb0},
	{0x5901, 0x00},
	{0x5a01, 0x00},
	{0x5a03, 0x00},
	{0x5a04, 0x0c},
	{0x5a05, 0xe0},
	{0x5a06, 0x09},
	{0x5a07, 0xb0},
	{0x5a08, 0x06},
	{0x5e00, 0x00},
	{0x3734, 0x40},
	{0x5b00, 0x01},
	{0x5b01, 0x10},
	{0x5b02, 0x01},
	{0x5b03, 0xdb},
	{0x3d8c, 0x71},
	{0x3d8d, 0xea},
	{0x4017, 0x08},
	{0x3618, 0x2a},
	{0x5780, 0x3e},/*Strong DPC1.53*/
	{0x5781, 0x0f},
	{0x5782, 0x44},
	{0x5783, 0x02},
	{0x5784, 0x01},
	{0x5785, 0x01},
	{0x5786, 0x00},
	{0x5787, 0x04},
	{0x5788, 0x02},
	{0x5789, 0x0f},
	{0x578a, 0xfd},
	{0x578b, 0xf5},
	{0x578c, 0xf5},
	{0x578d, 0x03},
	{0x578e, 0x08},
	{0x578f, 0x0c},
	{0x5790, 0x08},
	{0x5791, 0x04},
	{0x5792, 0x00},
	{0x5793, 0x52},
	{0x5794, 0xa3},
	{0x3002, 0x61},/*[6]ULPM output enable*/
	{0x3010, 0x40},/*[6]enable ULPM as GPIO controlled by register*/
	{0x300D, 0x00},/*[6]ULPM output low (if 1=> high)*/
};

struct regval_tab ov5670_fmt_raw10[] = {
};

struct regval_tab ov5670_res_5M[] = {
	{0x0100, 0x00},
	{0x3501, 0x7b},/*exposore M*/
	{0x3623, 0x00},/*analog control*/
	{0x366e, 0x10},/*analog control*/
	{0x370b, 0x1b},/*sensor control*/
	{0x3808, 0x0a},/*x output size H*/
	{0x3809, 0x20},/*x output size L*/
	{0x380a, 0x07},/*y outout size H*/
	{0x380b, 0x98},/*y output size L*/
	{0x380c, 0x06},/*HTS H*/
	{0x380d, 0x8c},/*HTS L*/
	{0x380e, 0x07},/*VTS H*/
	{0x380f, 0xfd},/*VTS L*/
	{0x3814, 0x01},/*x inc odd*/
	{0x3820, 0x80},/*vflip off*/
	{0x3821, 0x46},/*hsync_en_o, mirror on, dig_bin off*/
	{0x382a, 0x01},/*y inc odd*/
	{0x4009, 0x0d},/*BLC, black line end line*/
	{0x400a, 0x02},/*BLC, offset trigger threshold H*/
	{0x400b, 0x00},/*BLC, offset trigger threshold L*/
	{0x4502, 0x40},
	{0x4508, 0xaa},
	{0x4509, 0xaa},
	{0x450a, 0x00},
	{0x4600, 0x01},
	{0x4601, 0x03},
	{0x4017, 0x08},/*BLC, offset trigger threshold*/
	{0x0100, 0x01},
};

struct regval_tab ov5670_res_quarter_5M[] = {
	{0x0100, 0x00},
	{0x3501, 0x3d},/* exposure M*/
	{0x3623, 0x00},/* analog control*/
	{0x366e, 0x08},/*analog control*/
	{0x370b, 0x1b},/*sensor control*/
	{0x3808, 0x05},/*x output size H*/
	{0x3809, 0x10},/*x output size L*/
	{0x380a, 0x03},/*y outout size H*/
	{0x380b, 0xc0},/*y output size L*/
	{0x380c, 0x06},/*HTS H*/
	{0x380d, 0x8c},/*HTS L*/
	{0x380e, 0x07},/*VTS H*/
	{0x380f, 0xfd},/*VTS L*/
	{0x3814, 0x03},/*x inc odd*/
	{0x3820, 0x90},/*vsyn48_blc on, vflip off*/
	{0x3821, 0x47},/*hsync_en_o, mirror on, dig_bin on*/
	{0x382a, 0x03},/*y inc odd*/
	{0x4009, 0x05},/*BLC, black line end line*/
	{0x400a, 0x02},/*BLC, offset trigger threshold H*/
	{0x400b, 0x00},/*BLC, offset trigger threshold L*/
	{0x4502, 0x48},
	{0x4508, 0x55},
	{0x4509, 0x55},
	{0x450a, 0x00},
	{0x4600, 0x00},
	{0x4601, 0x81},
	{0x4017, 0x10},/*BLC, offset trigger threshold*/
	{0x0100, 0x01},
};

struct regval_tab ov5670_id[] = {
	{0x300b, 0x56, 0xff},
	{0x300c, 0x70, 0xff},
};
struct regval_tab ov5670_vts[] = {
	{0x380e, 0x0d, 0x7f},
	{0x380f, 0x60, 0xff},
};
struct regval_tab ov5670_expo[] = {
	{0x3500, 0x00, 0xff},
	{0x3501, 0x20, 0xff},
	{0x3502, 0x00, 0xf0},
};
struct regval_tab ov5670_ag[] = {
	{0x3508, 0x00, 0xff},
	{0x3509, 0x80, 0xff},
};
struct regval_tab ov5670_af[] = {
};

struct regval_tab ov5670_stream_on[] = {
	{0x0100, 0x01, 0xff},
};
struct regval_tab ov5670_stream_off[] = {
	{0x0100, 0x00, 0xff},
};

struct regval_tab ov5670_vflip[] = {
};
struct regval_tab ov5670_hflip[] = {
};

struct b52_sensor_i2c_attr ov5670_i2c_attr[] = {
	[0] = {
		.reg_len = I2C_16BIT,
		.val_len = I2C_8BIT,
		.addr = 0x10,
	},
};
#define N_OV5670_I2C_ATTR ARRAY_SIZE(ov5670_i2c_attr)
#define N_OV5670_INIT ARRAY_SIZE(ov5670_res_init)
#define N_OV5670_ID ARRAY_SIZE(ov5670_id)
#define N_OV5670_FMT_RAW10 ARRAY_SIZE(ov5670_fmt_raw10)
#define N_OV5670_5M ARRAY_SIZE(ov5670_res_5M)
#define N_OV5670_QUARTER_5M ARRAY_SIZE(ov5670_res_quarter_5M)
#define N_OV5670_VTS ARRAY_SIZE(ov5670_vts)
#define N_OV5670_EXPO ARRAY_SIZE(ov5670_expo)
#define N_OV5670_AG ARRAY_SIZE(ov5670_ag)
#define N_OV5670_AF ARRAY_SIZE(ov5670_af)
#define N_OV5670_STREAM_ON ARRAY_SIZE(ov5670_stream_on)
#define N_OV5670_STREAM_OFF ARRAY_SIZE(ov5670_stream_off)
#define N_OV5670_HFLIP ARRAY_SIZE(ov5670_hflip)
#define N_OV5670_VFLIP ARRAY_SIZE(ov5670_vflip)

struct b52_sensor_mbus_fmt ov5670_fmt = {
	.mbus_code	= V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace	= V4L2_COLORSPACE_SRGB,
	.regs = {
		.tab = ov5670_fmt_raw10,
		.num = N_OV5670_FMT_RAW10,
	}
};
struct b52_sensor_resolution ov5670_res[] = {
	[0] = {
		 .width = 2592,
		 .height = 1944,
		 .hts = 0x068c*2,
		 .min_vts = 0x07fd,
		 .prop = SENSOR_RES_BINING1,
		 .regs = {
			.tab = ov5670_res_5M,
			.num = N_OV5670_5M,
		},
	},
	[1] = {
		 .width = 1296,
		 .height = 960,
		 .hts = 0x0b00,
		 .min_vts = 0x07c0,
		 .prop = SENSOR_RES_BINING2,
		 .regs = {
			.tab = ov5670_res_quarter_5M,
			.num = N_OV5670_QUARTER_5M,
		},
	},
};

static int OV5670_get_pixelclock(struct v4l2_subdev *sd, u32 *rate, u32 mclk);
static int OV5670_get_dphy_desc(struct v4l2_subdev *sd,
		struct csi_dphy_desc *dphy_desc, u32 mclk);
static int OV5670_update_otp(struct v4l2_subdev *sd,
		struct b52_sensor_otp *otp);

struct b52_sensor_spec_ops ov5670_ops = {
	.get_pixel_rate = OV5670_get_pixelclock,
	.get_dphy_desc = OV5670_get_dphy_desc,
	.update_otp = OV5670_update_otp,
	.power_on = b52_sensor_power_on,
};
struct b52_sensor_data b52_ov5670 = {
	.name = "ovt.ov5670",
	.type = OVT_SENSOR,
	.chip_ident = V4L2_IDENT_OV5670,
	.i2c_attr = ov5670_i2c_attr,
	.num_i2c_attr = N_OV5670_I2C_ATTR,
	.id = {
		.tab = ov5670_id,
		.num = N_OV5670_ID,
	},
	.global_setting = {
		.tab = ov5670_res_init,
		.num = N_OV5670_INIT,
	},
	.mbus_fmt = &ov5670_fmt,
	.num_mbus_fmt = 1,
	.res = ov5670_res,
	.num_res = 2,
	.streamon = {
		.tab = ov5670_stream_on,
		.num = N_OV5670_STREAM_ON,
	},
	.streamoff = {
		.tab = ov5670_stream_off,
		.num = N_OV5670_STREAM_OFF,
	},
	.gain2iso_ratio = {
		.numerator = 100,
		.denominator = 0x10,
	},
	.vts_range = {0x07fd, 0x7fff},
	.gain_range = {
		[B52_SENSOR_AG] = {0x0010, 0x00f8},
		[B52_SENSOR_DG] = {0x0010, 0x0010},
	},
	.expo_range = {0x00010, 0x7dd},
	.focus_range = {0x0010, 0x03ff},
	.vts_reg = {
		.tab = ov5670_vts,
		.num = N_OV5670_VTS,
	},
	.expo_reg = {
		.tab = ov5670_expo,
		.num = N_OV5670_EXPO,
	},
	.gain_reg = {
		[B52_SENSOR_AG] = {
			.tab = ov5670_ag,
			.num = N_OV5670_AG,
		},
		[B52_SENSOR_DG] = {
			.tab = NULL,
			.num = 0,
		},
	},
	.af_reg = {
		.tab = ov5670_af,
		.num = N_OV5670_AF,
	},
	.hflip = {
		.tab = ov5670_hflip,
		.num = N_OV5670_HFLIP,
	},
	.vflip = {
		.tab = ov5670_vflip,
		.num = N_OV5670_VFLIP,
	},
	.gain_shift = 3,
	.nr_lane = 2,
	.ops = &ov5670_ops,
};
#endif
