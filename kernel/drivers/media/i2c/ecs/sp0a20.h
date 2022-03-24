/*
 * sp0a20 sensor driver.
 *
 * Copyright (C) 2014 Marvell Internation Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#ifndef SP0A20_H_
#define SP0A20_H_

#include <linux/types.h>
#include <media/v4l2-common.h>
#include <media/soc_camera.h>
#include <linux/platform_data/camera-mmp.h>
#include "ecs-subdev.h"

char *sp0a20_get_profile(const struct i2c_client *client);
static int sp0a20_g_frame_interval(struct v4l2_subdev *sd,
		struct v4l2_subdev_frame_interval *inter);

static struct reg_tab_bb sp0a20_init_req[] = {
};

static struct reg_tab_bb sp0a20_init_done[] = {
	{0xfd, 0x01},
	{0x36, 0x02},
	{0xfd, 0x00},
	{0x0c, 0x00},
	{0x12, 0x02},
	{0x13, 0x2f},
	{0x6d, 0x32},
	{0x6c, 0x32},
	{0x6f, 0x33},
	{0x6e, 0x34},
	{0x99, 0x04},
	{0x16, 0x38},
	{0x17, 0x38},
	{0x70, 0x3a},
	{0x14, 0x02},
	{0x15, 0x20},
	{0x71, 0x23},
	{0x69, 0x25},
	{0x6a, 0x1a},
	{0x72, 0x1c},
	{0x75, 0x1e},
	{0x73, 0x3c},
	{0x74, 0x21},
	{0x79, 0x00},
	{0x77, 0x10},
	{0x1a, 0x4d},
	{0x1b, 0x27},
	{0x1c, 0x07},
	{0x1e, 0x15},
	{0x21, 0x0e},
	{0x22, 0x28},
	{0x26, 0x66},
	{0x28, 0x0b},
	{0x37, 0x5a},
	{0xfd, 0x02},
	{0x01, 0x80},
	{0x52, 0x1b},
	{0x53, 0x06},
	{0x54, 0x00},
	{0x56, 0x03},
	{0xfd, 0x01},
	{0x41, 0x00},
	{0x42, 0x00},
	{0x43, 0x00},
	{0x44, 0x00},
	{0xfd, 0x00},
	{0x03, 0x02},
	{0x04, 0x58},
	{0x05, 0x00},
	{0x06, 0x00},
	{0x07, 0x00},
	{0x08, 0x00},
	{0x09, 0x01},
	{0x0a, 0x64},
	{0xfd, 0x01},
	{0xf0, 0x00},
	{0xf7, 0x64},
	{0x02, 0x0e},
	{0x03, 0x01},
	{0x06, 0x64},
	{0x07, 0x00},
	{0x08, 0x01},
	{0x09, 0x00},
	{0xfd, 0x02},
	{0xbe, 0x78},
	{0xbf, 0x05},
	{0xd0, 0x78},
	{0xd1, 0x05},
	{0xfd, 0x01},
	{0x5a, 0x40},
	{0xfd, 0x02},
	{0xbc, 0x70},
	{0xbd, 0x50},
	{0xb8, 0x70},
	{0xb9, 0x90},
	{0xba, 0x30},
	{0xbb, 0x45},
	{0xfd, 0x01},
	{0xe0, 0x44},
	{0xe1, 0x36},
	{0xe2, 0x30},
	{0xe3, 0x2a},
	{0xe4, 0x2a},
	{0xe5, 0x28},
	{0xe6, 0x28},
	{0xe7, 0x26},
	{0xe8, 0x26},
	{0xe9, 0x26},
	{0xea, 0x24},
	{0xf3, 0x24},
	{0xf4, 0x24},
	{0xfd, 0x01},
	{0x04, 0xa0},
	{0x05, 0x24},
	{0x0a, 0xa0},
	{0x0b, 0x24},
	{0xfd, 0x01},
	{0xeb, 0x78},
	{0xec, 0x78},
	{0xed, 0x05},
	{0xee, 0x0a},
	{0xfd, 0x01},
	{0xf2, 0x4d},
	{0xfd, 0x02},
	{0x54, 0x0a},
	{0x5b, 0x05},
	{0x5c, 0xa0},
	{0xfd, 0x01},
	{0x26, 0x80},
	{0x27, 0x4f},
	{0x28, 0x00},
	{0x29, 0x20},
	{0x2a, 0x00},
	{0x2b, 0x03},
	{0x2c, 0x00},
	{0x2d, 0x20},
	{0x30, 0x00},
	{0x31, 0x00},
	{0xfd, 0x01},
	{0xa1, 0x36},
	{0xa2, 0x36},
	{0xa3, 0x30},
	{0xa4, 0x30},
	{0xa5, 0x2c},
	{0xa6, 0x2c},
	{0xa7, 0x2c},
	{0xa8, 0x2c},
	{0xa9, 0x2c},
	{0xaa, 0x2c},
	{0xab, 0x2c},
	{0xac, 0x2c},
	{0xad, 0x0b},
	{0xae, 0x0b},
	{0xaf, 0x0b},
	{0xb0, 0x0b},
	{0xb1, 0x00},
	{0xb2, 0x00},
	{0xb3, 0x00},
	{0xb4, 0x00},
	{0xb5, 0x00},
	{0xb6, 0x00},
	{0xb7, 0x00},
	{0xb8, 0x00},
	{0xfd, 0x02},
	{0x1d, 0x03},
	{0x1f, 0x04},
	{0x08, 0x00},
	{0x09, 0x06},
	{0x0b, 0x2f},
	{0x0d, 0x0e},
	{0xfd, 0x01},
	{0x32, 0x00},
	{0xfd, 0x02},
	{0x26, 0xa0},
	{0x27, 0xa8},
	{0x10, 0x00},
	{0x11, 0x00},
	{0x1b, 0x80},
	{0x1a, 0x80},
	{0x18, 0x27},
	{0x19, 0x26},
	{0x2a, 0x01},
	{0x2b, 0x10},
	{0x28, 0xf8},
	{0x29, 0x08},
	{0x66, 0x3b},
	{0x67, 0x5a},
	{0x68, 0xda},
	{0x69, 0xf7},
	{0x6a, 0xa5},
	{0x7c, 0x2e},
	{0x7d, 0x4b},
	{0x7e, 0xf7},
	{0x7f, 0x13},
	{0x80, 0xa6},
	{0x70, 0x1f},
	{0x71, 0x3d},
	{0x72, 0x29},
	{0x73, 0x48},
	{0x74, 0xaa},
	{0x6b, 0x13},
	{0x6c, 0x33},
	{0x6d, 0x2a},
	{0x6e, 0x4c},
	{0x6f, 0xaa},
	{0x61, 0xea},
	{0x62, 0x02},
	{0x63, 0x4f},
	{0x64, 0x72},
	{0x65, 0x6a},
	{0x75, 0x80},
	{0x76, 0x09},
	{0x77, 0x02},
	{0x24, 0x25},
	{0x0e, 0x16},
	{0x3b, 0x09},
	{0xfd, 0x02},
	{0xde, 0x0f},
	{0xd7, 0x0a},
	{0xd8, 0x0c},
	{0xd9, 0x12},
	{0xda, 0x18},
	{0xe8, 0x20},
	{0xe9, 0x20},
	{0xea, 0x20},
	{0xeb, 0x20},
	{0xec, 0x20},
	{0xed, 0x20},
	{0xee, 0x20},
	{0xef, 0x20},
	{0xd3, 0x20},
	{0xd4, 0x48},
	{0xd5, 0x20},
	{0xd6, 0x08},
	{0xfd, 0x01},
	{0xd1, 0x20},
	{0xfd, 0x02},
	{0xdc, 0x05},
	{0x05, 0x20},
	{0xfd, 0x02},
	{0x81, 0x00},
	{0xfd, 0x01},
	{0xfc, 0x00},
	{0x7d, 0x05},
	{0x7e, 0x05},
	{0x7f, 0x09},
	{0x80, 0x08},
	{0xfd, 0x02},
	{0xdd, 0x0f},
	{0xfd, 0x01},
	{0x86, 0x20},
	{0x6d, 0x12},
	{0x6e, 0x18},
	{0x6f, 0x18},
	{0x70, 0x10},
	{0x86, 0x10},
	{0x71, 0x12},
	{0x72, 0x12},
	{0x73, 0x0b},
	{0x74, 0x0b},
	{0x75, 0x08},
	{0x76, 0x0a},
	{0x77, 0x06},
	{0x78, 0x06},
	{0x79, 0x25},
	{0x7a, 0x23},
	{0x7b, 0x22},
	{0x7c, 0x00},
	{0x81, 0x0d},
	{0x82, 0x18},
	{0x83, 0x20},
	{0x84, 0x24},
	{0xfd, 0x02},
	{0x83, 0x10},
	{0x83, 0x12},
	{0x84, 0x14},
	{0x86, 0x04},
	{0xfd, 0x01},
	{0x61, 0x60},
	{0x62, 0x28},
	{0x8a, 0x10},
	{0xfd, 0x01},
	{0x8b, 0x00},
	{0x8c, 0x09},
	{0x8d, 0x17},
	{0x8e, 0x22},
	{0x8f, 0x2e},
	{0x90, 0x42},
	{0x91, 0x53},
	{0x92, 0x5f},
	{0x93, 0x6d},
	{0x94, 0x84},
	{0x95, 0x95},
	{0x96, 0xa5},
	{0x97, 0xb3},
	{0x98, 0xc0},
	{0x99, 0xcc},
	{0x9a, 0xd6},
	{0x9b, 0xdf},
	{0x9c, 0xe7},
	{0x9d, 0xee},
	{0x9e, 0xf4},
	{0x9f, 0xfa},
	{0xa0, 0xff},
	{0xfd, 0x02},
	{0x15, 0xc0},
	{0x16, 0x8c},
	{0xa0, 0xa6},
	{0xa1, 0xf4},
	{0xa2, 0xe6},
	{0xa3, 0xeb},
	{0xa4, 0xac},
	{0xa5, 0xea},
	{0xa6, 0xed},
	{0xa7, 0xcd},
	{0xa8, 0xc6},
	{0xa9, 0x3c},
	{0xaa, 0x33},
	{0xab, 0x0f},
	{0xac, 0xa6},
	{0xad, 0xda},
	{0xae, 0x00},
	{0xaf, 0xc7},
	{0xb0, 0xcc},
	{0xb1, 0xed},
	{0xb2, 0xc7},
	{0xb3, 0xd4},
	{0xb4, 0xe6},
	{0xb5, 0x0c},
	{0xb6, 0x33},
	{0xb7, 0x0f},
	{0xfd, 0x01},
	{0xd3, 0x64},
	{0xd4, 0x69},
	{0xd5, 0x54},
	{0xd6, 0x45},
	{0xd7, 0x64},
	{0xd8, 0x69},
	{0xd9, 0x54},
	{0xda, 0x45},
	{0xfd, 0x01},
	{0xdd, 0x30},
	{0xde, 0x10},
	{0xdf, 0xff},
	{0x00, 0x00},
	{0xfd, 0x01},
	{0xc2, 0xaa},
	{0xc3, 0x88},
	{0xc4, 0x77},
	{0xc5, 0x66},
	{0xfd, 0x01},
	{0xcd, 0x10},
	{0xce, 0x1f},
	{0xcf, 0x30},
	{0xd0, 0x45},
	{0xfd, 0x02},
	{0x31, 0x60},
	{0x32, 0x60},
	{0x33, 0xc0},
	{0x35, 0x60},
	{0x36, 0x30},
	{0x37, 0x13},
	{0xfd, 0x01},
	{0x0e, 0x80},
	{0x0f, 0x20},
	{0x10, 0x90},
	{0x11, 0x90},
	{0x12, 0x90},
	{0x13, 0x90},
	{0x14, 0x90},
	{0x15, 0x90},
	{0x16, 0x90},
	{0x17, 0x90},
	{0xfd, 0x00},
	{0x31, 0x00},
	{0xfd, 0x01},
	{0x32, 0x15},
	{0x33, 0xef},
	{0x34, 0x07},
	{0xd2, 0x01},
	{0xfb, 0x25},
	{0xf2, 0x49},
	{0x35, 0x00},

	{0x5d, 0x11},
	{0xfd, 0x01},
	{0x36, 0x01},
	{0x0c, 0x01},
};

static struct reg_tab_bb sp0a20_stm_on[] = {
	{0xfd, 0x00},
	{0x0c, 0x00},
	{0x92, 0x11},
	{0xfd, 0x01},
	{0x36, 0x00},
	{0xfd, 0x00},
};

static struct reg_tab_bb sp0a20_stm_off[] = {
	{0xfd, 0x00},
	{0x0c, 0x01},
	{0x92, 0x00},
	{0xfd, 0x01},
	{0x36, 0x02},
	{0xfd, 0x00},
};

static struct reg_tab_bb sp0a20_fmt_yuyv[] = {
	{0x35, 0x40},
};

static struct reg_tab_bb sp0a20_fmt_yvyu[] = {
	{0x35, 0x41},
};

static struct reg_tab_bb sp0a20_fmt_uyvy[] = {
	{0x35, 0x00},
};

static struct reg_tab_bb sp0a20_fmt_vyuy[] = {
	{0x35, 0x01},
};

static struct reg_tab_bb sp0a20_res_qcif[] = {
	{0xfd, 0x00},
	{0xa2, 0xb0},
	{0xa3, 0x00},
	{0xa4, 0x90},
	{0xa5, 0x00},
	{0xfd, 0x01},
	{0x4a, 0xa8},
	{0x4b, 0x00},
	{0x4c, 0x90},
	{0x4d, 0xe8},
	{0x4e, 0x00},
	{0x4f, 0xb0},
};
static struct reg_tab_bb sp0a20_res_qvga[] = {
	{0xfd, 0x00},
	{0xa2, 0x40},
	{0xa3, 0x01},
	{0xa4, 0xf0},
	{0xa5, 0x00},
	{0xfd, 0x01},
	{0x4a, 0x78},
	{0x4b, 0x00},
	{0x4c, 0xf0},
	{0x4d, 0xa0},
	{0x4e, 0x01},
	{0x4f, 0x40},
};

static struct reg_tab_bb sp0a20_res_vga[] = {
	{0xfd, 0x00},
	{0xa2, 0x80},
	{0xa3, 0x02},
	{0xa4, 0xe0},
	{0xa5, 0x01},
	{0xfd, 0x01},
	{0x4a, 0x00},
	{0x4b, 0x01},
	{0x4c, 0xe0},
	{0x4d, 0x00},
	{0x4e, 0x02},
	{0x4f, 0x80},
};

/*interface for mipi setting, based on 24MHz MCLK */
static __attribute__((unused)) struct reg_tab_bb sp0a20_mipi_24m_ld[] = {
};

