/******************** (C) COPYRIGHT 2013 STMicroelectronics ********************
*
* File Name          : lsm9ds1_acc_gyr.c
* Authors            : MSH - C&I BU - Application Team
*                    : Giuseppe Barba (giuseppe.barba@st.com)
*                    : Matteo Dameno (matteo.dameno@st.com)
*                    : Denis Ciocca (denis.ciocca@st.com)
*                    : Both authors are willing to be considered the contact
*                    : and update points for the driver.
* Version            : V.1.0.0
* Date               : 2014/Feb/11
* Description        : LSM9DS1 accelerometer & gyroscope driver
*
********************************************************************************
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
* OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
* PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
********************************************************************************/
//#define DEBUG

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif

extern char panel_type[20];

//#include <linux/input/lsm9ds1.h>
#include <linux/lsm9ds1.h>

//#define LSM9DS1_FIFO_MODE
#ifdef LSM9DS1_FIFO_MODE
#define LSM9DS1_FIFO_SIZE		32
#endif

#define I2C_AUTO_INCREMENT		(0x80)
#define MS_TO_NS(x)			(x*1000000L)
#define REFERENCE_G			(0x0B)

/* Sensitivity */
#define SENSITIVITY_ACC_2G		(60)	/** ug/LSB */
#define SENSITIVITY_ACC_4G		(120)	/** ug/LSB */
#define SENSITIVITY_ACC_8G		(240)	/** ug/LSB */
#define SENSITIVITY_GYR_250		(8750)	/** udps/LSB */
#define SENSITIVITY_GYR_500		(17500)	/** udps/LSB */
#define SENSITIVITY_GYR_2000		(70000)	/** udps/LSB */

#define ACC_G_MAX_POS			(1495040)/** max positive value acc [ug] */
#define ACC_G_MAX_NEG			(1495770)/** max negative value acc [ug] */
#define MAG_G_MAX_POS			(983520)/** max positive value mag [ugauss] */
#define MAG_G_MAX_NEG			(983040)/** max negative value mag [ugauss] */
#define GYR_FS_MAX			(32768)

#define FUZZ				(0)
#define FLAT				(0)

#define FILTER_50			(50)/** Anti-Aliasing 50 Hz */
#define FILTER_105			(105)/** Anti-Aliasing 105 Hz */
#define FILTER_211			(211)/** Anti-Aliasing 211 Hz */
#define FILTER_408			(408)/** Anti-Aliasing 408 Hz */

#define RANGE_245DPS			(245)
#define RANGE_500DPS			(500)
#define RANGE_2000DPS			(2000)

#define ACT_THS				(0x04)
#define ACT_DUR				(0x05)
#define WHO_AM_I			(0x0F)
#define WHO_AM_I_VAL			(0x68)

/* Angular rate sensor Control Register 1 */
#define CTRL_REG1_G			(0x10)

#define BW_G_SHIFT			(0)
#define BW_G_MASK			(0x03)

#define FS_G_SHIFT			(3)
#define FS_G_MASK			(0x18) 

/* Angular rate sensor Control Register 2 */
#define CTRL_REG2_G			(0x11)
#ifdef LSM9DS1_FIFO_MODE
#define CTRL_REG2_G_DEF		(0x02)
#endif

#define OUT_SEL_SHIFT			(0)
#define OUT_SEL_MASK			(0x03)

#define INT_SEL_SHIFT			(2)
#define INT_SEL_MASK			(0x0C)

#define SEL_LPF1			(0x00)
#define SEL_HPF				(0x01)
#define SEL_LPF2			(0x02)


#define CTRL_REG3_G			(0x12)

/* Angular rate sensor sign and orientation register. */
#define ORIENT_CFG_G			(0x13)
#define ORIENT_CFG_G_SIGN_X_MASK	(0x20)
#define ORIENT_CFG_G_SIGN_Y_MASK	(0x10)
#define ORIENT_CFG_G_SIGN_Z_MASK	(0x08)
#define ORIENT_CFG_G_SIGN_ORIENT_MASK	(0x07)

#define OUT_TEMP_L			(0x15)
#define OUT_TEMP_H			(0x16)
#define STATUS_REG1			(0x17)
#define	OUT_X_L_G			(0x18) /* 1st AXIS OUT REG of 6 */

#define CTRL_REG4			(0x1E)
#define CTRL_REG4_DEF			(0x38)
#define CTRL_REG4_X_EN			(0x08)
#define CTRL_REG4_Y_EN			(0x10)
#define CTRL_REG4_Z_EN			(0x20)
#define CTRL_REG4_ALL_AXES_EN		(0x38)
#define CTRL_REG4_AXES_EN_MASK		(0x38)

#define CTRL_REG5_XL			(0x1F)
#define CTRL_REG5_XL_DEF		(0x38)

/* Linear acceleration sensor Control Register 6 */
#define CTRL_REG6_XL			(0x20)

#define LSM9DS1_ACC_FS_DEF		(LSM9DS1_ACC_FS_4G)

#define BW_SCAL_ODR_SHIFT		(2)
#define BW_SCAL_ODR_MASK		(0x04)

#define BW_XL_50			(0x0C)
#define BW_XL_105			(0x08)
#define BW_XL_211			(0x04)
#define BW_XL_408			(0x00)
#define BW_XL_DEF			(BW_XL_408)


#define CTRL_REG7_XL			(0x21)

#define CTRL_REG8			(0x22)
#define CTRL_REG8_DEF			(0x44)

#define CTRL_REG9			(0x23)
#ifdef LSM9DS1_FIFO_MODE
#define CTRL_REG9_DEF		(0x02)
#endif
#define CTRL_REG10			(0x24)


#define STATUS_REG2			(0x27)
#define OUT_X_L_XL			(0x28) /* 1st AXIS OUT REG of 6 */

#define FIFO_CTRL			(0x2E)
#define FIFO_SRC			(0x2F)
#ifdef LSM9DS1_FIFO_MODE
#define FIFO_CTRL_DEF		(0xD0) /* TH 16byte*/
#endif

/* INT1_A/G pin control register. */
#define INT1_CTRL			(0x0C)
#define INT1_CTRL_IG_G_MASK		(0x80)
#define INT1_CTRL_IG_XL_MASK		(0x40)
#define INT1_CTRL_FSS5_MASK		(0x20)
#define INT1_CTRL_OVR_MASK		(0x10)
#define INT1_CTRL_FTH_MASK		(0x08)
#define INT1_CTRL_BOOT_MASK		(0x04)
#define INT1_CTRL_DRDY_G_MASK		(0x02)
#define INT1_CTRL_DRDY_XL_MASK		(0x01)
#ifdef LSM9DS1_FIFO_MODE
#define INT1_CTRL_DEF			(0x18)
#endif

/* INT2_A/G pin control register. */
#define INT2_CTRL			(0x0D)
#define INT2_CTRL_INACT_MASK		(0x80)
#define INT2_CTRL_FSS5_MASK		(0x20)
#define INT2_CTRL_OVR_MASK		(0x10)
#define INT2_CTRL_FTH_MASK		(0x08)
#define INT2_CTRL_DRDY_TEMP_MASK	(0x04)
#define INT2_CTRL_DRDY_G_MASK		(0x02)
#define INT2_CTRL_DRDY_XL_MASK		(0x01)

/* Linear acceleration sensor interrupt source register. */
#define INT_GEN_SRC_XL			(0x26)
#define INT_GEN_SRC_XL_IA_MASK		(0x40)
#define INT_GEN_SRC_XL_ZH_MASK		(0x20)
#define INT_GEN_SRC_XL_ZL_MASK		(0x10)
#define INT_GEN_SRC_XL_YH_MASK		(0x08)
#define INT_GEN_SRC_XL_YL_MASK		(0x04)
#define INT_GEN_SRC_XL_XH_MASK		(0x02)
#define INT_GEN_SRC_XL_XL_MASK		(0x01)

/* Linear acceleration sensor interrupt generator configuration register. */
#define INT_GEN_CFG_XL			(0x06)
#define INT_GEN_CFG_XL_AOI_MASK	(0x80)
#define INT_GEN_CFG_XL_6D_MASK		(0x40)
#define INT_GEN_CFG_XL_ZHIE_MASK	(0x20)
#define INT_GEN_CFG_XL_ZLIE_MASK	(0x10)
#define INT_GEN_CFG_XL_YHIE_MASK	(0x08)
#define INT_GEN_CFG_XL_YLIE_MASK	(0x04)
#define INT_GEN_CFG_XL_XHIE_MASK	(0x02)
#define INT_GEN_CFG_XL_XLIE_MASK	(0x01)

/* Linear acceleration sensor interrupt threshold registers. */
#define INT_GEN_THS_X_XL		(0x07)
#define INT_GEN_THS_Y_XL		(0x08)
#define INT_GEN_THS_Z_XL		(0x09)

/* Linear acceleration sensor interrupt duration register. */
#define INT_GEN_DUR_XL			(0x0A)
#define INT_GEN_DUR_XL_WAIT_MASK	(0x80)
#define INT_GEN_DUR_XL_DUR_MASK	(0x7F)

/* Angular rate sensor interrupt source register. */
#define INT_GEN_SRC_G			(0x14)
#define INT_GEN_SRC_G_IA_MASK		(0x40)
#define INT_GEN_SRC_G_ZH_MASK		(0x20)
#define INT_GEN_SRC_G_ZL_MASK		(0x10)
#define INT_GEN_SRC_G_YH_MASK		(0x08)
#define INT_GEN_SRC_G_YL_MASK		(0x04)
#define INT_GEN_SRC_G_XH_MASK		(0x02)
#define INT_GEN_SRC_G_XL_MASK		(0x01)

/* Angular rate sensor interrupt generator configuration register. */
#define INT_GEN_CFG_G			(0x30)
#define INT_GEN_CFG_G_AOI_MASK		(0x80)
#define INT_GEN_CFG_G_LIR_MASK		(0x40)
#define INT_GEN_CFG_G_ZHIE_MASK	(0x20)
#define INT_GEN_CFG_G_ZLIE_MASK	(0x10)
#define INT_GEN_CFG_G_YHIE_MASK	(0x08)
#define INT_GEN_CFG_G_YLIE_MASK	(0x04)
#define INT_GEN_CFG_G_XHIE_MASK	(0x02)
#define INT_GEN_CFG_G_XLIE_MASK	(0x01)

/* Angular rate sensor interrupt generator threshold registers. */
#define INT_GEN_THS_XH_G		(0x31)
#define INT_GEN_THS_XL_G		(0x32)
#define INT_GEN_THS_YH_G		(0x33)
#define INT_GEN_THS_YL_G		(0x34)
#define INT_GEN_THS_ZH_G		(0x35)
#define INT_GEN_THS_ZL_G		(0x36)

/* Angular rate sensor interrupt generator duration register. */
#define INT_GEN_DUR_G			(0x37)
#define INT_GEN_DUR_G_WAIT_MASK	(0x80)
#define INT_GEN_DUR_G_DUR_MASK		(0x7F)

#define DEF_ZERO			(0x00)
#define UNDEF				(0x00)
#define NDTEMP				(1000)	/* Not Available temperature */

#define GET_BIT(reg,mask)		(((reg & mask) == mask) ? 1 : 0)
#define SET_BIT(reg,mask)		(reg | mask)
#define UNSET_BIT(reg,mask)		(reg & (~mask))

static struct kobject *acc_kobj;
static struct kobject *gyr_kobj;

struct workqueue_struct *lsm9ds1_acc_gyr_workqueue = 0;

#define to_dev(obj) container_of(obj, struct device, kobj)

struct output_rate{
	unsigned int cutoff_ms;
	u8 value;
}; 

static const struct output_rate lsm9ds1_gyr_odr_table[] = {
	{  1, (LSM9DS1_GYR_ODR_952 | (LSM9DS1_GYR_BW_3)) },
	{  2, (LSM9DS1_GYR_ODR_476 | (LSM9DS1_GYR_BW_3)) },
	{  4, (LSM9DS1_GYR_ODR_238 | (LSM9DS1_GYR_BW_3)) },
	{  8, (LSM9DS1_GYR_ODR_119 | (LSM9DS1_GYR_BW_3)) },
	{ 16, (LSM9DS1_GYR_ODR_59_5 | (LSM9DS1_GYR_BW_3)) },
	{ 67, (LSM9DS1_GYR_ODR_14_9) },
};

static const struct output_rate lsm9ds1_acc_odr_table[] = {
        {  1, (LSM9DS1_ACC_ODR_952) },
        {  2, (LSM9DS1_ACC_ODR_476) },
        {  4, (LSM9DS1_ACC_ODR_238) },
        {  8, (LSM9DS1_ACC_ODR_119) },
        { 20, (LSM9DS1_ACC_ODR_50) },
        { 100, (LSM9DS1_ACC_ODR_10) },
};

static const struct lsm9ds1_acc_platform_data default_lsm9ds1_acc_pdata = {
	.fs_range = LSM9DS1_ACC_FS_4G,
	.poll_interval = LSM9DS1_ACC_POLL_INTERVAL_DEF,
	.min_interval = LSM9DS1_ACC_MIN_POLL_PERIOD_MS,
	.aa_filter_bandwidth = LSM9DS1_ACC_BW_408,
};