static __attribute__((unused)) struct reg_tab_bb sp0a20_mipi_24m_full[] = {
};

/**************************** register section end ****************************/

enum {
	SP0A20_PROP_INIT = 0,		/* Initialize sequence */
	SP0A20_PROP_STM,		/* Stream on/off */
	SP0A20_PROP_FMT,		/* Output format */
	SP0A20_PROP_RES,		/* Output resolution */
	SP0A20_PROP_IF,		/* Interface behavior: a sensor output should be
						 * either MIPI or DVP, so MIPI and DVP share the
						 * same property ID */
	SP0A20_PROP_MIPI = SP0A20_PROP_IF,	/* MIPI interface behavior */
	SP0A20_PROP_DVP = SP0A20_PROP_IF,	/* DVP interface behavior */
	SP0A20_PROP_END,
};

enum {
	SP0A20_INIT_REQ	= 0,
	SP0A20_INIT_DONE,
	SP0A20_INIT_END,	/* don't use it*/
};

enum {
	SP0A20_STM_OFF	= 0,
	SP0A20_STM_ON,
	SP0A20_STM_END,	/* don't use it*/
};

enum {
	SP0A20_FMT_YUYV	= 0,
	SP0A20_FMT_YVYU,
	SP0A20_FMT_UYVY,
	SP0A20_FMT_VYUY,
	SP0A20_FMT_END,	/* don't use it*/
};

enum {
	SP0A20_RES_QCIF = 0,
	SP0A20_RES_QVGA,
	SP0A20_RES_VGA,
	SP0A20_RES_END,	/* don't use it*/
};

enum {	/* P3:021x~0x2b */
	SP0A20_IF_LD	= 0,
	SP0A20_IF_FULL,
	SP0A20_IF_END,

	SP0A20_MIPI_LD		= SP0A20_IF_LD,
	SP0A20_MIPI_FULL	= SP0A20_IF_FULL,
	SP0A20_MIPI_END		= SP0A20_IF_END,	/* don't use it*/
};

enum {
	SP0A20_ST_INVALID	= 0, /* place holder, don't use 0 as state id*/
	SP0A20_ST_UYVY_QCIF,
	SP0A20_ST_UYVY_QVGA,
	SP0A20_ST_UYVY_VGA,
	SP0A20_ST_END,	/* don't use it*/
};

static struct ecs_default_fmt_info sp0a20_fmt_info_table[] = {
	[SP0A20_FMT_YUYV] = {
		.code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.clrspc	= V4L2_COLORSPACE_JPEG,
		.fourcc	= V4L2_PIX_FMT_YUYV,
	},
	[SP0A20_FMT_YVYU] = {
		.code	= V4L2_MBUS_FMT_YVYU8_2X8,
		.clrspc	= V4L2_COLORSPACE_JPEG,
		.fourcc	= V4L2_PIX_FMT_YVYU,
	},
	[SP0A20_FMT_UYVY] = {
		.code	= V4L2_MBUS_FMT_UYVY8_2X8,
		.clrspc	= V4L2_COLORSPACE_JPEG,
		.fourcc	= V4L2_PIX_FMT_UYVY,
	},
	[SP0A20_FMT_VYUY] = {
		.code	= V4L2_MBUS_FMT_VYUY8_2X8,
		.clrspc	= V4L2_COLORSPACE_JPEG,
		.fourcc	= V4L2_PIX_FMT_VYUY,
	},
};