static const struct lsm9ds1_gyr_platform_data default_lsm9ds1_gyr_pdata = {
	.fs_range = LSM9DS1_GYR_FS_2000DPS,
	.poll_interval = LSM9DS1_GYR_POLL_INTERVAL_DEF,
	.min_interval = LSM9DS1_GYR_MIN_POLL_PERIOD_MS,
};

struct lsm9ds1_acc_gyr_main_platform_data default_lsm9ds1_main_platform_data = {
	.rot_matrix = {
		{1, 0, 0},
		{0, 1, 0},
		{0, 0, 1},
	},
	.gpio_int1 = LSM9DS1_INT1_GPIO_DEF,
	.gpio_int2 = LSM9DS1_INT2_GPIO_DEF,
};

struct interrupt_enable {
	atomic_t enable;
	u8 address;
	u8 mask;
};

struct interrupt_value {
	int value;
	u8 address;
};

#ifdef LSM9DS1_FIFO_MODE
#pragma pack(1)
struct lsm9ds1_data_axis {
	s64 x;
	s64 y;
	s64 z;
};
struct lsm9ds1_fifo{
	s64 count;
	s64 period;
	s64 timestamp;
	struct lsm9ds1_data_axis fifo_data[LSM9DS1_FIFO_SIZE];
};
#pragma pack()
#endif

struct lsm9ds1_acc_gyr_status {
	struct i2c_client *client;
	struct lsm9ds1_acc_gyr_main_platform_data *pdata_main;
	struct lsm9ds1_acc_platform_data *pdata_acc;
	struct lsm9ds1_gyr_platform_data *pdata_gyr;

	struct mutex lock;
#ifndef LSM9DS1_FIFO_MODE
	struct work_struct input_work_acc;
	struct work_struct input_work_gyr;
#else
	struct work_struct input_work;
#endif

#ifndef LSM9DS1_FIFO_MODE
	struct hrtimer hr_timer_acc;
	ktime_t ktime_acc;
	struct hrtimer hr_timer_gyr;
	ktime_t ktime_gyr;
#endif

	struct input_dev *input_dev_acc;
	struct input_dev *input_dev_gyr;
	//struct input_dev *input_dev_temp;

	int8_t hw_initialized;
	/* hw_working=-1 means not tested yet */
	int8_t hw_working;

	atomic_t enabled_acc;
	atomic_t enabled_acc_r;
	atomic_t enabled_gyr;
	atomic_t enabled_temp;

	int32_t temp_value_dec;
	uint32_t temp_value_flo;

	int32_t on_before_suspend;
	int32_t use_smbus;

	uint16_t sensitivity_acc;
	uint32_t sensitivity_gyr;

	int irq1;
	struct work_struct irq1_work;
	struct workqueue_struct *irq1_work_queue;

	int irq2;
	struct work_struct irq2_work;
	struct workqueue_struct *irq2_work_queue;
#ifdef LSM9DS1_FIFO_MODE
	struct lsm9ds1_fifo fifo;
	atomic_t fifo_ready;
	wait_queue_head_t fifo_wq;

	struct lsm9ds1_fifo fifo_gyr;
	atomic_t fifo_gyr_ready;
	wait_queue_head_t fifo_gyr_wq;

	unsigned int acc_cutoff_ms;
	unsigned int gyr_cutoff_ms;
	
	s64 period_rel;
	int fifo_wakeup;
	int fifo_timeout;
#endif
};

static struct lsm9ds1_acc_gyr_status * p_lsm9ds1_data = NULL;

static int lsm9ds1_acc_disable(struct lsm9ds1_acc_gyr_status*);

struct reg_rw {
	u8 address;
	u8 default_val;
	u8 resume_val;
};

struct reg_r {
	u8 address;
	u8 default_val;
};

static struct status_registers {
	struct reg_rw act_ths;
	struct reg_rw act_dur;
	struct reg_rw int_gen_cfg_xl;
	struct reg_rw int_gen_ths_x_xl;
	struct reg_rw int_gen_ths_y_xl;
	struct reg_rw int_gen_ths_z_xl;
	struct reg_rw int_gen_dur_xl;
	struct reg_rw reference_g;
	struct reg_rw int1_ctrl;
	struct reg_rw int2_ctrl;
	struct reg_r who_am_i;
	struct reg_rw ctrl_reg1_g;
	struct reg_rw ctrl_reg2_g;
	struct reg_rw ctrl_reg3_g;
	struct reg_rw orient_cfg_g;
	struct reg_r int_gen_src_g;
	struct reg_r status_reg1;
	struct reg_rw ctrl_reg4;
	struct reg_rw ctrl_reg5_xl;
	struct reg_rw ctrl_reg6_xl;
	struct reg_rw ctrl_reg7_xl;
	struct reg_rw ctrl_reg8;
	struct reg_rw ctrl_reg9;
	struct reg_rw ctrl_reg10;	
	struct reg_r int_gen_src_xl;
	struct reg_r status_reg2;
	struct reg_rw fifo_ctrl;
	struct reg_r fifo_src;
	struct reg_rw int_gen_cfg_g;
	struct reg_rw int_gen_ths_xh_g;
	struct reg_rw int_gen_ths_xl_g;
	struct reg_rw int_gen_ths_yh_g;
	struct reg_rw int_gen_ths_yl_g;
	struct reg_rw int_gen_ths_zh_g;
	struct reg_rw int_gen_ths_zl_g;
	struct reg_rw int_gen_dur_g;
} status_registers = {
	.act_ths =
		{.address = ACT_THS, 		.default_val = DEF_ZERO,},
	.act_dur =
		{.address = ACT_DUR, 		.default_val = DEF_ZERO,},
	.int_gen_cfg_xl =
		{.address = INT_GEN_CFG_XL, 	.default_val = DEF_ZERO,},
	.int_gen_ths_x_xl =
		{.address = INT_GEN_THS_X_XL, 	.default_val = DEF_ZERO,},
	.int_gen_ths_y_xl =
		{.address = INT_GEN_THS_Y_XL, 	.default_val = DEF_ZERO,},
	.int_gen_ths_z_xl =
		{.address = INT_GEN_THS_Z_XL, 	.default_val = DEF_ZERO,},
	.int_gen_dur_xl = 
		{.address = INT_GEN_DUR_XL, 	.default_val = DEF_ZERO,},
	.reference_g =
		{.address = REFERENCE_G, 	.default_val = DEF_ZERO,},
	.int1_ctrl =
		{.address = INT1_CTRL,		.default_val = DEF_ZERO,},
	.int2_ctrl =
		{.address = INT2_CTRL,		.default_val = DEF_ZERO,},
	.who_am_i =
		{.address = WHO_AM_I,		.default_val = WHO_AM_I_VAL,},
	.ctrl_reg1_g =
		{.address = CTRL_REG1_G,	.default_val = DEF_ZERO,},
	.ctrl_reg2_g =
#ifndef LSM9DS1_FIFO_MODE
		{.address = CTRL_REG2_G,	.default_val = DEF_ZERO,},
#else
		{.address = CTRL_REG2_G,	.default_val = CTRL_REG2_G_DEF,},
#endif
	.ctrl_reg3_g =
		{.address = CTRL_REG3_G,	.default_val = DEF_ZERO,},
	.orient_cfg_g =
		{.address = ORIENT_CFG_G,	.default_val = DEF_ZERO,},
	.int_gen_src_g =
		{.address = INT_GEN_SRC_G,	.default_val = UNDEF,},
	.status_reg1 =
		{.address = STATUS_REG1,	.default_val = UNDEF,},
	.ctrl_reg4 =
		{.address = CTRL_REG4,		.default_val = CTRL_REG4_DEF,},
	.ctrl_reg5_xl =
		{.address = CTRL_REG5_XL,	.default_val = CTRL_REG5_XL_DEF,},
	.ctrl_reg6_xl =
		{.address = CTRL_REG6_XL,	.default_val = DEF_ZERO,},
	.ctrl_reg7_xl =
		{.address = CTRL_REG7_XL,	.default_val = DEF_ZERO,},
	.ctrl_reg8 =
		{.address = CTRL_REG8,		.default_val = CTRL_REG8_DEF,},
	.ctrl_reg9 =
#ifndef LSM9DS1_FIFO_MODE
		{.address = CTRL_REG9,		.default_val = DEF_ZERO,},
#else
		{.address = CTRL_REG9,		.default_val = CTRL_REG9_DEF,},
#endif
	.ctrl_reg10 =
		{.address = CTRL_REG10,	.default_val = DEF_ZERO,},
	.int_gen_src_xl =
		{.address = INT_GEN_SRC_XL,	.default_val = DEF_ZERO,},
	.status_reg2 =
		{.address = STATUS_REG2,	.default_val = UNDEF,},
	.fifo_ctrl =
#ifndef LSM9DS1_FIFO_MODE
		{.address = FIFO_CTRL,		.default_val = DEF_ZERO,},
#else
		{.address = FIFO_CTRL,		.default_val = FIFO_CTRL_DEF,},
#endif
	.fifo_src =
		{.address = FIFO_SRC,		.default_val = UNDEF,},
	.int_gen_cfg_g =
		{.address = INT_GEN_CFG_G,	.default_val = DEF_ZERO,},
	.int_gen_ths_xh_g =
		{.address = INT_GEN_THS_XH_G,	.default_val = DEF_ZERO,},
	.int_gen_ths_xl_g =
		{.address = INT_GEN_THS_XL_G,	.default_val = DEF_ZERO,},
	.int_gen_ths_yh_g =
		{.address = INT_GEN_THS_YH_G,	.default_val = DEF_ZERO,},
	.int_gen_ths_yl_g =
		{.address = INT_GEN_THS_YL_G,	.default_val = DEF_ZERO,},
	.int_gen_ths_zh_g =
		{.address = INT_GEN_THS_ZH_G,	.default_val = DEF_ZERO,},
	.int_gen_ths_zl_g =
		{.address = INT_GEN_THS_ZL_G,	.default_val = DEF_ZERO,},
	.int_gen_dur_g =
		{.address = INT_GEN_DUR_G,	.default_val = DEF_ZERO,},
};
/*****************************************************************************/

static int lsm9ds1_i2c_write(struct lsm9ds1_acc_gyr_status *stat, u8 *buf, int len)
{
	int ret;
	u8 reg = buf[0];
	u8 value;
#ifdef DEBUG
	unsigned int ii;
#endif
	struct i2c_msg msg = {
		.addr = stat->client->addr,
		.flags = 0,
		.len = len + 1,
		.buf = buf,
	};

	if (len > 1)
		reg |= I2C_AUTO_INCREMENT;

	value = buf[1];

	if (stat->use_smbus) {
		if (len == 1) {
			ret = i2c_smbus_write_byte_data(stat->client,
								reg, value);
#ifdef DEBUG
			dev_warn(&stat->client->dev,
				"i2c_smbus_write_byte_data: ret=%d, len:%d, "
				"command=0x%02x, value=0x%02x\n",
				ret, len, reg , value);
#endif
			return ret;
		} else if (len > 1) {
			ret = i2c_smbus_write_i2c_block_data(stat->client,
							reg, len, buf + 1);
#ifdef DEBUG
			dev_warn(&stat->client->dev,
				"i2c_smbus_write_i2c_block_data: ret=%d, "
				"len:%d, command=0x%02x, ",
				ret, len, reg);
			for (ii = 0; ii < (len + 1); ii++)
				printk(KERN_DEBUG "value[%d]=0x%02x,",
								ii, buf[ii]);

			printk("\n");
#endif
			return ret;
		}
	}

	ret = i2c_transfer(stat->client->adapter, &msg, 1);

	return (ret == 1) ? 0 : 1;
}

static int lsm9ds1_i2c_read(struct lsm9ds1_acc_gyr_status *stat, u8 *buf,
								  int len)
{
	int ret;
	u8 cmd = buf[0];
#ifdef DEBUG
	unsigned int ii;
#endif
	struct i2c_msg msgs[] = {
		{
			.addr = stat->client->addr,
			.flags = 0,
			.len = 1,
			.buf = buf,
		},
		{
			.addr = stat->client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		}
	};

	if (len > 1)
		cmd |= I2C_AUTO_INCREMENT;

	if (stat->use_smbus) {
		if (len == 1) {
			ret = i2c_smbus_read_byte_data(stat->client, cmd);
			buf[0] = ret & 0xff;
#ifdef DEBUG
			dev_warn(&stat->client->dev,
				"i2c_smbus_read_byte_data: ret=0x%02x, len:%d ,"
				"command=0x%02x, buf[0]=0x%02x\n",
				ret, len, cmd , buf[0]);
#endif
		} else if (len > 1) {
			ret = i2c_smbus_read_i2c_block_data(stat->client,
								cmd, len, buf);
#ifdef DEBUG
			dev_warn(&stat->client->dev,
				"i2c_smbus_read_i2c_block_data: ret:%d len:%d, "
				"command=0x%02x, ",
				ret, len, cmd);
			for (ii = 0; ii < len; ii++)
				printk(KERN_DEBUG "buf[%d]=0x%02x,",
								ii, buf[ii]);

			printk("\n");
#endif
		} else
			ret = -1;

		if (ret < 0) {
			dev_err(&stat->client->dev,
				"read transfer error: len:%d, command=0x%02x\n",
				len, cmd);
			return 0;
		}
		return len;
	}

	ret = i2c_transfer(stat->client->adapter, msgs, 2);

	return (ret == 2) ? 0 : 1;
}