static struct ecs_default_res_info sp0a20_res_info_table[] = {
	[SP0A20_RES_QCIF] = {
		.h_act = 176,
		.v_act = 144,
	},
	[SP0A20_RES_QVGA] = {
		.h_act = 320,
		.v_act = 240,
	},
	[SP0A20_RES_VGA] = {
		.h_act = 640,
		.v_act = 480,
	},
};

static struct csi_dphy_desc sp0a20_mipi_24m_info_table[] = {
	[SP0A20_MIPI_LD] = { /* Controller config for low defination YUV */
		.clk_mul	= 32,
		.clk_div	= 4,
		.hs_prepare	= 27,	/* time count in clock period */
		.hs_zero	= 53,	/* time count in clock period */
		.nr_lane	= 1,
	},
	[SP0A20_MIPI_FULL] = { /* FULL */
		.clk_mul	= 32,
		.clk_div	= 4,
		.hs_prepare	= 27,	/* time count in clock period */
		.hs_zero	= 53,	/* time count in clock period */
		.nr_lane	= 1,
	},
};

#define SP0A20_DECLARE_INIT_SETTING(VAL, val) \
	__DECLARE_SETTING(SP0A20, sp0a20, INIT, init, VAL, val)
static struct ecs_setting sp0a20_init_stn_table[SP0A20_INIT_END] = {
	SP0A20_DECLARE_INIT_SETTING(REQ, req),
	SP0A20_DECLARE_INIT_SETTING(DONE, done),
};