static int lsm9ds1_acc_device_power_off(struct lsm9ds1_acc_gyr_status *stat)
{
	int err;
	u8 buf[2];

	buf[0] = status_registers.ctrl_reg6_xl.address;
	buf[1] = (LSM9DS1_ACC_ODR_MASK & LSM9DS1_ACC_ODR_OFF) |
	((~LSM9DS1_ACC_ODR_MASK) & status_registers.ctrl_reg6_xl.resume_val);

	err = lsm9ds1_i2c_write(stat, buf, 1);

	if (err < 0)
		dev_err(&stat->client->dev, "accelerometer soft power off "
							"failed: %d\n", err);

	if (stat->pdata_acc->power_off) {
		stat->pdata_acc->power_off();
	}

	atomic_set(&stat->enabled_acc, 0);
	dev_info(&stat->client->dev, "accelerometer switched off.");

	return 0;
}

static int lsm9ds1_gyr_device_power_off(struct lsm9ds1_acc_gyr_status *stat)
{
	int err;
	u8 buf[2];

	buf[0] = status_registers.ctrl_reg1_g.address;
	buf[1] = (LSM9DS1_GYR_ODR_MASK & LSM9DS1_GYR_ODR_OFF) |
	((~LSM9DS1_GYR_ODR_MASK) & status_registers.ctrl_reg1_g.resume_val);

	err = lsm9ds1_i2c_write(stat, buf, 1);
	
	if (err < 0)
		dev_err(&stat->client->dev, "gyroscope soft power off "
							"failed: %d\n", err);

	if (stat->pdata_gyr->power_off) {
		stat->pdata_gyr->power_off();
	}

	atomic_set(&stat->enabled_gyr, 0);
	dev_info(&stat->client->dev, "gyroscope switched off.");

	return 0;
}

static int lsm9ds1_gyr_disable(struct lsm9ds1_acc_gyr_status *stat)
{
	if (atomic_cmpxchg(&stat->enabled_gyr, 1, 0)) {
#ifndef LSM9DS1_FIFO_MODE
		cancel_work_sync(&stat->input_work_gyr);
		hrtimer_cancel(&stat->hr_timer_gyr);
		lsm9ds1_gyr_device_power_off(stat);
#else
		lsm9ds1_gyr_device_power_off(stat);
#endif
	}

	return 0;
}

static int lsm9ds1_acc_disable(struct lsm9ds1_acc_gyr_status *stat)
{
	if (atomic_cmpxchg(&stat->enabled_acc, 1, 0)) {
#ifndef LSM9DS1_FIFO_MODE
		cancel_work_sync(&stat->input_work_acc);
		hrtimer_cancel(&stat->hr_timer_acc);
#endif
		lsm9ds1_acc_device_power_off(stat);

		if(stat->pdata_main->gpio_int1 >= 0)
			disable_irq_nosync(stat->irq1);
//		if(stat->pdata_main->gpio_int2 >= 0)
//			disable_irq_nosync(stat->irq2);
	}

	return 0;
}

static void lsm9ds1_acc_input_cleanup(struct lsm9ds1_acc_gyr_status *stat)
{
	input_unregister_device(stat->input_dev_acc);
	input_free_device(stat->input_dev_acc);
}

static void lsm9ds1_gyr_input_cleanup(struct lsm9ds1_acc_gyr_status *stat)
{
	input_unregister_device(stat->input_dev_gyr);
	input_free_device(stat->input_dev_gyr);
}

#ifndef LSM9DS1_FIFO_MODE
enum hrtimer_restart poll_function_read_acc(struct hrtimer *timer)
{
	struct lsm9ds1_acc_gyr_status *stat;


	stat = container_of((struct hrtimer *)timer,
				struct lsm9ds1_acc_gyr_status, hr_timer_acc);

	queue_work(lsm9ds1_acc_gyr_workqueue, &stat->input_work_acc);

	hrtimer_forward_now(&stat->hr_timer_acc, stat->ktime_acc);
	return HRTIMER_RESTART;
}

enum hrtimer_restart poll_function_read_gyr(struct hrtimer *timer)
{
	struct lsm9ds1_acc_gyr_status *stat;


	stat = container_of((struct hrtimer *)timer,
				struct lsm9ds1_acc_gyr_status, hr_timer_gyr);

	queue_work(lsm9ds1_acc_gyr_workqueue, &stat->input_work_gyr);

	hrtimer_forward_now(&stat->hr_timer_gyr, stat->ktime_gyr);
	return HRTIMER_RESTART;
}
#endif

static void lsm9ds1_validate_polling(unsigned int *min_interval,
					unsigned int *poll_interval,
					unsigned int min)
{
	*min_interval = max(min, *min_interval);
	*poll_interval = max(*poll_interval, *min_interval);
}

static int lsm9ds1_acc_validate_pdata(struct lsm9ds1_acc_gyr_status *stat)
{
	int res = -EINVAL;

	lsm9ds1_validate_polling(&stat->pdata_acc->min_interval,
				 &stat->pdata_acc->poll_interval,
				(unsigned int)LSM9DS1_ACC_MIN_POLL_PERIOD_MS);

	switch (stat->pdata_acc->aa_filter_bandwidth) {
	case LSM9DS1_ACC_BW_50:
		res = 1;
		break;
	case LSM9DS1_ACC_BW_105:
		res = 1;
		break;
	case LSM9DS1_ACC_BW_211:
		res = 1;
		break;
	case LSM9DS1_ACC_BW_408:
		res = 1;
		break;
	default:
		dev_err(&stat->client->dev, "invalid accelerometer "
			"bandwidth selected: %u\n",
				stat->pdata_acc->aa_filter_bandwidth);
	}

	return res;
}

static int lsm9ds1_gyr_validate_pdata(struct lsm9ds1_acc_gyr_status *stat)
{
	/* checks for correctness of minimal polling period */
	lsm9ds1_validate_polling(&stat->pdata_gyr->min_interval, 
				 &stat->pdata_gyr->poll_interval,
				(unsigned int)LSM9DS1_GYR_MIN_POLL_PERIOD_MS);

	/* Enforce minimum polling interval */
	if (stat->pdata_gyr->poll_interval < stat->pdata_gyr->min_interval) {
		dev_err(&stat->client->dev,
			"minimum poll interval violated\n");
		return -EINVAL;
	}
	return 0;
}


static int lsm9ds1_acc_gyr_hw_init(struct lsm9ds1_acc_gyr_status *stat)
{
	int err = -1;
	u8 buf[1];

	dev_info(&stat->client->dev, "%s: hw init start\n", LSM9DS1_ACC_GYR_DEV_NAME);

	buf[0] = status_registers.who_am_i.address;
	err = lsm9ds1_i2c_read(stat, buf, 1);
	
	if (err < 0) {
		dev_warn(&stat->client->dev, "Error reading WHO_AM_I: is device"
		" available/working?\n");
		goto err_firstread;
	} else
		stat->hw_working = 1;

	if (buf[0] != status_registers.who_am_i.default_val) {
	dev_err(&stat->client->dev,
		"device unknown. Expected: 0x%02x,"
		" Replies: 0x%02x\n", status_registers.who_am_i.default_val, 
					buf[0]);
		err = -1;
		goto err_unknown_device;
	}

	status_registers.act_ths.resume_val =
				status_registers.act_ths.default_val;
	status_registers.act_dur.resume_val =
				status_registers.act_dur.default_val;
	status_registers.int_gen_cfg_xl.resume_val =
				status_registers.int_gen_cfg_xl.default_val;
	status_registers.int_gen_ths_x_xl.resume_val =
				status_registers.int_gen_ths_x_xl.default_val;
	status_registers.int_gen_ths_y_xl.resume_val =
				status_registers.int_gen_ths_y_xl.default_val;
	status_registers.int_gen_ths_z_xl.resume_val =
				status_registers.int_gen_ths_z_xl.default_val;
	status_registers.int_gen_dur_xl.resume_val =
				status_registers.int_gen_dur_xl.default_val;
	status_registers.reference_g.resume_val =
				status_registers.reference_g.default_val;
	status_registers.int1_ctrl.resume_val =
				status_registers.int1_ctrl.default_val;
	status_registers.int2_ctrl.resume_val =
				status_registers.int2_ctrl.default_val;
	status_registers.ctrl_reg1_g.resume_val =
				status_registers.ctrl_reg1_g.default_val;
	status_registers.ctrl_reg2_g.resume_val =
				status_registers.ctrl_reg2_g.default_val;
	status_registers.ctrl_reg3_g.resume_val =
				status_registers.ctrl_reg3_g.default_val;
	status_registers.orient_cfg_g.resume_val =
				status_registers.orient_cfg_g.default_val;
	status_registers.ctrl_reg4.resume_val =
				status_registers.ctrl_reg4.default_val;
	status_registers.ctrl_reg5_xl.resume_val =
				status_registers.ctrl_reg5_xl.default_val;
	status_registers.ctrl_reg6_xl.resume_val =
				status_registers.ctrl_reg6_xl.default_val;
	status_registers.ctrl_reg7_xl.resume_val =
				status_registers.ctrl_reg7_xl.default_val;
	status_registers.ctrl_reg8.resume_val =
				status_registers.ctrl_reg8.default_val;
	status_registers.ctrl_reg9.resume_val =
				status_registers.ctrl_reg9.default_val;
	status_registers.ctrl_reg10.resume_val =
				status_registers.ctrl_reg10.default_val;
	status_registers.fifo_ctrl.resume_val =
				status_registers.fifo_ctrl.default_val;
	status_registers.int_gen_cfg_g.resume_val =
				status_registers.int_gen_cfg_g.default_val;
	status_registers.int_gen_ths_xh_g.resume_val =
				status_registers.int_gen_ths_xh_g.default_val;
	status_registers.int_gen_ths_xl_g.resume_val =
				status_registers.int_gen_ths_xl_g.default_val;
	status_registers.int_gen_ths_yh_g.resume_val =
				status_registers.int_gen_ths_yh_g.default_val;
	status_registers.int_gen_ths_yl_g.resume_val =
				status_registers.int_gen_ths_yl_g.default_val;
	status_registers.int_gen_ths_zh_g.resume_val =
				status_registers.int_gen_ths_zh_g.default_val;
	status_registers.int_gen_ths_zl_g.resume_val =
				status_registers.int_gen_ths_zl_g.default_val;
	status_registers.int_gen_dur_g.resume_val =
				status_registers.int_gen_dur_g.default_val;

	stat->temp_value_dec = NDTEMP;

	stat->hw_initialized = 1;
	dev_info(&stat->client->dev, "%s: hw init done\n", LSM9DS1_ACC_GYR_DEV_NAME);

	return 0;

err_unknown_device:
err_firstread:
	stat->hw_working = 0;
	stat->hw_initialized = 0;
	return err;
}

static int lsm9ds1_acc_device_power_on(struct lsm9ds1_acc_gyr_status *stat)
{
	int err = -1;
	u8 buf[9];

	if (stat->pdata_acc->power_on) {
		err = stat->pdata_acc->power_on();
		if (err < 0) {
			dev_err(&stat->client->dev,
				"accelerometer power_on failed: %d\n", err);
			return err;
		}
	}

	buf[0] = status_registers.ctrl_reg4.address;
 	buf[1] = status_registers.ctrl_reg4.resume_val;
 	buf[2] = status_registers.ctrl_reg5_xl.resume_val;
	buf[3] = status_registers.ctrl_reg6_xl.resume_val;
	buf[4] = status_registers.ctrl_reg7_xl.resume_val;
	buf[5] = status_registers.ctrl_reg8.resume_val;
	buf[6] = status_registers.ctrl_reg9.resume_val;
	buf[7] = status_registers.ctrl_reg10.resume_val;
	err = lsm9ds1_i2c_write(stat, buf, 7);
	if (err < 0)
		goto err_resume_state;
	
	buf[0] = status_registers.int_gen_cfg_xl.address;
	buf[1] = status_registers.int_gen_cfg_xl.resume_val;
	buf[2] = status_registers.int_gen_ths_x_xl.resume_val;
	buf[3] = status_registers.int_gen_ths_y_xl.resume_val;
	buf[4] = status_registers.int_gen_ths_z_xl.resume_val;
	buf[5] = status_registers.int_gen_dur_xl.resume_val;
	err = lsm9ds1_i2c_write(stat, buf, 5);
	if (err < 0)
		goto err_resume_state;
	
	buf[0] = status_registers.int1_ctrl.address;
	buf[1] = status_registers.int1_ctrl.resume_val;
	buf[2] = status_registers.int2_ctrl.resume_val;
	err = lsm9ds1_i2c_write(stat, buf, 2);
	if (err < 0)
		goto err_resume_state;
	
	buf[0] = status_registers.fifo_ctrl.address;
	buf[1] = status_registers.fifo_ctrl.resume_val;
	err = lsm9ds1_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;
	
	buf[0] = status_registers.ctrl_reg8.address;
	buf[1] = status_registers.ctrl_reg8.resume_val;
	buf[2] = status_registers.ctrl_reg9.resume_val;
	err = lsm9ds1_i2c_write(stat, buf, 2);
	if (err < 0)
		goto err_resume_state;

	atomic_set(&stat->enabled_acc, 1);

	return 0;

err_resume_state:
	atomic_set(&stat->enabled_acc, 0);
	dev_err(&stat->client->dev, "accelerometer hw power on error "
				"0x%02x,0x%02x: %d\n", buf[0], buf[1], err);
	return err;
}