#define SP0A20_DECLARE_STM_SETTING(VAL, val) \
	__DECLARE_SETTING(SP0A20, sp0a20, STM, stm, VAL, val)
static struct ecs_setting sp0a20_stm_stn_table[SP0A20_STM_END] = {
	SP0A20_DECLARE_STM_SETTING(OFF, off),
	SP0A20_DECLARE_STM_SETTING(ON, on),
};

#define SP0A20_DECLARE_FMT_SETTING(VAL, val) \
	__DECLARE_SETTING_VS_INFO(SP0A20, sp0a20, FMT, fmt, VAL, val)
static struct ecs_setting sp0a20_fmt_stn_table[SP0A20_FMT_END] = {
	SP0A20_DECLARE_FMT_SETTING(YUYV, yuyv),
	SP0A20_DECLARE_FMT_SETTING(YVYU, yvyu),
	SP0A20_DECLARE_FMT_SETTING(UYVY, uyvy),
	SP0A20_DECLARE_FMT_SETTING(VYUY, vyuy),
};

#define SP0A20_DECLARE_RES_SETTING(VAL, val) \
	__DECLARE_SETTING_VS_INFO(SP0A20, sp0a20, RES, res, VAL, val)
static struct ecs_setting sp0a20_res_stn_table[SP0A20_RES_END] = {
	SP0A20_DECLARE_RES_SETTING(QCIF, qcif),
	SP0A20_DECLARE_RES_SETTING(QVGA, qvga),
	SP0A20_DECLARE_RES_SETTING(VGA, vga),
};

#define SP0A20_DECLARE_MIPI_24M_SETTING(VAL, val) \
	__DECLARE_SETTING_VS_INFO(SP0A20, sp0a20, MIPI, mipi_24m, VAL, val)
static struct ecs_setting sp0a20_mipi_stn_table[SP0A20_MIPI_END] = {
	SP0A20_DECLARE_MIPI_24M_SETTING(LD, ld),
	SP0A20_DECLARE_MIPI_24M_SETTING(FULL, full),
};

static struct ecs_property sp0a20_property_table[SP0A20_PROP_END] = {
	[SP0A20_PROP_INIT] = {
		.name		= "INITIALIZE",
		.id		= SP0A20_PROP_INIT,
		.stn_tab	= sp0a20_init_stn_table,
		.stn_num	= SP0A20_INIT_END,
		.reg_low	= 0,
		.reg_high	= 0,
		.speculate	= 1,
		.value_now	= UNSET,
		.cfg_handler	= NULL,
	},
	[SP0A20_PROP_STM] = {
		.name		= "STREAM",
		.id		= SP0A20_PROP_STM,
		.stn_tab	= sp0a20_stm_stn_table,
		.stn_num	= SP0A20_STM_END,
		.reg_low	= 0,
		.reg_high	= 0,
		.speculate	= 1,
		.value_now	= UNSET,
		.cfg_handler	= NULL,
	},
	[SP0A20_PROP_FMT] = {
		.name		= "FORMAT",
		.id		= SP0A20_PROP_FMT,
		.stn_tab	= sp0a20_fmt_stn_table,
		.stn_num	= SP0A20_FMT_END,
		.reg_low	= 0,
		.reg_high	= 0,
		.speculate	= 1,
		.value_now	= UNSET,
		.cfg_handler	= NULL,
	},
	[SP0A20_PROP_RES] = {
		.name		= "RESOLUTION",
		.id		= SP0A20_PROP_RES,
		.stn_tab	= sp0a20_res_stn_table,
		.stn_num	= SP0A20_RES_END,
		.reg_low	= 0,
		.reg_high	= 0,
		.speculate	= 1,
		.value_now	= UNSET,
		.cfg_handler	= NULL,
	},
	[SP0A20_PROP_IF] = {
		.name		= "INTERFACE",
		.id		= SP0A20_PROP_IF,
		.stn_tab	= NULL,
		.stn_num	= SP0A20_IF_END,
		.reg_low	= 0,
		.reg_high	= 0,
		.speculate	= 1,
		.value_now	= UNSET,
		.cfg_handler	= NULL,
	},
};

/*********************************** state ***********************************/
/* Default global init sequence */
static struct ecs_state_cfg sp0a20_state_uyvy_qcif[] = {
	{SP0A20_PROP_FMT,	SP0A20_FMT_UYVY},
	{SP0A20_PROP_RES,	SP0A20_RES_QCIF},
	{SP0A20_PROP_IF,	SP0A20_IF_LD},
};

static struct ecs_state_cfg sp0a20_state_uyvy_qvga[] = {
	{SP0A20_PROP_FMT,	SP0A20_FMT_UYVY},
	{SP0A20_PROP_RES,	SP0A20_RES_QVGA},
	{SP0A20_PROP_IF,	SP0A20_IF_LD},
};

static struct ecs_state_cfg sp0a20_state_uyvy_vga[] = {
	{SP0A20_PROP_FMT,	SP0A20_FMT_UYVY},
	{SP0A20_PROP_RES,	SP0A20_RES_VGA},
	{SP0A20_PROP_IF,	SP0A20_IF_FULL},
};


#define SP0A20_DECLARE_STATE(VAL, val) \
	__DECLARE_STATE(SP0A20, sp0a20, VAL, val)