static int lsm9ds1_gyr_device_power_on(struct lsm9ds1_acc_gyr_status *stat)
{
	int err = -1;
	u8 buf[9];


	if (stat->pdata_gyr->power_on) {
		err = stat->pdata_gyr->power_on();
		if (err < 0) {
			dev_err(&stat->client->dev,
				"gyroscope power_on failed: %d\n", err);
			return err;
		}
	}

	buf[0] = status_registers.act_ths.address;
	buf[1] = status_registers.act_ths.resume_val;
	err = lsm9ds1_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;
	
	buf[0] = status_registers.reference_g.address;
	buf[1] = status_registers.reference_g.resume_val;
	err = lsm9ds1_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = status_registers.ctrl_reg1_g.address;
	buf[1] = status_registers.ctrl_reg1_g.resume_val;
	buf[2] = status_registers.ctrl_reg2_g.resume_val;
	buf[3] = status_registers.ctrl_reg3_g.resume_val;
	buf[4] = status_registers.orient_cfg_g.resume_val;
	err = lsm9ds1_i2c_write(stat, buf, 4);
	if (err < 0)
		goto err_resume_state;

	buf[0] = status_registers.ctrl_reg4.address;
	buf[1] = status_registers.ctrl_reg4.resume_val;
	err = lsm9ds1_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = status_registers.int_gen_cfg_g.address;
	buf[1] = status_registers.int_gen_cfg_g.resume_val;
	buf[2] = status_registers.int_gen_ths_xh_g.resume_val;
	buf[3] = status_registers.int_gen_ths_xl_g.resume_val;
	buf[4] = status_registers.int_gen_ths_yh_g.resume_val;
	buf[5] = status_registers.int_gen_ths_yl_g.resume_val;
	buf[6] = status_registers.int_gen_ths_zh_g.resume_val;
	buf[7] = status_registers.int_gen_ths_zl_g.resume_val;
	buf[8] = status_registers.int_gen_dur_g.resume_val;
	err = lsm9ds1_i2c_write(stat, buf, 8);
	if (err < 0)
		goto err_resume_state;

	buf[0] = status_registers.int1_ctrl.address;
	buf[1] = status_registers.int1_ctrl.resume_val;
	buf[2] = status_registers.int2_ctrl.resume_val;
	err = lsm9ds1_i2c_write(stat, buf, 2);
	if (err < 0)
		goto err_resume_state;

	buf[0] = status_registers.fifo_ctrl.address;
	buf[1] = status_registers.fifo_ctrl.resume_val;
	err = lsm9ds1_i2c_write(stat, buf, 1);
	if (err < 0)
		goto err_resume_state;

	atomic_set(&stat->enabled_gyr, 1);

	return 0;

err_resume_state:
	atomic_set(&stat->enabled_gyr, 0);
	dev_err(&stat->client->dev, "gyroscope hw power on error "
				"0x%02x,0x%02x: %d\n", buf[0], buf[1], err);
	return err;
}

static int lsm9ds1_acc_update_fs_range(struct lsm9ds1_acc_gyr_status *stat,
								u8 new_fs_range)
{
	int err = -1;

	u16 sensitivity;
	u8 val;
	u8 buf[2];

	switch (new_fs_range) {
	case LSM9DS1_ACC_FS_2G:
		sensitivity = SENSITIVITY_ACC_2G;
		break;
	case LSM9DS1_ACC_FS_4G:
		sensitivity = SENSITIVITY_ACC_4G;
		break;
	case LSM9DS1_ACC_FS_8G:
		sensitivity = SENSITIVITY_ACC_8G;
		break;
	default:
		dev_err(&stat->client->dev, "invalid accelerometer "
				"fs range requested: %u\n", new_fs_range);
		return -EINVAL;
	}

	val = ((LSM9DS1_ACC_FS_MASK & new_fs_range) | ((~LSM9DS1_ACC_FS_MASK) & 
				status_registers.ctrl_reg6_xl.resume_val));

	buf[0] = status_registers.ctrl_reg6_xl.address;
	buf[1] = val;

	err = lsm9ds1_i2c_write(stat, buf, 1);
	if (err < 0)
		goto error;

	status_registers.ctrl_reg6_xl.resume_val = val;
	stat->sensitivity_acc = sensitivity;

	return err;

error:
	dev_err(&stat->client->dev, "update accelerometer fs range failed "
		"0x%02x,0x%02x: %d\n", buf[0], buf[1], err);
	return err;
}

static int lsm9ds1_gyr_update_fs_range(struct lsm9ds1_acc_gyr_status *stat,
								u8 new_fs_range)
{
	int err = -1;
	u8 buf[2];
	u8 updated_val;

	u32 sensitivity;

	switch(new_fs_range) {
	case LSM9DS1_GYR_FS_245DPS:
		sensitivity = SENSITIVITY_GYR_250;
		break;
	case LSM9DS1_GYR_FS_500DPS:
		sensitivity = SENSITIVITY_GYR_500;
		break;
	case LSM9DS1_GYR_FS_2000DPS:
		sensitivity = SENSITIVITY_GYR_2000;
		break;
	default:
		dev_err(&stat->client->dev, "invalid g range "
					"requested: %u\n", new_fs_range);
		return -EINVAL;
	}


	buf[0] = status_registers.ctrl_reg1_g.address;
	err = lsm9ds1_i2c_read(stat, buf, 1);
	if (err < 0)
		goto error;

	updated_val = ((LSM9DS1_GYR_FS_MASK & new_fs_range) | 
					  ((~LSM9DS1_GYR_FS_MASK) & buf[0]));
	
	buf[0] = status_registers.ctrl_reg1_g.address;
	buf[1] = updated_val;

	err = lsm9ds1_i2c_write(stat, buf, 1);
	if (err < 0)
		goto error;

	status_registers.ctrl_reg1_g.resume_val = updated_val;
	stat->sensitivity_gyr = sensitivity;

error:
	return err;
}

static int lsm9ds1_acc_update_odr(struct lsm9ds1_acc_gyr_status *stat,
						unsigned int poll_interval_ms)
{
	int err = -1;
	u8 buf[2];
	int i;

	for (i = ARRAY_SIZE(lsm9ds1_acc_odr_table) - 1; i >= 0; i--) {
		if ((lsm9ds1_acc_odr_table[i].cutoff_ms <= poll_interval_ms)
								|| (i == 0))
			break;
	}

	if (atomic_read(&stat->enabled_acc)) {
		buf[0] = status_registers.ctrl_reg6_xl.address;
		buf[1] = LSM9DS1_ACC_ODR_MASK & lsm9ds1_acc_odr_table[i].value;
		buf[1] |= (~LSM9DS1_ACC_ODR_MASK) & 
				status_registers.ctrl_reg6_xl.resume_val;

		err = lsm9ds1_i2c_write(stat, buf, 1);
		if (err < 0)
			goto error;

		status_registers.ctrl_reg6_xl.resume_val = buf[1];
#ifndef LSM9DS1_FIFO_MODE
		stat->ktime_acc = ktime_set(0, MS_TO_NS(poll_interval_ms));
#else
		stat->acc_cutoff_ms = lsm9ds1_acc_odr_table[i].cutoff_ms;

		buf[0] = status_registers.fifo_src.address;
		err = lsm9ds1_i2c_read(stat, buf, 1);
		if (err < 0)
			goto error;

		buf[0] = status_registers.fifo_ctrl.address;
		buf[1] = status_registers.fifo_ctrl.resume_val & 0x1F;
		err = lsm9ds1_i2c_write(stat, buf, 1);
		if (err < 0)
			goto error;

		buf[0] = status_registers.fifo_ctrl.address;
		if(stat->acc_cutoff_ms == 100)
			buf[1] = status_registers.fifo_ctrl.resume_val & 0xE0 | 0x01;
		else
			buf[1] = status_registers.fifo_ctrl.resume_val;
		err = lsm9ds1_i2c_write(stat, buf, 1);
		if (err < 0)
			goto error;

		buf[0] = status_registers.fifo_src.address;
		err = lsm9ds1_i2c_read(stat, buf, 1);
		if (err < 0)
			goto error;
#endif
	}

	return err;

error:
	dev_err(&stat->client->dev, "update accelerometer odr failed "
			"0x%02x,0x%02x: %d\n", buf[0], buf[1], err);

	return err;
}

static int lsm9ds1_gyr_update_odr(struct lsm9ds1_acc_gyr_status *stat,
						unsigned int poll_interval_ms)
{
	int err = -1;
	u8 buf[2];
	u8 val;
	int i;

	if (atomic_read(&stat->enabled_gyr)) {
		if (atomic_read(&stat->enabled_acc)) {
			val = min(poll_interval_ms, 
					stat->pdata_acc->poll_interval);
		} else {
			val = poll_interval_ms;
		}

		for (i = ARRAY_SIZE(lsm9ds1_gyr_odr_table) - 1; i >= 0; i--) {
			if ((lsm9ds1_gyr_odr_table[i].cutoff_ms <= val)
								|| (i == 0))
				break;
		}

		/* Set ODR value */
		buf[0] = status_registers.ctrl_reg1_g.address;
		buf[1] = LSM9DS1_GYR_ODR_MASK & lsm9ds1_gyr_odr_table[i].value;
		buf[1] |= (~LSM9DS1_GYR_ODR_MASK) &
					status_registers.ctrl_reg1_g.resume_val;

		err = lsm9ds1_i2c_write(stat, buf, 1);
		if (err < 0)
			goto error;

		status_registers.ctrl_reg1_g.resume_val = buf[1];

		/* Enable all axes */
		buf[0] = status_registers.ctrl_reg4.address;
		buf[1] = CTRL_REG4_ALL_AXES_EN | 
				status_registers.ctrl_reg4.resume_val;

		err = lsm9ds1_i2c_write(stat, buf, 1);
		if (err < 0)
			goto error;

		status_registers.ctrl_reg4.resume_val = buf[1];
#ifndef LSM9DS1_FIFO_MODE
		stat->ktime_gyr = ktime_set(0, MS_TO_NS(poll_interval_ms));
#else
		stat->gyr_cutoff_ms = lsm9ds1_gyr_odr_table[i].cutoff_ms;
		buf[0] = status_registers.fifo_ctrl.address;
		buf[1] = status_registers.fifo_ctrl.resume_val & 0x1F;
		err = lsm9ds1_i2c_write(stat, buf, 1);
		if (err < 0)
			goto error;

		buf[0] = status_registers.fifo_ctrl.address;
		if(stat->gyr_cutoff_ms == 67)
			buf[1] = status_registers.fifo_ctrl.resume_val & 0xE0 | 0x01;
		else
			buf[1] = status_registers.fifo_ctrl.resume_val;
		err = lsm9ds1_i2c_write(stat, buf, 1);
		if (err < 0)
			goto error;
#endif
	}

	return err;

error:
	dev_err(&stat->client->dev, "update accelerometer odr failed "
			"0x%02x,0x%02x: %d\n", buf[0], buf[1], err);

	return err;
}

static int lsm9ds1_acc_update_filter(struct lsm9ds1_acc_gyr_status *stat,
							u8 new_bandwidth)
{
	int err = -1;
	u8 updated_val;
	u8 buf[2];

	switch (new_bandwidth) {
	case LSM9DS1_ACC_BW_50:
		break;
	case LSM9DS1_ACC_BW_105:
		break;
	case LSM9DS1_ACC_BW_211:
		break;
	case LSM9DS1_ACC_BW_408:
		break;
	default:
		dev_err(&stat->client->dev, "invalid accelerometer "
			"update bandwidth requested: %u\n", new_bandwidth);
		return -EINVAL;
	}

	buf[0] = status_registers.ctrl_reg6_xl.address;
	err = lsm9ds1_i2c_read(stat, buf, 1);
	if (err < 0)
		goto error;

	status_registers.ctrl_reg6_xl.resume_val = buf[0];

	updated_val = ((LSM9DS1_ACC_BW_MASK & new_bandwidth) |
					((~LSM9DS1_ACC_BW_MASK) & buf[0]));

	buf[0] = status_registers.ctrl_reg6_xl.address;
	buf[1] = updated_val;

	err = lsm9ds1_i2c_write(stat, buf, 1);
	if (err < 0)
		goto error;

	status_registers.ctrl_reg6_xl.resume_val = updated_val;

	return err;

error:
	dev_err(&stat->client->dev, "update accelerometer fs range failed "
		"0x%02x,0x%02x: %d\n", buf[0], buf[1], err);
	return err;
}