static struct ecs_state sp0a20_state_table[SP0A20_ST_END] = {
	SP0A20_DECLARE_STATE(UYVY_QCIF, uyvy_qcif),
	SP0A20_DECLARE_STATE(UYVY_QVGA, uyvy_qvga),
	SP0A20_DECLARE_STATE(UYVY_VGA, uyvy_vga),
};

static struct x_i2c sp0a20_xic = {
	.ident_addr	= {0x02},
	.ident_mask	= {0xFF},
	.ident_data	= {0x2b},
	.ident_regs	= 1,
	.read		= xic_read_bb,
	.write		= xic_write_bb,
	.write_array	= xic_write_array_bb,
	.write_burst	= xic_write_burst_bb,
	.detect		= xic_detect_bb,
};

/* This struct is acually the code to instantize ECS to sp0a20 driver */
static struct ecs_sensor generic_sp0a20 = {
	.name		= "sp0a20",
	.speculate	= 1,
	.prop_tab	= sp0a20_property_table,
	.prop_num	= SP0A20_PROP_END,
	.state_tab	= sp0a20_state_table,
	.state_num	= SP0A20_ST_END,
	.state_now	= UNSET,
	.hw_ctx		= &sp0a20_xic,
};

/* x_subdev related */
/* The supported format*resolutions mapping table */
static struct v4l2_mbus_framefmt sp0a20_fmt_map[SP0A20_ST_END];
static int sp0a20_enum_map[SP0A20_FMT_END * 2];

struct v4l2_subdev_video_ops sp0a20_video_ops = {
	.g_frame_interval	= sp0a20_g_frame_interval,
};

struct v4l2_subdev_ops sp0a20_ops = {
	.video	= &sp0a20_video_ops,
};



static struct x_subdev sp0a20_xsd = {
	.ecs		= &generic_sp0a20,
	.ops		= &sp0a20_ops,	/* Contain specialized functions only */
	.cid_list	= NULL,
	.cid_cnt	= 0,
	.state_map	= sp0a20_fmt_map,
	.enum_map	= sp0a20_enum_map,
	.profile	= -1,
	.init_id	= SP0A20_PROP_INIT,
	.fmt_id		= SP0A20_PROP_FMT,
	.res_id		= SP0A20_PROP_RES,
	.str_id		= SP0A20_PROP_STM,
	.ifc_id		= SP0A20_PROP_IF,
	.get_profile	= &sp0a20_get_profile,
	.get_fmt_code	= &xsd_default_get_fmt_code,
	.get_res_desc	= &xsd_default_get_res_desc,
	.get_mbus_cfg	= (int (*)(void *, struct v4l2_mbus_config *)) &csi2phy_desc_to_mbus_cfg,
};

/********************** platform specific configuration **********************/

static struct ecs_property pxa98x_spec_prop[] = {
	{
		.name		= "MIPI",
		.id		= SP0A20_PROP_MIPI,
		.stn_tab	= sp0a20_mipi_stn_table,
		.stn_num	= SP0A20_MIPI_END,
		.speculate	= 1,
	},
};

static struct ecs_sensor pxa98x_spec = {
	.name		= "pxa98x specific settings",
	.prop_tab	= pxa98x_spec_prop,
	.prop_num	= ARRAY_SIZE(pxa98x_spec_prop),
};

static __attribute__((unused)) int sp0a20_state_list[] = {
	SP0A20_ST_UYVY_QCIF,
	SP0A20_ST_UYVY_QVGA,
	SP0A20_ST_UYVY_VGA,
};

struct xsd_spec_item sp0a20_spec_list[] = {
	{
		.name		= "pxa-mipi",
		.state_list	= sp0a20_state_list,
		.state_cnt	= ARRAY_SIZE(sp0a20_state_list),
		.ecs		= &pxa98x_spec,
	},
};
/************************* Finally the big data boss *************************/
static const struct xsd_driver_id sp0a20_drv_table[] = {
	{
		.name		= "sp0a20",
		.prototype	= &sp0a20_xsd,
		.spec_list	= sp0a20_spec_list,
		.spec_cnt	= ARRAY_SIZE(sp0a20_spec_list),
	},
	/* TODO: add more driver structure above this end mark */
	{},
};

#endif