static int lsm9ds1_acc_enable(struct lsm9ds1_acc_gyr_status *stat)
{
	int err = -1;

	if (!atomic_cmpxchg(&stat->enabled_acc, 0, 1)) {
		err = lsm9ds1_acc_device_power_on(stat);
		if (err < 0) {
			atomic_set(&stat->enabled_acc, 0);
			dev_err(&stat->client->dev, "enable accelerometer "
				      "failed");
			return err;
		}
#ifndef LSM9DS1_FIFO_MODE
		hrtimer_start(&stat->hr_timer_acc, stat->ktime_acc, 
							HRTIMER_MODE_REL);
#else
		u8 buf[3];
		status_registers.int1_ctrl.resume_val = INT1_CTRL_DEF;
		buf[0] = status_registers.int1_ctrl.address;
		buf[1] = status_registers.int1_ctrl.resume_val;
		buf[2] = status_registers.int2_ctrl.resume_val;
		err = lsm9ds1_i2c_write(stat, buf, 2);
#endif
		if(stat->pdata_main->gpio_int1 >= 0)
			enable_irq(stat->irq1);
//		if(stat->pdata_main->gpio_int2 >= 0)
//			enable_irq(stat->irq2);
	}

	return 0;
}

static int lsm9ds1_gyr_enable(struct lsm9ds1_acc_gyr_status *stat)
{
	int err = -1;

	if (!atomic_cmpxchg(&stat->enabled_gyr, 0, 1)) {

		err = lsm9ds1_gyr_device_power_on(stat);
		if (err < 0) {
			atomic_set(&stat->enabled_gyr, 0);
			return err;
		}
		mdelay(50);
#ifndef LSM9DS1_FIFO_MODE
		hrtimer_start(&(stat->hr_timer_gyr), stat->ktime_gyr,
							HRTIMER_MODE_REL);
#endif
	}
	return 0;
}

int lsm9ds1_acc_input_open(struct input_dev *input)
{
	struct lsm9ds1_acc_gyr_status *stat = input_get_drvdata(input);
	dev_dbg(&stat->client->dev, "%s\n", __func__);

	//return lsm9ds1_acc_enable(stat);
	return 0;
}

void lsm9ds1_acc_input_close(struct input_dev *dev)
{
	struct lsm9ds1_acc_gyr_status *stat = input_get_drvdata(dev);
	dev_dbg(&stat->client->dev, "%s\n", __func__);
	//lsm9ds1_acc_disable(stat);
}

int lsm9ds1_gyr_input_open(struct input_dev *input)
{
	struct lsm9ds1_acc_gyr_status *stat = input_get_drvdata(input);
	dev_dbg(&stat->client->dev, "%s\n", __func__);

	//return lsm9ds1_gyr_enable(stat);
	return 0;
}

void lsm9ds1_gyr_input_close(struct input_dev *dev)
{
	struct lsm9ds1_acc_gyr_status *stat = input_get_drvdata(dev);
	dev_dbg(&stat->client->dev, "%s\n", __func__);
	//lsm9ds1_gyr_disable(stat);
}

static int lsm9ds1_acc_get_data(struct lsm9ds1_acc_gyr_status *stat, int *xyz)
{
	int i, err = -1;
	u8 acc_data[6];
	s32 hw_d[3] = { 0 };

	acc_data[0] = OUT_X_L_XL;
	err = lsm9ds1_i2c_read(stat, acc_data, 6);
	if (err < 0)
		return err;

	hw_d[0] = ((s32)( (s16)((acc_data[1] << 8) | (acc_data[0]))));
	hw_d[1] = ((s32)( (s16)((acc_data[3] << 8) | (acc_data[2]))));
	hw_d[2] = ((s32)( (s16)((acc_data[5] << 8) | (acc_data[4]))));

	hw_d[0] = hw_d[0] * stat->sensitivity_acc;
	hw_d[1] = hw_d[1] * stat->sensitivity_acc;
	hw_d[2] = hw_d[2] * stat->sensitivity_acc;

	for (i = 0; i < 3; i++) {
		xyz[i] = stat->pdata_main->rot_matrix[0][i] * hw_d[0] +
				stat->pdata_main->rot_matrix[1][i] * hw_d[1] +
				stat->pdata_main->rot_matrix[2][i] * hw_d[2];
	}

	return err;
}

static int lsm9ds1_gyr_get_data(struct lsm9ds1_acc_gyr_status *stat, int *xyz)
{
	int i, err = 1;
	u8 gyro_data[6];
	s32 hw_d[3] = { 0 };

	gyro_data[0] = OUT_X_L_G;
	err = lsm9ds1_i2c_read(stat, gyro_data, 6);

	if (err < 0)
		return err;

	hw_d[0] = (s32) ((s16)((gyro_data[1]) << 8) | gyro_data[0]);
	hw_d[1] = (s32) ((s16)((gyro_data[3]) << 8) | gyro_data[2]);
	hw_d[2] = (s32) ((s16)((gyro_data[5]) << 8) | gyro_data[4]);

#ifdef DEBUG
	dev_dbg(&stat->client->dev, "%s read x=%X %X(regH regL), x=%d(dec) [udps]\n",
		LSM9DS1_GYR_DEV_NAME, gyro_data[1], gyro_data[0], hw_d[0]);
	dev_dbg(&stat->client->dev, "%s read y=%X %X(regH regL), y=%d(dec) [udps]\n",
		LSM9DS1_GYR_DEV_NAME, gyro_data[3], gyro_data[2], hw_d[1]);
	dev_dbg(&stat->client->dev, "%s read z=%X %X(regH regL), z=%d(dec) [udps]\n",
		LSM9DS1_GYR_DEV_NAME, gyro_data[5], gyro_data[4], hw_d[2]);
#endif

	hw_d[0] = hw_d[0] * stat->sensitivity_gyr;
	hw_d[1] = hw_d[1] * stat->sensitivity_gyr;
	hw_d[2] = hw_d[2] * stat->sensitivity_gyr;

	for (i = 0; i < 3; i++) {
		xyz[i] = stat->pdata_main->rot_matrix[0][i] * hw_d[0] +
				stat->pdata_main->rot_matrix[1][i] * hw_d[1] +
				stat->pdata_main->rot_matrix[2][i] * hw_d[2];
	}

	return err;
}

static void lsm9ds1_acc_report_values(struct lsm9ds1_acc_gyr_status *stat,
								int *xyz)
{
	input_report_abs(stat->input_dev_acc, ABS_X, xyz[0]);
	input_report_abs(stat->input_dev_acc, ABS_Y, xyz[1]);
	input_report_abs(stat->input_dev_acc, ABS_Z, xyz[2]);
	input_sync(stat->input_dev_acc);
}

static void lsm9ds1_gyr_report_values(struct lsm9ds1_acc_gyr_status *stat,
								int *xyz)
{
	input_report_abs(stat->input_dev_gyr, ABS_X, xyz[0]);
	input_report_abs(stat->input_dev_gyr, ABS_Y, xyz[1]);
	input_report_abs(stat->input_dev_gyr, ABS_Z, xyz[2]);
	input_sync(stat->input_dev_gyr);
}

static int lsm9ds1_acc_input_init(struct lsm9ds1_acc_gyr_status *stat)
{
	int err;

	stat->input_dev_acc = input_allocate_device();
	if (!stat->input_dev_acc) {
		err = -ENOMEM;
		dev_err(&stat->client->dev, "accelerometer "
					"input device allocation failed\n");
		return err;
	}

	stat->input_dev_acc->open = lsm9ds1_acc_input_open;
	stat->input_dev_acc->close = lsm9ds1_acc_input_close;
	stat->input_dev_acc->name = LSM9DS1_ACC_DEV_NAME;
	stat->input_dev_acc->id.bustype = BUS_I2C;
	stat->input_dev_acc->dev.parent = &stat->client->dev;

	input_set_drvdata(stat->input_dev_acc, stat);

	set_bit(EV_ABS, stat->input_dev_acc->evbit);

	input_set_abs_params(stat->input_dev_acc, ABS_X, -ACC_G_MAX_NEG,
						ACC_G_MAX_POS, FUZZ, FLAT);
	input_set_abs_params(stat->input_dev_acc, ABS_Y, -ACC_G_MAX_NEG,
						ACC_G_MAX_POS, FUZZ, FLAT);
	input_set_abs_params(stat->input_dev_acc, ABS_Z,-ACC_G_MAX_NEG,
						ACC_G_MAX_POS, FUZZ, FLAT);

	err = input_register_device(stat->input_dev_acc);
	if (err) {
		dev_err(&stat->client->dev,
			"unable to register accelerometer input device %s\n",
				stat->input_dev_acc->name);
		input_free_device(stat->input_dev_acc);
	}

	return err;
}

static int lsm9ds1_gyr_input_init(struct lsm9ds1_acc_gyr_status *stat)
{
	int err = -1;

	dev_dbg(&stat->client->dev, "%s\n", __func__);

	stat->input_dev_gyr = input_allocate_device();
	if (!stat->input_dev_gyr) {
		err = -ENOMEM;
		dev_err(&stat->client->dev,
			"input device allocation failed\n");
		return err;
	}

	stat->input_dev_gyr->open = lsm9ds1_gyr_input_open;
	stat->input_dev_gyr->close = lsm9ds1_gyr_input_close;
	stat->input_dev_gyr->name = LSM9DS1_GYR_DEV_NAME;
	stat->input_dev_gyr->id.bustype = BUS_I2C;
	stat->input_dev_gyr->dev.parent = &stat->client->dev;

	input_set_drvdata(stat->input_dev_gyr, stat);

	set_bit(EV_ABS, stat->input_dev_gyr->evbit);

	input_set_abs_params(stat->input_dev_gyr, ABS_X, -GYR_FS_MAX - 1,
							GYR_FS_MAX, 0, 0);
	input_set_abs_params(stat->input_dev_gyr, ABS_Y, -GYR_FS_MAX - 1,
							GYR_FS_MAX, 0, 0);
	input_set_abs_params(stat->input_dev_gyr, ABS_Z, -GYR_FS_MAX - 1,
							GYR_FS_MAX, 0, 0);


	err = input_register_device(stat->input_dev_gyr);
	if (err) {
		dev_err(&stat->client->dev,
			"unable to register input device %s\n",
			stat->input_dev_gyr->name);
		input_free_device(stat->input_dev_gyr);
	}

	return err;
}
static void lsm9ds1_input_cleanup(struct lsm9ds1_acc_gyr_status *stat)
{
	input_unregister_device(stat->input_dev_acc);
	input_free_device(stat->input_dev_acc);

	input_unregister_device(stat->input_dev_gyr);
	input_free_device(stat->input_dev_gyr);
}

static ssize_t attr_set_polling_rate_acc(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t size)
{
	struct device *dev = to_dev(kobj->parent);
	struct lsm9ds1_acc_gyr_status *stat = dev_get_drvdata(dev);
	unsigned long interval_ms;

	if (strict_strtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if (!interval_ms)
		return -EINVAL;

	interval_ms = (unsigned int)max((unsigned int)interval_ms,
						stat->pdata_acc->min_interval);

	mutex_lock(&stat->lock);
	stat->pdata_acc->poll_interval = (unsigned int)interval_ms;
	lsm9ds1_acc_update_odr(stat, interval_ms);
	mutex_unlock(&stat->lock);

	return size;
}

static ssize_t attr_get_polling_rate_acc(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	unsigned int val;
	struct device *dev = to_dev(kobj->parent);
	struct lsm9ds1_acc_gyr_status *stat = dev_get_drvdata(dev);

	mutex_lock(&stat->lock);
	val = stat->pdata_acc->poll_interval;
	mutex_unlock(&stat->lock);

	return sprintf(buf, "%u\n", val);
}

static ssize_t attr_get_enable_acc(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	struct device *dev = to_dev(kobj->parent);
	struct lsm9ds1_acc_gyr_status *stat = dev_get_drvdata(dev);

	int val = (int)atomic_read(&stat->enabled_acc);

	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_enable_acc(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t size)
{
	struct device *dev = to_dev(kobj->parent);
	struct lsm9ds1_acc_gyr_status *stat = dev_get_drvdata(dev);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
	{
		lsm9ds1_acc_enable(stat);
		atomic_set(&stat->enabled_acc_r, 0);
	}
	else
	{
		if (atomic_read(&stat->enabled_gyr) == 0){
			lsm9ds1_acc_disable(stat);
			atomic_set(&stat->enabled_acc_r, 0);
		}
		else
			atomic_set(&stat->enabled_acc_r, 1);
	}

	return size;
}

static ssize_t attr_get_range_acc(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	struct device *dev = to_dev(kobj->parent);
	u8 val;
	struct lsm9ds1_acc_gyr_status *stat = dev_get_drvdata(dev);
	int range = 2;

	mutex_lock(&stat->lock);
	val = stat->pdata_acc->fs_range ;
	mutex_unlock(&stat->lock);

	switch (val) {
	case LSM9DS1_ACC_FS_2G:
		range = 2;
		break;
	case LSM9DS1_ACC_FS_4G:
		range = 4;
		break;
	case LSM9DS1_ACC_FS_8G:
		range = 8;
		break;
	}

	return sprintf(buf, "%d\n", range);
}

static ssize_t attr_set_range_acc(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t size)
{
	struct device *dev = to_dev(kobj->parent);
	struct lsm9ds1_acc_gyr_status *stat = dev_get_drvdata(dev);
	unsigned long val;
	u8 range;
	int err;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	switch (val) {
	case 2:
		range = LSM9DS1_ACC_FS_2G;
		break;
	case 4:
		range = LSM9DS1_ACC_FS_4G;
		break;
	case 8:
		range = LSM9DS1_ACC_FS_8G;
		break;
	default:
		dev_err(&stat->client->dev, "accelerometer invalid range "
					"request: %lu, discarded\n", val);
		return -EINVAL;
	}

	mutex_lock(&stat->lock);
	err = lsm9ds1_acc_update_fs_range(stat, range);
	if (err < 0) {
		mutex_unlock(&stat->lock);
		return err;
	}
	stat->pdata_acc->fs_range = range;
	mutex_unlock(&stat->lock);

	dev_info(&stat->client->dev, "accelerometer range set to:"
							" %lu g\n", val);

	return size;
}

static ssize_t attr_get_aa_filter(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	struct device *dev = to_dev(kobj->parent);
	u8 val;
	struct lsm9ds1_acc_gyr_status *stat = dev_get_drvdata(dev);
	int frequency = FILTER_408;

	mutex_lock(&stat->lock);
	val = stat->pdata_acc->aa_filter_bandwidth;
	mutex_unlock(&stat->lock);

	switch (val) {
	case LSM9DS1_ACC_BW_50:
		frequency = FILTER_50;
		break;
	case LSM9DS1_ACC_BW_105:
		frequency = FILTER_105;
		break;
	case LSM9DS1_ACC_BW_211:
		frequency = FILTER_211;
		break;
	case LSM9DS1_ACC_BW_408:
		frequency = FILTER_408;
		break;
	}

	return sprintf(buf, "%d\n", frequency);
}

static ssize_t attr_set_aa_filter(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t size)
{
	struct device *dev = to_dev(kobj->parent);
	struct lsm9ds1_acc_gyr_status *stat = dev_get_drvdata(dev);
	unsigned long val;
	u8 frequency;
	int err;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	switch (val) {
	case FILTER_50:
		frequency = LSM9DS1_ACC_BW_50;
		break;
	case FILTER_105:
		frequency = LSM9DS1_ACC_BW_105;
		break;
	case FILTER_211:
		frequency = LSM9DS1_ACC_BW_211;
		break;
	case FILTER_408:
		frequency = LSM9DS1_ACC_BW_408;
		break;
	default:
		dev_err(&stat->client->dev, "accelerometer invalid filter "
					"request: %lu, discarded\n", val);
		return -EINVAL;
	}

	mutex_lock(&stat->lock);
	err = lsm9ds1_acc_update_filter(stat, frequency);
	if (err < 0) {
		mutex_unlock(&stat->lock);
		return err;
	}
	stat->pdata_acc->aa_filter_bandwidth = frequency;
	mutex_unlock(&stat->lock);

	dev_info(&stat->client->dev, "accelerometer anti-aliasing filter "
					"set to: %lu Hz\n", val);

	return size;
}

static ssize_t attr_get_polling_rate_gyr(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	int val;
	struct device *dev = to_dev(kobj->parent);
	struct lsm9ds1_acc_gyr_status *stat = dev_get_drvdata(dev);

	mutex_lock(&stat->lock);
	val = stat->pdata_gyr->poll_interval;
	mutex_unlock(&stat->lock);

	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_polling_rate_gyr(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t size)
{
	int err;
	struct device *dev = to_dev(kobj->parent);
	struct lsm9ds1_acc_gyr_status *stat = dev_get_drvdata(dev);
	unsigned long interval_ms;

	if (strict_strtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if (!interval_ms)
		return -EINVAL;
	
	interval_ms = (unsigned int)max((unsigned int)interval_ms,
					stat->pdata_gyr->min_interval);

	mutex_lock(&stat->lock);
	err = lsm9ds1_gyr_update_odr(stat, interval_ms);
	if(err >= 0)
		stat->pdata_gyr->poll_interval = interval_ms;
	mutex_unlock(&stat->lock);

	return size;
}

static ssize_t attr_get_enable_gyr(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	struct device *dev = to_dev(kobj->parent);
	struct lsm9ds1_acc_gyr_status *stat = dev_get_drvdata(dev);
	int val = atomic_read(&stat->enabled_gyr);

	return sprintf(buf, "%d\n", val);
}

static ssize_t attr_set_enable_gyr(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t size)
{
	struct device *dev = to_dev(kobj->parent);
	struct lsm9ds1_acc_gyr_status *stat = dev_get_drvdata(dev);
	unsigned long val;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	if (val)
	{
		if (atomic_read(&stat->enabled_acc) == 0){
			lsm9ds1_acc_enable(stat);
			atomic_set(&stat->enabled_acc_r, 1);
		}
		lsm9ds1_gyr_enable(stat);
	}
	else
	{
		lsm9ds1_gyr_disable(stat);
		if (atomic_read(&stat->enabled_acc_r) == 1){
			lsm9ds1_acc_disable(stat);
			atomic_set(&stat->enabled_acc_r, 0);
		}
	}

	return size;
}

static ssize_t attr_get_range_gyr(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	struct device *dev = to_dev(kobj->parent);
	struct lsm9ds1_acc_gyr_status *stat = dev_get_drvdata(dev);
	int range = 0;
	u8 val;

	mutex_lock(&stat->lock);
	val = stat->pdata_gyr->fs_range;
	switch (val) {
	case LSM9DS1_GYR_FS_245DPS:
		range = RANGE_245DPS;
		break;
	case LSM9DS1_GYR_FS_500DPS:
		range = RANGE_500DPS;
		break;
	case LSM9DS1_GYR_FS_2000DPS:
		range = RANGE_2000DPS;
		break;
	}
	mutex_unlock(&stat->lock);

	return sprintf(buf, "%d\n", range);
}

static ssize_t attr_set_range_gyr(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t size)
{
	struct device *dev = to_dev(kobj->parent);
	struct lsm9ds1_acc_gyr_status *stat = dev_get_drvdata(dev);
	unsigned long val;
	u8 range;
	int err;

	if (strict_strtoul(buf, 10, &val))
		return -EINVAL;

	switch (val) {
	case 245:
		range = LSM9DS1_GYR_FS_245DPS;
		break;
	case 500:
		range = LSM9DS1_GYR_FS_500DPS;
		break;
	case 2000:
		range = LSM9DS1_GYR_FS_2000DPS;
		break;
	default:
		dev_err(&stat->client->dev, "invalid range request: %lu,"
				" discarded\n", val);
		return -EINVAL;
	}

	mutex_lock(&stat->lock);
	err = lsm9ds1_gyr_update_fs_range(stat, range);
	if (err >= 0)
		stat->pdata_gyr->fs_range = range;
	mutex_unlock(&stat->lock);

	dev_info(&stat->client->dev, "range set to: %lu dps\n", val);

	return size;
}

#ifdef LSM9DS1_FIFO_MODE
static ssize_t lsm9ds1_acc_fifo_show(struct kobject *kobj,
					struct kobj_attribute *attr,
					char *buf)
{
	int count = 0;
	struct device *dev = to_dev(kobj->parent);
	struct lsm9ds1_acc_gyr_status *pdata = dev_get_drvdata(dev);
	mutex_lock(&pdata->lock);
	count = sprintf(&buf[count], "period poll  :%d  ms\n", pdata->pdata_acc->poll_interval);
	count += sprintf(&buf[count],"period fifo  :%lld  ns\n",pdata->period_rel);
	count += sprintf(&buf[count],"timeout :%d ms\n", pdata->fifo_timeout);
//	count += sprintf(&buf[count],"interrupt wake up: %s\n", (pdata->pdata_acc->fifo_wakeup ? "yes" : "no"));  /*is the interrupt enable*/ 
	mutex_unlock(&pdata->lock);
	return count;
}

static ssize_t lsm9ds1_acc_fifo_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t size)
{
	struct device *dev = to_dev(kobj->parent);
	struct lsm9ds1_acc_gyr_status *pdata = dev_get_drvdata(dev);
	struct i2c_client * client = pdata->client;
	int period,timeout,wakeup;
	sscanf(buf,"%d,%d,%d",&period,&timeout,&wakeup);
	printk("period %d ,timeout is %d, wake up is :%d\n",period,timeout,wakeup);
	if(timeout > 0){
		mutex_lock(&pdata->lock);
		pdata->pdata_acc->poll_interval = period;
		mutex_unlock(&pdata->lock);
		pdata->fifo_timeout = timeout;
		pdata->fifo_wakeup = wakeup;
	}else{
		pdata->fifo_timeout = timeout;
		pdata->fifo_wakeup = wakeup;
		mutex_lock(&pdata->lock);
		pdata->pdata_acc->poll_interval = period;
		mutex_unlock(&pdata->lock);
	}
	return size;
}
#endif

static struct kobj_attribute poll_attr_acc =
	__ATTR(pollrate_ms, 0664, attr_get_polling_rate_acc, 
						attr_set_polling_rate_acc);
static struct kobj_attribute enable_attr_acc =
	__ATTR(enable_device, 0664, attr_get_enable_acc, attr_set_enable_acc);
static struct kobj_attribute fs_attr_acc =
	__ATTR(range, 0664, attr_get_range_acc, attr_set_range_acc);
static struct kobj_attribute aa_filter_attr  =
	__ATTR(anti_aliasing_frequency, 0664, attr_get_aa_filter, 
							attr_set_aa_filter);
#ifdef LSM9DS1_FIFO_MODE
static struct kobj_attribute fifo_acc =
	__ATTR(fifo, 0664, lsm9ds1_acc_fifo_show, lsm9ds1_acc_fifo_store);
#endif
static struct kobj_attribute poll_attr_gyr =
	__ATTR(pollrate_ms, 0664, attr_get_polling_rate_gyr, 
						attr_set_polling_rate_gyr);
static struct kobj_attribute enable_attr_gyr =
	__ATTR(enable_device, 0664, attr_get_enable_gyr, attr_set_enable_gyr);
static struct kobj_attribute range_attr_gyr =
	__ATTR(range, 0664, attr_get_range_gyr, attr_set_range_gyr);

static struct attribute *attributes_acc[] = {
	&poll_attr_acc.attr,
	&enable_attr_acc.attr,
	&fs_attr_acc.attr,
	&aa_filter_attr.attr,
#ifdef LSM9DS1_FIFO_MODE
	&fifo_acc.attr,
#endif
	NULL,
};

static struct attribute *attributes_gyr[] = {
	&poll_attr_gyr.attr,
	&enable_attr_gyr.attr,
	&range_attr_gyr.attr,
	NULL,
};

static struct attribute_group attr_group_acc = {
	.attrs = attributes_acc,
};

static struct attribute_group attr_group_gyr = {
	.attrs = attributes_gyr,
};

static int create_sysfs_interfaces(struct device *dev)
{
	int err;

	acc_kobj = kobject_create_and_add("accelerometer", &dev->kobj);
	if(!acc_kobj)
		return -ENOMEM;

	gyr_kobj = kobject_create_and_add("gyroscope", &dev->kobj);
	if(!gyr_kobj)
		return -ENOMEM;

	err = sysfs_create_group(acc_kobj, &attr_group_acc);
	if (err)
		kobject_put(acc_kobj);

	err = sysfs_create_group(gyr_kobj, &attr_group_gyr);
	if (err)
		kobject_put(gyr_kobj);

	return 0;
}

static void remove_sysfs_interfaces(struct device *dev)
{
	kobject_put(acc_kobj);
	kobject_put(gyr_kobj);
}

#ifdef LSM9DS1_FIFO_MODE
static int lsm9ds1_read_acc_gyr_fifo_data(struct lsm9ds1_acc_gyr_status *stat){
	struct lsm9ds1_fifo *pfifo_acc = &stat->fifo;
	struct lsm9ds1_fifo *pfifo_gyr = &stat->fifo_gyr;
	struct timespec ts;
	u8 buf[1];
	int count,i,err;
	int xyz[3] = { 0 };

	buf[0] = status_registers.fifo_src.address;
	err = lsm9ds1_i2c_read(stat, buf, 1);
	if (err < 0)
		return err;

	count = buf[0] & (u8)0x3f;
	if(count > 0){
		ktime_get_ts(&ts);
		for(i=0;i<count;i++){
			err = lsm9ds1_acc_get_data(stat, xyz);
			if (err < 0)
				dev_err(&stat->client->dev, "get accelerometer data failed\n");
			pfifo_acc->fifo_data[i].x = xyz[0];
			pfifo_acc->fifo_data[i].y = xyz[1];
			pfifo_acc->fifo_data[i].z = xyz[2];

			err = lsm9ds1_gyr_get_data(stat, xyz);
			if (err < 0)
				dev_err(&stat->client->dev, "get gyroscope data failed\n");
			pfifo_gyr->fifo_data[i].x = xyz[0];
			pfifo_gyr->fifo_data[i].y = xyz[1];
			pfifo_gyr->fifo_data[i].z = xyz[2];
		}

		pfifo_acc->count = count;
		pfifo_gyr->count = count;
		pfifo_acc->period = stat->acc_cutoff_ms * 1000000;
		pfifo_gyr->period = stat->gyr_cutoff_ms * 1000000;
		pfifo_acc->timestamp = ((s64)ts.tv_sec) * NSEC_PER_SEC  + ts.tv_nsec;
		pfifo_gyr->timestamp = ((s64)ts.tv_sec) * NSEC_PER_SEC  + ts.tv_nsec;
	}

	return 0;
}

static void fifo_function_work(struct work_struct *input_work)
{
	struct lsm9ds1_acc_gyr_status *stat;
	int err;

	stat = container_of((struct work_struct *)input_work,
			struct lsm9ds1_acc_gyr_status, input_work);

	err = lsm9ds1_read_acc_gyr_fifo_data(stat);
	if(!err){
		if (atomic_read(&stat->enabled_acc)){
			atomic_set(&stat->fifo_ready, 1);
			wake_up(&stat->fifo_wq);
		}
		if (atomic_read(&stat->enabled_gyr)){
			atomic_set(&stat->fifo_gyr_ready, 1);
			wake_up(&stat->fifo_gyr_wq);
		}
	}
}

static irqreturn_t lsm9ds1_irq1_handler(int irq, void *dev)
{
	struct lsm9ds1_acc_gyr_status *stat = (struct lsm9ds1_acc_gyr_status *)dev;

	queue_work(lsm9ds1_acc_gyr_workqueue, &stat->input_work);

	return IRQ_HANDLED;
}
#endif

#ifndef LSM9DS1_FIFO_MODE
static void poll_function_work_acc(struct work_struct *input_work_acc)
{
	struct lsm9ds1_acc_gyr_status *stat;
	int xyz[3] = { 0 };
	int err;

	stat = container_of((struct work_struct *)input_work_acc,
			struct lsm9ds1_acc_gyr_status, input_work_acc);

	err = lsm9ds1_acc_get_data(stat, xyz);
	if (err < 0)
		dev_err(&stat->client->dev, "get accelerometer data failed\n");
	else
		lsm9ds1_acc_report_values(stat, xyz);
}

static void poll_function_work_gyr(struct work_struct *input_work_gyr)
{
	struct lsm9ds1_acc_gyr_status *stat;
	int xyz[3] = { 0 };
	int err;

	stat = container_of((struct work_struct *)input_work_gyr,
			struct lsm9ds1_acc_gyr_status, input_work_gyr);

	err = lsm9ds1_gyr_get_data(stat, xyz);
	if (err < 0)
		dev_err(&stat->client->dev, "get gyroscope data failed.\n");
	else
		lsm9ds1_gyr_report_values(stat, xyz);
}
#endif

#ifdef CONFIG_OF
static const struct of_device_id lsm9ds1_acc_gyr_dt_id[] = {
	{.compatible = "st,lsm9ds1-acc-gyr",},
	{},
};
MODULE_DEVICE_TABLE(of, lsm9ds1_acc_gyr_dt_id);

static int lsm9ds1_acc_gyr_parse_dt(struct lsm9ds1_acc_gyr_status *stat,
                                        struct device* dev)
{
	struct device_node *dn;
	uint8_t i, j;
	uint32_t val;
	short vect[9];
	int orient;

	if (of_match_device(lsm9ds1_acc_gyr_dt_id, dev)) {
		dn = dev->of_node;
		stat->pdata_main->of_node = dn;
		
		stat->pdata_main->gpio_int1 = of_get_gpio(dn, 0);
		if (!gpio_is_valid(stat->pdata_main->gpio_int1)) {
			dev_err(dev, "failed to get gpio_int1\n");

			stat->pdata_main->gpio_int1 = LSM9DS1_INT1_GPIO_DEF;
		}

		stat->pdata_main->gpio_int2 = of_get_gpio(dn, 1);
		if (!gpio_is_valid(stat->pdata_main->gpio_int2)) {
			dev_err(dev, "failed to get gpio_int2\n");

			stat->pdata_main->gpio_int2 = LSM9DS1_INT2_GPIO_DEF;
		}

		if (!strcmp(panel_type, "WXGA_P")) {
			orient = 1;
		}
		else if (!strcmp(panel_type, "WXGA")) {
			orient = 0;
		}

		if(orient) {
			if (of_property_read_u16_array(dn, "rot-matrix_p", vect,
				      ARRAY_SIZE(vect)) >= 0) {
				for (j = 0; j < 3; j++) {
					for (i = 0; i < 3; i++) {
						stat->pdata_main->rot_matrix[i][j] =
							(short)vect[3 * j + i];
					}
				}
			} else {
				for (j = 0; j < 3; j++) {
					for (i = 0; i < 3; i++) {
						stat->pdata_main->rot_matrix[i][j] =
				default_lsm9ds1_main_platform_data.rot_matrix[i][j];
					}
				}
			}
		}
		else {
			if (of_property_read_u16_array(dn, "rot-matrix", vect,
				      ARRAY_SIZE(vect)) >= 0) {
				for (j = 0; j < 3; j++) {
					for (i = 0; i < 3; i++) {
						stat->pdata_main->rot_matrix[i][j] =
							(short)vect[3 * j + i];
					}
				}
			} else {
				for (j = 0; j < 3; j++) {
					for (i = 0; i < 3; i++) {
						stat->pdata_main->rot_matrix[i][j] =
				default_lsm9ds1_main_platform_data.rot_matrix[i][j];
					}
				}
			}
		}

		if (!of_property_read_u32(dn, "g-poll-interval", &val)) {
			stat->pdata_gyr->poll_interval = val;
		} else {
			stat->pdata_gyr->poll_interval =
				LSM9DS1_GYR_POLL_INTERVAL_DEF;
		}

		if (!of_property_read_u32(dn, "g-min-interval", &val)) {
			stat->pdata_gyr->min_interval = val;
		} else {
			stat->pdata_gyr->min_interval =
				LSM9DS1_GYR_MIN_POLL_PERIOD_MS;
		}

		if (!of_property_read_u32(dn, "g-fs-range", &val)) {
			stat->pdata_gyr->fs_range = val;
		} else {
			stat->pdata_gyr->fs_range = LSM9DS1_GYR_FS_2000DPS;
		}

		if (!of_property_read_u32(dn, "x-poll-interval", &val)) {
			stat->pdata_acc->poll_interval = val;
		} else {
			stat->pdata_acc->poll_interval =
				LSM9DS1_ACC_POLL_INTERVAL_DEF;
		}

		if (!of_property_read_u32(dn, "x-min-interval", &val)) {
			stat->pdata_acc->min_interval = val;
		} else {
			stat->pdata_acc->min_interval =
				LSM9DS1_ACC_MIN_POLL_PERIOD_MS;
		}

		if (!of_property_read_u32(dn, "x-fs-range", &val)) {
			stat->pdata_acc->fs_range = val;
		} else {
			stat->pdata_acc->fs_range = LSM9DS1_ACC_FS_4G;
		}

		if (!of_property_read_u32(dn, "aa-filter-bw", &val)) {
			stat->pdata_acc->fs_range = val;
		} else {
			stat->pdata_acc->fs_range = LSM9DS1_ACC_BW_408;
		}
		return 0;
	}
	return -1;
}
#else
#endif

static int lsm9ds1_open(struct inode *inode, struct file *file){
	int err;
	err = nonseekable_open(inode, file);
	if (err)
		return err;
	file->private_data = p_lsm9ds1_data;
	return 0;
}

#ifdef LSM9DS1_FIFO_MODE
static ssize_t lsm9ds1_read(struct file *file, char __user *buf, size_t size, loff_t *ppos){
	struct lsm9ds1_acc_gyr_status *pdata = file->private_data;
	int ret = 0;

	if (!(file->f_flags & O_NONBLOCK)) {
		ret = wait_event_interruptible(pdata->fifo_wq, (atomic_read(&pdata->fifo_ready) != 0));
		if (ret)
			return ret;
	}
	if (!atomic_read(&pdata->fifo_ready))
		return -ENODEV;
	if(size < sizeof(struct lsm9ds1_fifo)){
		printk(KERN_ERR "the buffer leght less than need\n");
		return -ENOMEM;
	}

	if(!copy_to_user(buf,&pdata->fifo,sizeof(struct lsm9ds1_fifo))){
		atomic_set(&pdata->fifo_ready,0);
		return size;
	}
	return -ENOMEM ;
}

static unsigned int lsm9ds1_poll(struct file * file, struct poll_table_struct * wait){
	struct lsm9ds1_acc_gyr_status *pdata = file->private_data;
	poll_wait(file, &pdata->fifo_wq, wait);
	if (atomic_read(&pdata->fifo_ready))
		return POLLIN | POLLRDNORM;
	return 0;
}

static const struct file_operations lsm9ds1_fops = {
	.owner = THIS_MODULE,
	.open = lsm9ds1_open,
	.read = lsm9ds1_read,
	.poll = lsm9ds1_poll,
};

static struct miscdevice lsm9ds1_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "lsm9ds1",
	.fops = &lsm9ds1_fops,
};

static int lsm9ds1_gyr_open(struct inode *inode, struct file *file){
	int err;
	err = nonseekable_open(inode, file);
	if (err)
		return err;
	file->private_data = p_lsm9ds1_data;
	return 0;
}

static ssize_t lsm9ds1_gyr_read(struct file *file, char __user *buf, size_t size, loff_t *ppos){
	struct lsm9ds1_acc_gyr_status *pdata = file->private_data;
	int ret = 0;
	if (!(file->f_flags & O_NONBLOCK)) {
		ret = wait_event_interruptible(pdata->fifo_gyr_wq, (atomic_read(&pdata->fifo_gyr_ready) != 0));
		if (ret)
			return ret;
	}
	if (!atomic_read(&pdata->fifo_gyr_ready))
		return -ENODEV;
	if(size < sizeof(struct lsm9ds1_fifo)){
		printk(KERN_ERR "the buffer leght less than need\n");
		return -ENOMEM;
	}

	if(!copy_to_user(buf,&pdata->fifo_gyr,sizeof(struct lsm9ds1_fifo))){
		atomic_set(&pdata->fifo_gyr_ready,0);
		return size;
	}
	return -ENOMEM;
}

static unsigned int lsm9ds1_gyr_poll(struct file * file, struct poll_table_struct * wait){
	struct lsm9ds1_acc_gyr_status *pdata = file->private_data;
	poll_wait(file, &pdata->fifo_gyr_wq, wait);
	if (atomic_read(&pdata->fifo_gyr_ready)){
		return POLLIN | POLLRDNORM;
	}
	return 0;
}

static const struct file_operations lsm9ds1_gyr_fops = {
	.owner = THIS_MODULE,
	.open = lsm9ds1_gyr_open,
	.read = lsm9ds1_gyr_read,
	.poll = lsm9ds1_gyr_poll,
};

static struct miscdevice lsm9ds1_gyr_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "lsm9ds1_gyr",
	.fops = &lsm9ds1_gyr_fops,
};
#endif

static int lsm9ds1_acc_gyr_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct lsm9ds1_acc_gyr_status *stat;
	int err = -1;
	u32 smbus_func = I2C_FUNC_SMBUS_BYTE_DATA |
			I2C_FUNC_SMBUS_WORD_DATA | I2C_FUNC_SMBUS_I2C_BLOCK;

	dev_info(&client->dev, "probe start.\n");
	stat = kzalloc(sizeof(struct lsm9ds1_acc_gyr_status), GFP_KERNEL);
	if (stat == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for module data: "
					"%d\n", err);
		goto exit_check_functionality_failed;
	}
	p_lsm9ds1_data = stat;
	stat->use_smbus = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_warn(&client->dev, "client not i2c capable\n");
		if (i2c_check_functionality(client->adapter, smbus_func)){
			stat->use_smbus = 1;
			dev_warn(&client->dev, "client using SMBUS\n");
		} else {
			err = -ENODEV;
			dev_err(&client->dev, "client nor SMBUS capable\n");
			goto exit_check_functionality_failed;
		}
	}

	if(lsm9ds1_acc_gyr_workqueue == 0)
		lsm9ds1_acc_gyr_workqueue = 
			create_workqueue("lsm9ds1_acc_gyr_workqueue");

#ifndef LSM9DS1_FIFO_MODE
	hrtimer_init(&stat->hr_timer_acc, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	stat->hr_timer_acc.function = &poll_function_read_acc;
	hrtimer_init(&stat->hr_timer_gyr, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	stat->hr_timer_gyr.function = &poll_function_read_gyr;
#endif

	mutex_init(&stat->lock);
	mutex_lock(&stat->lock);
	stat->client = client;
	i2c_set_clientdata(client, stat);

	stat->pdata_main = kzalloc(sizeof(*stat->pdata_main), GFP_KERNEL);
	stat->pdata_acc = kzalloc(sizeof(*stat->pdata_acc), GFP_KERNEL);
	stat->pdata_gyr = kzalloc(sizeof(*stat->pdata_gyr), GFP_KERNEL);

	if ((stat->pdata_main == NULL) || (stat->pdata_acc == NULL) ||
						  (stat->pdata_gyr == NULL)){
		err = -ENOMEM;
		dev_err(&client->dev,
			"failed to allocate memory for pdata: %d\n", err);
		goto err_mutexunlock;
	}
	stat->pdata_main->pdata_acc = stat->pdata_acc;
	stat->pdata_main->pdata_gyr = stat->pdata_gyr;
#ifdef CONFIG_OF
	lsm9ds1_acc_gyr_parse_dt(stat, &client->dev);
#else
	if (client->dev.platform_data == NULL) {
		memcpy(stat->pdata_main, 
				&default_lsm9ds1_main_platform_data,
				sizeof(*stat->pdata_main));
		memcpy(stat->pdata_acc, &default_lsm9ds1_acc_pdata,
						sizeof(*stat->pdata_acc));
		memcpy(stat->pdata_gyr, &default_lsm9ds1_gyr_pdata,
						sizeof(*stat->pdata_gyr));
		dev_info(&client->dev, "using default plaform_data for "
					"accelerometer and gyroscope\n");
	} else {
		struct lsm9ds1_acc_gyr_main_platform_data *platform_data;
		platform_data = client->dev.platform_data;

		if(platform_data == NULL) {
			memcpy(stat->pdata_main,
				&default_lsm9ds1_main_platform_data,
				sizeof(*stat->pdata_main));
			dev_info(&client->dev, "using default plaform_data"
							" for accelerometer\n");
		} else {
			memcpy(stat->pdata_main, platform_data,
						sizeof(*stat->pdata_acc));
		}

		if(platform_data->pdata_acc == NULL) {
			memcpy(stat->pdata_acc, &default_lsm9ds1_acc_pdata,
						sizeof(*stat->pdata_acc));
			dev_info(&client->dev, "using default plaform_data"
							" for accelerometer\n");
		} else {
			memcpy(stat->pdata_acc, platform_data->pdata_acc,
						sizeof(*stat->pdata_acc));
		}

		if(platform_data->pdata_gyr == NULL) {
			memcpy(stat->pdata_gyr, &default_lsm9ds1_gyr_pdata,
						sizeof(*stat->pdata_gyr));
			dev_info(&client->dev, "using default plaform_data"
							" for gyroscope\n");
		} else {
			memcpy(stat->pdata_gyr, platform_data->pdata_gyr,
						sizeof(*stat->pdata_gyr));
		}
	}
#endif

	err = lsm9ds1_acc_validate_pdata(stat);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data for "
							"accelerometer \n");
		goto exit_kfree_pdata;
	}

	err = lsm9ds1_gyr_validate_pdata(stat);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data for "
							"gyroscope\n");
		goto exit_kfree_pdata;
	}

	if (stat->pdata_acc->init) {
		err = stat->pdata_acc->init();
		if (err < 0) {
			dev_err(&client->dev, "accelerometer init failed: "
								"%d\n", err);
			goto err_pdata_acc_init;
		}
	}
	if (stat->pdata_gyr->init) {
		err = stat->pdata_gyr->init();
		if (err < 0) {
			dev_err(&client->dev, "magnetometer init failed: "
								"%d\n", err);
			goto err_pdata_gyr_init;
		}
	}

	if(stat->pdata_main->gpio_int1 >= 0) {
		if (!gpio_is_valid(stat->pdata_main->gpio_int1)) {
  			dev_err(&client->dev, "The requested GPIO [%d] is not "
				"available\n", stat->pdata_main->gpio_int1);
			err = -EINVAL;
  			goto err_gpio1_valid;
		}

		err = gpio_request(stat->pdata_main->gpio_int1,
						"INTERRUPT_PIN1_LSM9DS1");
		if(err < 0) {
			dev_err(&client->dev, "Unable to request GPIO [%d].\n",
						stat->pdata_main->gpio_int1);
  			err = -EINVAL;
			goto err_gpio1_valid;
		}
		gpio_direction_input(stat->pdata_main->gpio_int1);
		stat->irq1 = gpio_to_irq(stat->pdata_main->gpio_int1);
		if(stat->irq1 < 0) {
			dev_err(&client->dev, "GPIO [%d] cannot be used as "
				"interrupt.\n",	stat->pdata_main->gpio_int1);
			err = -EINVAL;
			goto err_gpio1_irq;
		}
		pr_info("%s: %s has set irq1 to irq: %d, mapped on gpio:%d\n",
			LSM9DS1_ACC_GYR_DEV_NAME, __func__, stat->irq1,
						stat->pdata_main->gpio_int1);
	}

	if(stat->pdata_main->gpio_int2 >= 0) {
		if (!gpio_is_valid(stat->pdata_main->gpio_int2)) {
  			dev_err(&client->dev, "The requested GPIO [%d] is not "
				"available\n", stat->pdata_main->gpio_int2);
			err = -EINVAL;
  			goto err_gpio2_valid;
		}

		err = gpio_request(stat->pdata_main->gpio_int2,
						"INTERRUPT_PIN2_LSM9DS1");
		if(err < 0) {
			dev_err(&client->dev, "Unable to request GPIO [%d].\n",
						stat->pdata_main->gpio_int2);
  			err = -EINVAL;
			goto err_gpio2_valid;
		}
		gpio_direction_input(stat->pdata_main->gpio_int2);
		stat->irq2 = gpio_to_irq(stat->pdata_main->gpio_int2);
		if(stat->irq2 < 0) {
			dev_err(&client->dev, "GPIO [%d] cannot be used as "
				"interrupt.\n", stat->pdata_main->gpio_int2);
			err = -EINVAL;
			goto err_gpio2_irq;
		}
		pr_info("%s: %s has set irq2 to irq: %d, "
							"mapped on gpio:%d\n",
			LSM9DS1_ACC_GYR_DEV_NAME, __func__, stat->irq2,
						stat->pdata_main->gpio_int2);
	}

	err = lsm9ds1_acc_gyr_hw_init(stat);
	if (err < 0) {
		dev_err(&client->dev, "hw init failed: %d\n", err);
		goto err_hw_init;
	}

	err = lsm9ds1_acc_device_power_on(stat);
	if (err < 0) {
		dev_err(&client->dev, "accelerometer power on failed: "
								"%d\n", err);
		goto err_pdata_init;
	}

	err = lsm9ds1_gyr_device_power_on(stat);
	if (err < 0) {
		dev_err(&client->dev, "gyroscope power on failed: "
								"%d\n", err);
		goto err_pdata_init;
	}

	err = lsm9ds1_acc_update_fs_range(stat, stat->pdata_acc->fs_range);
	if (err < 0) {
		dev_err(&client->dev, "update accelerometer full scale range "
								"failed\n");
		goto  err_power_off_acc;
	}

	err = lsm9ds1_gyr_update_fs_range(stat, stat->pdata_gyr->fs_range);
	if (err < 0) {
		dev_err(&client->dev, "update gyroscope full scale range "
								"failed\n");
		goto  err_power_off_gyr;
	}

	err = lsm9ds1_acc_update_odr(stat, stat->pdata_acc->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update accelerometer ODR failed\n");
		goto  err_power_off;
	}

	err = lsm9ds1_gyr_update_odr(stat, stat->pdata_gyr->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update gyroscope ODR failed\n");
		goto  err_power_off;
	}

	err = lsm9ds1_acc_update_filter(stat, 
					stat->pdata_acc->aa_filter_bandwidth);
	if (err < 0) {
		dev_err(&client->dev, "update accelerometer filter "
								"failed\n");
		goto  err_power_off;
	}

	err = lsm9ds1_acc_input_init(stat);
	if (err < 0) {
		dev_err(&client->dev, "accelerometer input init failed\n");
		goto err_power_off;
	}

	err = lsm9ds1_gyr_input_init(stat);
	if (err < 0) {
		dev_err(&client->dev, "gyroscope input init failed\n");
		goto err_power_off;
	}

	err = create_sysfs_interfaces(&client->dev);
	if (err < 0) {
		dev_err(&client->dev, "device %s sysfs register failed\n",
			LSM9DS1_ACC_GYR_DEV_NAME);
		goto err_input_cleanup;
	}

	lsm9ds1_acc_device_power_off(stat);
	lsm9ds1_gyr_device_power_off(stat);
	mutex_unlock(&stat->lock);

#ifndef LSM9DS1_FIFO_MODE
	INIT_WORK(&stat->input_work_acc, poll_function_work_acc);
	INIT_WORK(&stat->input_work_gyr, poll_function_work_gyr);
#else
	INIT_WORK(&stat->input_work, fifo_function_work);
	if(stat->irq1){
		err = request_irq(stat->irq1, lsm9ds1_irq1_handler,  /*IRQF_TRIGGER_FALLING |*/ IRQF_TRIGGER_RISING | IRQF_ONESHOT, client->dev.driver->name, stat);
		if (err) {
			dev_err(&client->dev,"request_threaded_irq error\n");
			goto err_input_cleanup;
		}
		disable_irq(stat->irq1);
	}

	init_waitqueue_head(&stat->fifo_wq);
	err = misc_register(&lsm9ds1_dev);
	if (err) {
		dev_err(&client->dev,"register fifo device error\n");
		goto err_input_cleanup;
	}

	init_waitqueue_head(&stat->fifo_gyr_wq);
	err = misc_register(&lsm9ds1_gyr_dev);
	if (err) {
		dev_err(&client->dev,"register fifo device error\n");
		goto err_input_cleanup;
	}
#endif

	dev_info(&client->dev, "%s: probed\n", LSM9DS1_ACC_GYR_DEV_NAME);
	return 0;

err_input_cleanup:
	lsm9ds1_input_cleanup(stat);
err_power_off:
err_power_off_gyr:
	lsm9ds1_gyr_device_power_off(stat);
err_power_off_acc:
	lsm9ds1_acc_device_power_off(stat);
err_hw_init:
err_gpio2_irq:
	gpio_free(stat->pdata_main->gpio_int2);
err_gpio2_valid:
err_gpio1_irq:
	gpio_free(stat->pdata_main->gpio_int1);
err_gpio1_valid:
err_pdata_init:
err_pdata_gyr_init:
	if (stat->pdata_gyr->exit)
		stat->pdata_gyr->exit();
err_pdata_acc_init:
	if (stat->pdata_acc->exit)
		stat->pdata_acc->exit();
exit_kfree_pdata:
	kfree(stat->pdata_acc);
	kfree(stat->pdata_gyr);
err_mutexunlock:
	mutex_unlock(&stat->lock);
	kfree(stat);
	if(!lsm9ds1_acc_gyr_workqueue) {
		flush_workqueue(lsm9ds1_acc_gyr_workqueue);
		destroy_workqueue(lsm9ds1_acc_gyr_workqueue);
	}
exit_check_functionality_failed:
	dev_err(&stat->client->dev,"%s: Driver Init failed\n",
						LSM9DS1_ACC_GYR_DEV_NAME);
	return err;
}

static int lsm9ds1_acc_gyr_remove(struct i2c_client *client)
{
	struct lsm9ds1_acc_gyr_status *stat = i2c_get_clientdata(client);

	if (atomic_read(&stat->enabled_gyr)) {
		lsm9ds1_gyr_disable(stat);
		lsm9ds1_gyr_input_cleanup(stat);

		if (stat->pdata_gyr->exit)
			stat->pdata_gyr->exit();
	}

	lsm9ds1_acc_disable(stat);
	lsm9ds1_acc_input_cleanup(stat);

	remove_sysfs_interfaces(&client->dev);

	if (stat->pdata_acc->exit)
		stat->pdata_acc->exit();

	if(stat->pdata_main->gpio_int1 >= 0) {
		free_irq(stat->irq1, stat);
		gpio_free(stat->pdata_main->gpio_int1);
		destroy_workqueue(stat->irq1_work_queue);
	}

	if(stat->pdata_main->gpio_int2 >= 0) {
		free_irq(stat->irq2, stat);
		gpio_free(stat->pdata_main->gpio_int2);
		destroy_workqueue(stat->irq2_work_queue);
	}

	if(!lsm9ds1_acc_gyr_workqueue) {
		flush_workqueue(lsm9ds1_acc_gyr_workqueue);
		destroy_workqueue(lsm9ds1_acc_gyr_workqueue);
	}

	kfree(stat->pdata_acc);
	kfree(stat->pdata_gyr);
	kfree(stat->pdata_main);
	kfree(stat);
	return 0;
}

static const struct i2c_device_id lsm9ds1_acc_gyr_id[]
				= { { LSM9DS1_ACC_GYR_DEV_NAME, 0 }, { }, };

MODULE_DEVICE_TABLE(i2c, lsm9ds1_acc_gyr_id);

static struct i2c_driver lsm9ds1_acc_gyr_driver = {
	.driver = {
			.owner = THIS_MODULE,
			.name = LSM9DS1_ACC_GYR_DEV_NAME,
#ifdef CONFIG_OF
			.of_match_table = 
				of_match_ptr(lsm9ds1_acc_gyr_dt_id),
#endif
		  },
	.probe = lsm9ds1_acc_gyr_probe,
	.remove = lsm9ds1_acc_gyr_remove,
	.id_table = lsm9ds1_acc_gyr_id,
};

static int __init lsm9ds1_acc_gyr_init(void)
{
	return i2c_add_driver(&lsm9ds1_acc_gyr_driver);
}

static void __exit lsm9ds1_acc_gyr_exit(void)
{
	i2c_del_driver(&lsm9ds1_acc_gyr_driver);
}

module_init(lsm9ds1_acc_gyr_init);
module_exit(lsm9ds1_acc_gyr_exit);

MODULE_DESCRIPTION("lsm9ds1 accelerometer and gyroscope driver");
MODULE_AUTHOR("Giuseppe Barba, Matteo Dameno, Denis Ciocca,"
							" STMicroelectronics");
MODULE_LICENSE("GPL");
