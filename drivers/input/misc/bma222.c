/*
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2010 Bosch Sensortec GmbH
 * All Rights Reserved

 * Copyright (c) 2010 Yamaha Corporation
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>


#define BMA222_VERSION "1.2.0"

#define BMA222_NAME "bma222"

/* for debugging */
#define DEBUG 0
#define DEBUG_DELAY 0
#define DEBUG_THRESHOLD 0
#define TRACE_FUNC() pr_debug(BMA222_NAME ": <trace> %s()\n", __FUNCTION__)

/*
 * Default parameters
 */
#define BMA222_DEFAULT_DELAY            100
#define BMA222_MAX_DELAY                2000
#if defined (CONFIG_S5PC110_HAWK_BOARD) 
#define BMA222_FILTER_LEN        			8 /* nat */
#define BMA222_DEFAULT_THRESHOLD        5
#else
#define BMA222_DEFAULT_THRESHOLD        0
#endif

/*
 * Registers
 */
#define BMA222_CHIP_ID_REG              0x00
#define BMA222_CHIP_ID                  0x03

#define BMA222_ACC_REG                  0x02

#define BMA222_SOFT_RESET_REG           0x14
#define BMA222_SOFT_RESET_VAL           0xb6

#define BMA222_POWERMODE_REG            0x11
#define BMA222_POWERMODE_MASK           0xc0
#define BMA222_POWERMODE_SHIFT          6
#define BMA222_POWERMODE_NORMAL         0
#define BMA222_POWERMODE_LOW            1
#define BMA222_POWERMODE_OFF            2

#define BMA222_DATA_ENBL_REG            0x17
#define BMA222_DATA_ENBL_MASK           0x10
#define BMA222_DATA_ENBL_SHIFT          4

#define BMA222_SLEEP_DUR_REG            0x11
#define BMA222_SLEEP_DUR_MASK           0x1e
#define BMA222_SLEEP_DUR_SHIFT          1
#define BMA222_SLEEP_DUR_0              0
#define BMA222_SLEEP_DUR_1              6
#define BMA222_SLEEP_DUR_2              7
#define BMA222_SLEEP_DUR_4              8
#define BMA222_SLEEP_DUR_6              9
#define BMA222_SLEEP_DUR_10             10
#define BMA222_SLEEP_DUR_25             11
#define BMA222_SLEEP_DUR_50             12
#define BMA222_SLEEP_DUR_100            13
#define BMA222_SLEEP_DUR_500            14
#define BMA222_SLEEP_DUR_1000           15

#define BMA222_RANGE_REG                0x0f
#define BMA222_RANGE_MASK               0x0f
#define BMA222_RANGE_SHIFT              0
#define BMA222_RANGE_2G                 3
#define BMA222_RANGE_4G                 5
#define BMA222_RANGE_8G                 8
#define BMA222_RANGE_16G                12

#define BMA222_BANDWIDTH_REG            0x10
#define BMA222_BANDWIDTH_MASK           0x1f
#define BMA222_BANDWIDTH_SHIFT          0
#define BMA222_BANDWIDTH_1000HZ         15
#define BMA222_BANDWIDTH_500HZ          14
#define BMA222_BANDWIDTH_250HZ          13
#define BMA222_BANDWIDTH_125HZ          12
#define BMA222_BANDWIDTH_63HZ           11
#define BMA222_BANDWIDTH_32HZ           10
#define BMA222_BANDWIDTH_16HZ           9
#define BMA222_BANDWIDTH_8HZ            8

#define BMA222_EEPROM_CTRL_REG          0x33
#define BMA222_EEPROM_CTRL_MASK         0x0f
#define BMA222_EEPROM_CTRL_SHIFT        0

/* SETTING THIS BIT  UNLOCK'S WRITING SETTING REGISTERS TO EEPROM */
#define BMA222_UNLOCK_EE_WRITE_SETTING_SHIFT   0
#define BMA222_UNLOCK_EE_WRITE_SETTING_LEN     1
#define BMA222_UNLOCK_EE_WRITE_SETTING_MASK    0x01
#define BMA222_UNLOCK_EE_WRITE_SETTING_REG     BMA222_EEPROM_CTRL_REG


/* SETTING THIS BIT STARTS WRITING SETTING REGISTERS TO EEPROM */
#define BMA222_START_EE_WRITE_SETTING_SHIFT    1
#define BMA222_START_EE_WRITE_SETTING_LEN      1
#define BMA222_START_EE_WRITE_SETTING_MASK     0x02
#define BMA222_START_EE_WRITE_SETTING_REG      BMA222_EEPROM_CTRL_REG


/* STATUS OF WRITING TO EEPROM */
#define BMA222_EE_WRITE_SETTING_S_SHIFT        2
#define BMA222_EE_WRITE_SETTING_S_LEN          1
#define BMA222_EE_WRITE_SETTING_S_MASK         0x04
#define BMA222_EE_WRITE_SETTING_S_REG          BMA222_EEPROM_CTRL_REG


/* UPDATE IMAGE REGISTERS WRITING TO EEPROM */
#define BMA222_UPDATE_IMAGE_SHIFT              3
#define BMA222_UPDATE_IMAGE_LEN                1
#define BMA222_UPDATE_IMAGE_MASK               0x08
#define BMA222_UPDATE_IMAGE_REG                BMA222_EEPROM_CTRL_REG


/* STATUS OF IMAGE REGISTERS WRITING TO EEPROM */
#define BMA222_IMAGE_REG_EE_WRITE_S_SHIFT      3
#define BMA222_IMAGE_REG_EE_WRITE_S_LEN        1
#define BMA222_IMAGE_REG_EE_WRITE_S_MASK       0x08
#define BMA222_IMAGE_REG_EE_WRITE_S_REG        BMA222_EEPROM_CTRL_REG


#define BMA222_SERIAL_CTRL_REG          0x34
#define BMA222_SERIAL_CTRL_MASK         0x07
#define BMA222_SERIAL_CTRL_SHIFT        0

#define BMA222_OFFSET_CTRL_REG          0x36
#define BMA222_OFFSET_CTRL_MASK         0xff
#define BMA222_OFFSET_CTRL_SHIFT        0

/**    SLOW COMPENSATION FOR X,Y,Z AXIS      **/

#define BMA222_EN_SLOW_COMP_X_SHIFT            0
#define BMA222_EN_SLOW_COMP_X_LEN              1
#define BMA222_EN_SLOW_COMP_X_MASK             0x01
#define BMA222_EN_SLOW_COMP_X_REG              BMA222_OFFSET_CTRL_REG

#define BMA222_EN_SLOW_COMP_Y_SHIFT            1
#define BMA222_EN_SLOW_COMP_Y_LEN              1
#define BMA222_EN_SLOW_COMP_Y_MASK             0x02
#define BMA222_EN_SLOW_COMP_Y_REG              BMA222_OFFSET_CTRL_REG

#define BMA222_EN_SLOW_COMP_Z_SHIFT            2
#define BMA222_EN_SLOW_COMP_Z_LEN              1
#define BMA222_EN_SLOW_COMP_Z_MASK             0x04
#define BMA222_EN_SLOW_COMP_Z_REG              BMA222_OFFSET_CTRL_REG

#define BMA222_EN_SLOW_COMP_XYZ_SHIFT            0
#define BMA222_EN_SLOW_COMP_XYZ_LEN              3
#define BMA222_EN_SLOW_COMP_XYZ_MASK             0x07
#define BMA222_EN_SLOW_COMP_XYZ_REG              BMA222_OFFSET_CTRL_REG

/**    FAST COMPENSATION READY FLAG          **/

#define BMA222_FAST_COMP_RDY_S_SHIFT           4
#define BMA222_FAST_COMP_RDY_S_LEN             1
#define BMA222_FAST_COMP_RDY_S_MASK            0x10
#define BMA222_FAST_COMP_RDY_S_REG             BMA222_OFFSET_CTRL_REG

/**    FAST COMPENSATION FOR X,Y,Z AXIS      **/

#define BMA222_EN_FAST_COMP_SHIFT              5
#define BMA222_EN_FAST_COMP_LEN                2
#define BMA222_EN_FAST_COMP_MASK               0x60
#define BMA222_EN_FAST_COMP_REG                BMA222_OFFSET_CTRL_REG

/**    RESET OFFSET REGISTERS                **/

#define BMA222_RESET_OFFSET_REGS_SHIFT         7
#define BMA222_RESET_OFFSET_REGS_LEN           1
#define BMA222_RESET_OFFSET_REGS_MASK          0x80
#define BMA222_RESET_OFFSET_REGS_REG           BMA222_OFFSET_CTRL_REG

#define BMA222_OFFSET_PARAMS_REG        0x37
#define BMA222_OFFSET_PARAMS_MASK       0x7f
#define BMA222_OFFSET_PARAMS_SHIFT      0

#define BMA222_COMP_CUTOFF_SHIFT               0
#define BMA222_COMP_CUTOFF_LEN                 1
#define BMA222_COMP_CUTOFF_MASK                0x01
#define BMA222_COMP_CUTOFF_REG                 BMA222_OFFSET_PARAMS_REG

/**     COMPENSATION TARGET                  **/

#define BMA222_COMP_TARGET_OFFSET_X_SHIFT      1
#define BMA222_COMP_TARGET_OFFSET_X_LEN        2
#define BMA222_COMP_TARGET_OFFSET_X_MASK       0x06
#define BMA222_COMP_TARGET_OFFSET_X_REG        BMA222_OFFSET_PARAMS_REG

#define BMA222_COMP_TARGET_OFFSET_Y_SHIFT      3
#define BMA222_COMP_TARGET_OFFSET_Y_LEN        2
#define BMA222_COMP_TARGET_OFFSET_Y_MASK       0x18
#define BMA222_COMP_TARGET_OFFSET_Y_REG        BMA222_OFFSET_PARAMS_REG

#define BMA222_COMP_TARGET_OFFSET_Z_SHIFT      5
#define BMA222_COMP_TARGET_OFFSET_Z_LEN        2
#define BMA222_COMP_TARGET_OFFSET_Z_MASK       0x60
#define BMA222_COMP_TARGET_OFFSET_Z_REG        BMA222_OFFSET_PARAMS_REG

#define BMA222_OFFSET_FILT_X_REG        0x38
#define BMA222_OFFSET_FILT_X_MASK       0xff
#define BMA222_OFFSET_FILT_X_SHIFT      0

#define BMA222_OFFSET_FILT_Y_REG        0x39
#define BMA222_OFFSET_FILT_Y_MASK       0xff
#define BMA222_OFFSET_FILT_Y_SHIFT      0

#define BMA222_OFFSET_FILT_Z_REG        0x3A
#define BMA222_OFFSET_FILT_Z_MASK       0xff
#define BMA222_OFFSET_FILT_Z_SHIFT      0

#define BMA222_OFFSET_UNFILT_X_REG      0x3B
#define BMA222_OFFSET_UNFILT_X_MASK     0xff
#define BMA222_OFFSET_UNFILT_X_SHIFT    0

#define BMA222_OFFSET_UNFILT_Y_REG      0x3C
#define BMA222_OFFSET_UNFILT_Y_MASK     0xff
#define BMA222_OFFSET_UNFIILT_Y_SHIFT   0

#define BMA222_OFFSET_UNFILT_Z_REG      0x3D
#define BMA222_OFFSET_UNFILT_Z_MASK     0xff
#define BMA222_OFFSET_UNFILT_Z_SHIFT    0

/* ioctl commnad */
#define DCM_IOC_MAGIC			's'
#define BMA222_CALIBRATION		_IOWR(DCM_IOC_MAGIC, 48, short)

/*
 * Acceleration measurement
 */
#define BMA222_RESOLUTION               64

/* ABS axes parameter range [um/s^2] (for input event) */
#define GRAVITY_EARTH                   9806550
#define ABSMIN_2G                       (-GRAVITY_EARTH * 2)
#define ABSMAX_2G                       (GRAVITY_EARTH * 2)

struct acceleration {
    int x;
    int y;
    int z;
};

/*
 * Sleep duration
 */
struct bma222_sd {
    u8 bw;
    u8 sd;
};

static const struct bma222_sd bma222_sd_table[] = {
    {BMA222_BANDWIDTH_8HZ    /* 128ms */, BMA222_SLEEP_DUR_100},
    {BMA222_BANDWIDTH_16HZ   /*  64ms */, BMA222_SLEEP_DUR_50},
    {BMA222_BANDWIDTH_32HZ   /*  32ms */, BMA222_SLEEP_DUR_25},
    {BMA222_BANDWIDTH_63HZ   /*  16ms */, BMA222_SLEEP_DUR_10},
    {BMA222_BANDWIDTH_125HZ  /*   8ms */, BMA222_SLEEP_DUR_6},
    {BMA222_BANDWIDTH_250HZ  /*   4ms */, BMA222_SLEEP_DUR_2},
    {BMA222_BANDWIDTH_500HZ  /*   2ms */, BMA222_SLEEP_DUR_1},
    {BMA222_BANDWIDTH_1000HZ /*   1ms */, BMA222_SLEEP_DUR_0},
};

/*
 * Output data rate
 */
struct bma222_odr {
    unsigned long delay;            /* min delay (msec) in the range of ODR */
    u8 odr;                         /* bandwidth register value */
};

static const struct bma222_odr bma222_odr_table[] = {
#if ! defined (CONFIG_S5PC110_HAWK_BOARD) /* nathan */
    {1,   BMA222_BANDWIDTH_1000HZ},
    {2,   BMA222_BANDWIDTH_500HZ},
    {4,   BMA222_BANDWIDTH_250HZ}, 
    {8,   BMA222_BANDWIDTH_125HZ},
#endif
    {16,  BMA222_BANDWIDTH_63HZ},
    {32,  BMA222_BANDWIDTH_32HZ},
    {64,  BMA222_BANDWIDTH_16HZ},
    {128, BMA222_BANDWIDTH_8HZ},
};

#if defined (CONFIG_S5PC110_HAWK_BOARD) 
struct bma222_fir_filter {
	int num;
	int filter_len;
	int index;
	int32_t sequence[BMA222_FILTER_LEN];
};
#endif

/*
 * Transformation matrix for chip mounting position
 */
static const int bma222_position_map[][3][3] = {
    {{ 0, -1,  0}, { 1,  0,  0}, { 0,  0,  1}}, /* top/upper-left */
    {{ 1,  0,  0}, { 0,  1,  0}, { 0,  0,  1}}, /* top/upper-right */
    {{ 0,  1,  0}, {-1,  0,  0}, { 0,  0,  1}}, /* top/lower-right */
    {{-1,  0,  0}, { 0, -1,  0}, { 0,  0,  1}}, /* top/lower-left */
    {{ 0,  1,  0}, { 1,  0,  0}, { 0,  0, -1}}, /* bottom/upper-right */
    {{-1,  0,  0}, { 0,  1,  0}, { 0,  0, -1}}, /* bottom/upper-left */
    {{ 0, -1,  0}, {-1,  0,  0}, { 0,  0, -1}}, /* bottom/lower-left */
    {{ 1,  0,  0}, { 0, -1,  0}, { 0,  0, -1}}, /* bottom/lower-right */
};

/*
 * driver private data
 */
struct bma222_data {
    atomic_t enable;                /* attribute value */
    atomic_t delay;                 /* attribute value */
    atomic_t position;              /* attribute value */
    atomic_t threshold;             /* attribute value */
    struct acceleration last;       /* last measured data */
    struct mutex enable_mutex;
    struct mutex data_mutex;
    struct i2c_client *client;
    struct input_dev *input;
    struct delayed_work work;
    struct miscdevice bma222_device;
#if defined (CONFIG_S5PC110_HAWK_BOARD) 
    struct bma222_fir_filter filter[3];
#endif
#if DEBUG
    int suspend;
#endif
};

#define delay_to_jiffies(d) ((d)?msecs_to_jiffies(d):1)
#define actual_delay(d)     (jiffies_to_msecs(delay_to_jiffies(d)))

/* register access functions */
#define bma222_read_bits(p,r) \
    ((i2c_smbus_read_byte_data((p)->client, r##_REG) & r##_MASK) >> r##_SHIFT)

#define bma222_update_bits(p,r,v) \
    i2c_smbus_write_byte_data((p)->client, r##_REG, \
                              ((i2c_smbus_read_byte_data((p)->client,r##_REG) & ~r##_MASK) | (((v) << r##_SHIFT) & r##_MASK)))

#if defined (CONFIG_S5PC110_HAWK_BOARD) 
static void fir_filter_init(struct bma222_fir_filter *filter, int len)
{
	int i;

	filter->num = 0;
	filter->index = 0;
	filter->filter_len = len;

	for (i = 0; i < filter->filter_len; ++i)
		filter->sequence[i] = 0;
}

static int fir_filter_filter(struct bma222_fir_filter *filter, int in)
{
	int out = 0;
	int i;

	if (filter->filter_len == 0)
		return in;
	if (filter->num < filter->filter_len) {
		filter->sequence[filter->index++] = in;
		filter->num++;
		return in;
	} else {
		if (filter->filter_len <= filter->index)
			filter->index = 0;
		filter->sequence[filter->index++] = in;

		for (i = 0; i < filter->filter_len; i++)
			out += filter->sequence[i];
		return out / filter->filter_len;
	}
}

static void filter_init(struct bma222_data *bma222)
{
	int i;

	for (i = 0; i < 3; i++)
		fir_filter_init(&bma222->filter[i], BMA222_FILTER_LEN);
}

static void filter_filter(struct bma222_data *bma222, int *orig, int *filtered)
{
	int i;

	for (i = 0; i < 3; i++)
		filtered[i] = fir_filter_filter(&bma222->filter[i], orig[i]);
}

/* 
static void filter_stabilizer(struct bma222_data *bma222, int *orig, int *stabled)
{
	int i;
	static int buffer[3] = { 0, };

	for (i = 0; i < 3; i++) {
		if ((buffer[i] - orig[i] >= BMA023_STABLE_TH)
			|| (buffer[i] - orig[i] <= -BMA023_STABLE_TH)) {
			stabled[i] = orig[i];
			buffer[i] = stabled[i];
		} else
			stabled[i] = buffer[i];
	}
}
*/

#endif

/*
 * Device dependant operations
 */
static int bma222_set_sleep_dur(struct bma222_data *bma222, u8 bw)
{
    int i;
    int delay = atomic_read(&bma222->delay);

    if (bw == BMA222_BANDWIDTH_8HZ) {
        if (1000 < delay && delay < 2000) {
            return BMA222_SLEEP_DUR_500;
        }
        if (2000 <= delay) {
            return BMA222_SLEEP_DUR_1000;
        }
    }
    for (i = 0; i < sizeof(bma222_sd_table) / sizeof(struct bma222_sd); i++) {
        if (bma222_sd_table[i].bw == bw) {
            /* Success */
            return bma222_sd_table[i].sd;
        }
    }

    /* Error */
    return -1;
}

static int bma222_power_up(struct bma222_data *bma222)
{
    /* set new data interrupt enable bit */
    bma222_update_bits(bma222, BMA222_DATA_ENBL, 1);

    /* set powermode low */
    bma222_update_bits(bma222, BMA222_POWERMODE, BMA222_POWERMODE_NORMAL);

    return 0;
}

static int bma222_power_down(struct bma222_data *bma222)
{
    /* set new data interrupt disable bit */
    bma222_update_bits(bma222, BMA222_DATA_ENBL, 0);

    /* set powermode suspend */
    bma222_update_bits(bma222, BMA222_POWERMODE, BMA222_POWERMODE_OFF);

    return 0;
}

static int bma222_hw_init(struct bma222_data *bma222)
{
    /* reset hardware */
    i2c_smbus_write_byte_data(bma222->client, BMA222_SOFT_RESET_REG, BMA222_SOFT_RESET_VAL);
    while (i2c_smbus_read_byte_data(bma222->client, BMA222_SOFT_RESET_REG)) {
        msleep(1);
    }

    /* set range +-2G */
    bma222_update_bits(bma222, BMA222_RANGE, BMA222_RANGE_2G);

    return 0;
}

static int bma222_get_enable(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct bma222_data *bma222 = i2c_get_clientdata(client);

    return atomic_read(&bma222->enable);
}

static void bma222_set_enable(struct device *dev, int enable)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct bma222_data *bma222 = i2c_get_clientdata(client);
    int delay = atomic_read(&bma222->delay);

    mutex_lock(&bma222->enable_mutex);

    if (enable) {                   /* enable if state will be changed */
        if (!atomic_cmpxchg(&bma222->enable, 0, 1)) {
            bma222_power_up(bma222);
            schedule_delayed_work(&bma222->work, delay_to_jiffies(delay) + 1);
        }
    } else {                        /* disable if state will be changed */
        if (atomic_cmpxchg(&bma222->enable, 1, 0)) {
            cancel_delayed_work_sync(&bma222->work);
            bma222_power_down(bma222);
        }
    }
    atomic_set(&bma222->enable, enable);

    mutex_unlock(&bma222->enable_mutex);
}

static int bma222_get_delay(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct bma222_data *bma222 = i2c_get_clientdata(client);

    return atomic_read(&bma222->delay);
}

static void bma222_set_delay(struct device *dev, int delay)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct bma222_data *bma222 = i2c_get_clientdata(client);
    u8 odr;
    int i;

    /* determine optimum ODR */
    for (i = 1; (i < ARRAY_SIZE(bma222_odr_table)) &&
             (actual_delay(delay) >= bma222_odr_table[i].delay); i++)
        ;
    odr = bma222_odr_table[i-1].odr;
    atomic_set(&bma222->delay, delay);

    mutex_lock(&bma222->enable_mutex);

    if (bma222_get_enable(dev)) {
        cancel_delayed_work_sync(&bma222->work);
        bma222_update_bits(bma222, BMA222_BANDWIDTH, odr);
        bma222_update_bits(bma222, BMA222_SLEEP_DUR, bma222_set_sleep_dur(bma222, odr));
        schedule_delayed_work(&bma222->work, delay_to_jiffies(delay) + 1);
    } else {
        bma222_power_up(bma222);
        bma222_update_bits(bma222, BMA222_BANDWIDTH, odr);
        bma222_update_bits(bma222, BMA222_SLEEP_DUR, bma222_set_sleep_dur(bma222, odr));
        bma222_power_down(bma222);
    }

    mutex_unlock(&bma222->enable_mutex);
}

static int bma222_get_position(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct bma222_data *bma222 = i2c_get_clientdata(client);

    return atomic_read(&bma222->position);
}

static void bma222_set_position(struct device *dev, int position)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct bma222_data *bma222 = i2c_get_clientdata(client);

    atomic_set(&bma222->position, position);
}

static int bma222_get_threshold(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct bma222_data *bma222 = i2c_get_clientdata(client);

    return atomic_read(&bma222->threshold);
}

static void bma222_set_threshold(struct device *dev, int threshold)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct bma222_data *bma222 = i2c_get_clientdata(client);

    atomic_set(&bma222->threshold, threshold);
}

static int bma222_data_filter(struct bma222_data *bma222, struct acceleration *accel, int data[])
{
#if DEBUG_THRESHOLD
    struct i2c_client *client = bma222->client;
#endif
    int threshold = atomic_read(&bma222->threshold);
#if DEBUG_THRESHOLD
    int update;
#endif
#if DEBUG_THRESHOLD
    update = 0;
#endif
    mutex_lock(&bma222->data_mutex);
    if ((abs(bma222->last.x - data[0]) > threshold) ||
        (abs(bma222->last.y - data[1]) > threshold) ||
        (abs(bma222->last.z - data[2]) > threshold)) {
        accel->x = data[0];
        accel->y = data[1];
        accel->z = data[2];
#if DEBUG_THRESHOLD
        update = 1;
#endif
    } else {
        *accel = bma222->last;
    }

#if DEBUG_THRESHOLD
    if (update == 1) {
        dev_info(&client->dev, "threshold=%d x(%d) y(%d) z(%d) accel(%d,%d,%d) ****\n", threshold,
                 bma222->last.x - data[0], bma222->last.y - data[1], bma222->last.z - data[2], accel->x, accel->y, accel->z);
    } else {
        dev_info(&client->dev, "threshold=%d x(%d) y(%d) z(%d) accel(%d,%d,%d)\n", threshold,
                 bma222->last.x - data[0], bma222->last.y - data[1], bma222->last.z - data[2], accel->x, accel->y, accel->z);
    }
#endif
    mutex_unlock(&bma222->data_mutex);

    return 0;
}

static int bma222_measure(struct bma222_data *bma222, struct acceleration *accel)
{
    struct i2c_client *client = bma222->client;
    u8 buf[6];
    int raw[3], data[3];
    int pos = atomic_read(&bma222->position);
    long long g;
    int i, j;
#if DEBUG_DELAY
    struct timespec t;
#endif
#if defined (CONFIG_S5PC110_HAWK_BOARD) 
    int filtered_data[3];
#endif

#if DEBUG_DELAY
    getnstimeofday(&t);
#endif

    /* read acceleration data */
    if (i2c_smbus_read_i2c_block_data(client, BMA222_ACC_REG, 6, buf) != 6) {
        dev_err(&client->dev,
                "I2C block read error: addr=0x%02x, len=%d\n",
                BMA222_ACC_REG, 6);
        for (i = 0; i < 3; i++) {
            raw[i] = 0;
        }
    } else {
        for (i = 0; i < 3; i++) {
            raw[i] = *(s8 *)&buf[i*2+1];
        }
    }

    /* for X, Y, Z axis */
    for (i = 0; i < 3; i++) {
        /* coordinate transformation */
        data[i] = 0;
        for (j = 0; j < 3; j++) {
            data[i] += raw[j] * bma222_position_map[pos][i][j];
        }
        /* normalization */
        g = (long long)data[i]*4; /* nat g = (long long)data[i] * GRAVITY_EARTH / BMA222_RESOLUTION; */        
        data[i] = g;
    }

    dev_dbg(&client->dev, "raw(%5d,%5d,%5d) => norm(%8d,%8d,%8d)\n",
            raw[0], raw[1], raw[2], data[0], data[1], data[2]);

#if DEBUG_DELAY
    dev_info(&client->dev, "%ld.%lds:raw(%5d,%5d,%5d) => norm(%8d,%8d,%8d)\n", t.tv_sec, t.tv_nsec,
             raw[0], raw[1], raw[2], data[0], data[1], data[2]);
#endif

#if defined (CONFIG_S5PC110_HAWK_BOARD) 
    filter_filter(bma222, data, filtered_data); /* filter out sizzling values */
    bma222_data_filter(bma222, accel, filtered_data);
#else
    bma222_data_filter(bma222, accel, data);
#endif  

    return 0;
}

static void bma222_work_func(struct work_struct *work)
{
    struct bma222_data *bma222 = container_of((struct delayed_work *)work,
                                              struct bma222_data, work);
    struct acceleration accel;
    unsigned long delay = delay_to_jiffies(atomic_read(&bma222->delay));

    bma222_measure(bma222, &accel);

	input_report_rel(bma222->input, REL_X, accel.x);
	input_report_rel(bma222->input, REL_Y, accel.y);
	input_report_rel(bma222->input, REL_Z, accel.z);
	input_sync(bma222->input);

    mutex_lock(&bma222->data_mutex);
    bma222->last = accel;
    mutex_unlock(&bma222->data_mutex);

    schedule_delayed_work(&bma222->work, delay);
}

/*
 * Input device interface
 */
static int bma222_input_init(struct bma222_data *bma222)
{
    struct input_dev *dev;
    int err;

    dev = input_allocate_device();
    if (!dev) {
        return -ENOMEM;
    }
    dev->name = "accelerometer_sensor";
    dev->id.bustype = BUS_I2C;
	
	input_set_capability(dev, EV_REL, REL_RY);
	input_set_capability(dev, EV_REL, REL_X);
	input_set_capability(dev, EV_REL, REL_Y);
	input_set_capability(dev, EV_REL, REL_Z);
	input_set_drvdata(dev, bma222);

    err = input_register_device(dev);
    if (err < 0) {
        input_free_device(dev);
        return err;
    }
    bma222->input = dev;

    return 0;
}

static void bma222_input_fini(struct bma222_data *bma222)
{
    struct input_dev *dev = bma222->input;

    input_unregister_device(dev);
    input_free_device(dev);
}
   
static int bma222_fast_calibration(struct bma222_data *bma222, char layout[])
{
	char tmp = 1;	// select x axis in cal_trigger by default
	int power_off_after_calibration = 0;
	struct i2c_client *client = bma222->client;

	mutex_lock(&bma222->enable_mutex);

	if(!bma222_get_enable(&client->dev)) {
		bma222_power_up(bma222);
		power_off_after_calibration = 1;
	}

	bma222_update_bits(bma222, BMA222_COMP_TARGET_OFFSET_X, layout[0]);
	bma222_update_bits(bma222, BMA222_EN_FAST_COMP, tmp);
	
	do
	{
		mdelay(2);
		tmp = bma222_read_bits(bma222, BMA222_FAST_COMP_RDY_S);
	} while(tmp == 0);

	bma222_update_bits(bma222, BMA222_COMP_TARGET_OFFSET_Y, layout[1]);
	tmp = 2;	//selet y axis in cal_trigger
	bma222_update_bits(bma222, BMA222_EN_FAST_COMP, tmp);
	do
	{
		mdelay(2); 
		tmp = bma222_read_bits(bma222, BMA222_FAST_COMP_RDY_S);
	} while(tmp == 0);

	bma222_update_bits(bma222, BMA222_COMP_TARGET_OFFSET_Z, layout[2]);
	tmp = 3;	//selet z axis in cal_trigger
	bma222_update_bits(bma222, BMA222_EN_FAST_COMP, tmp);
	do
	{
		mdelay(2); 
		tmp = bma222_read_bits(bma222, BMA222_FAST_COMP_RDY_S);
	} while(tmp == 0);

	tmp = 1;	//unlock eeprom
	bma222_update_bits(bma222, BMA222_UNLOCK_EE_WRITE_SETTING, tmp);
	bma222_update_bits(bma222, BMA222_START_EE_WRITE_SETTING, 0x01);

	do
	{
		mdelay(2); 
		tmp = bma222_read_bits(bma222, BMA222_EE_WRITE_SETTING_S);
	} while(tmp==0);

	tmp = 0; 	//lock eemprom	
	bma222_update_bits(bma222, BMA222_UNLOCK_EE_WRITE_SETTING, tmp);

	printk("(%s) #8\n", __func__);

	if(power_off_after_calibration) {
		printk("(%s) #8 - 1\n", __func__);
		bma222_power_down(bma222);
	}

	printk("(%s) #9\n", __func__);
	
	mutex_unlock(&bma222->enable_mutex);

	return 0;
}
 

/*
 * sysfs device attributes
 */
static ssize_t bma222_enable_show(struct device *dev,
                                  struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", bma222_get_enable(dev));
}

static ssize_t bma222_enable_store(struct device *dev,
                                   struct device_attribute *attr,
                                   const char *buf, size_t count)
{
    unsigned long enable = simple_strtoul(buf, NULL, 10);

    if ((enable == 0) || (enable == 1)) {
        bma222_set_enable(dev, enable);
    }

    return count;
}

static ssize_t bma222_delay_show(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", bma222_get_delay(dev));
}

static ssize_t bma222_delay_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    unsigned long delay = simple_strtoul(buf, NULL, 10);

    if (delay > BMA222_MAX_DELAY) {
        delay = BMA222_MAX_DELAY;
    }

    bma222_set_delay(dev, delay);

    return count;
}

static ssize_t bma222_position_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", bma222_get_position(dev));
}

static ssize_t bma222_position_store(struct device *dev,
                                     struct device_attribute *attr,
                                     const char *buf, size_t count)
{
    unsigned long position;

    position = simple_strtoul(buf, NULL,10);
    if ((position >= 0) && (position <= 7)) {
        bma222_set_position(dev, position);
    }

    return count;
}

static ssize_t bma222_threshold_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", bma222_get_threshold(dev));
}

static ssize_t bma222_threshold_store(struct device *dev,
                                      struct device_attribute *attr,
                                      const char *buf, size_t count)
{
    unsigned long threshold;

    threshold = simple_strtoul(buf, NULL,10);
    if (threshold >= 0 && threshold <= ABSMAX_2G) {
        bma222_set_threshold(dev, threshold);
    }

    return count;
}

static ssize_t bma222_wake_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
    struct input_dev *input = to_input_dev(dev);
    static atomic_t serial = ATOMIC_INIT(0);

    input_report_abs(input, ABS_MISC, atomic_inc_return(&serial));

    return count;
}

static ssize_t bma222_data_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct bma222_data *bma222 = input_get_drvdata(input);
    struct acceleration accel;

    mutex_lock(&bma222->data_mutex);
    accel = bma222->last;
    mutex_unlock(&bma222->data_mutex);

    return sprintf(buf, "%d %d %d\n", accel.x, accel.y, accel.z);
}

#if DEBUG
static ssize_t bma222_debug_reg_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct bma222_data *bma222 = input_get_drvdata(input);
    struct i2c_client *client = bma222->client;
    ssize_t count = 0;
    u8 reg[0x3e];
    int i;

    i2c_smbus_read_i2c_block_data(client, 0x00, 0x3e, reg);
    for (i = 0; i < 0x3e; i++) {
        count += sprintf(&buf[count], "%02x: %d\n", i, reg[i]);
    }

    return count;
}

static int bma222_suspend(struct i2c_client *client, pm_message_t mesg);
static int bma222_resume(struct i2c_client *client);

static ssize_t bma222_debug_suspend_show(struct device *dev,
                                         struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct bma222_data *bma222 = input_get_drvdata(input);

    return sprintf(buf, "%d\n", bma222->suspend);
}

static ssize_t bma222_debug_suspend_store(struct device *dev,
                                          struct device_attribute *attr,
                                          const char *buf, size_t count)
{
    struct input_dev *input = to_input_dev(dev);
    struct bma222_data *bma222 = input_get_drvdata(input);
    struct i2c_client *client = bma222->client;
    unsigned long suspend = simple_strtoul(buf, NULL, 10);

    if (suspend) {
        pm_message_t msg;
        bma222_suspend(client, msg);
    } else {
        bma222_resume(client);
    }

    return count;
}
#endif /* DEBUG */

static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
                   bma222_enable_show, bma222_enable_store);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,
                   bma222_delay_show, bma222_delay_store);
static DEVICE_ATTR(position, S_IRUGO|S_IWUSR,
                   bma222_position_show, bma222_position_store);
static DEVICE_ATTR(threshold, S_IRUGO|S_IWUSR,
                   bma222_threshold_show, bma222_threshold_store);
static DEVICE_ATTR(wake, S_IWUSR|S_IWGRP,
                   NULL, bma222_wake_store);
static DEVICE_ATTR(data, S_IRUGO,
                   bma222_data_show, NULL);

#if DEBUG
static DEVICE_ATTR(debug_reg, S_IRUGO,
                   bma222_debug_reg_show, NULL);
static DEVICE_ATTR(debug_suspend, S_IRUGO|S_IWUSR,
                   bma222_debug_suspend_show, bma222_debug_suspend_store);
#endif /* DEBUG */

static struct attribute *bma222_attributes[] = {
    &dev_attr_enable.attr,
    &dev_attr_delay.attr,
    &dev_attr_position.attr,
    &dev_attr_threshold.attr,
    &dev_attr_wake.attr,
    &dev_attr_data.attr,
#if DEBUG
    &dev_attr_debug_reg.attr,
    &dev_attr_debug_suspend.attr,
#endif /* DEBUG */
    NULL
};

static struct attribute_group bma222_attribute_group = {
    .attrs = bma222_attributes
};

/*
 * I2C client
 */
static int bma222_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    int id;

    id = i2c_smbus_read_byte_data(client, BMA222_CHIP_ID_REG);
    if (id != BMA222_CHIP_ID)
        return -ENODEV;

    return 0;
}

static int bma222_open(struct inode *inode, struct file *file)
{
	struct bma222_data *bma222 = container_of(file->private_data,
						struct bma222_data,
						bma222_device);
	file->private_data = bma222;
	return 0;
}

static int bma222_close(struct inode *inode, struct file *file)
{
	return 0;
}

static int bma222_ioctl(struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	struct bma222_data *bma222 = file->private_data;
	int err = 0;
	unsigned char data[6];
	struct acceleration accel;
	char layout[3] = {0, 0, 1};

	switch (cmd) {
	case BMA222_CALIBRATION:	
	
		bma222_measure(bma222, &accel);
		printk("[bma222_ioctl] ++ X: %d Y: %d Z: %d \n",accel.x, accel.y, accel.z);
		
		if (copy_from_user((struct acceleration *)data, (struct acceleration *)arg, 6) != 0) {
			pr_err("copy_from_user error\n");
			return -EFAULT;
		}
		//printk("[bma222_ioctl] data ={%d, %d, %d, %d, %d, %d} \n",data[0], data[1], data[2], data[3], data[4], data[5]);
		
		err = bma222_fast_calibration(bma222, layout);

		bma222_measure(bma222, &accel);
		printk("[bma222_ioctl] --  X: %d Y: %d Z: %d \n",accel.x, accel.y, accel.z);
		return err;

	default:
		break;
	}
	return 0;
}

static const struct file_operations bma222_fops = {
	.owner = THIS_MODULE,
	.open = bma222_open,
	.release = bma222_close,
	.ioctl = bma222_ioctl,
};

static int bma222_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct bma222_data *bma222;
    int err;

    /* setup private data */
    bma222 = kzalloc(sizeof(struct bma222_data), GFP_KERNEL);
    if (!bma222) {
        err = -ENOMEM;
        goto error_0;
    }
    mutex_init(&bma222->enable_mutex);
    mutex_init(&bma222->data_mutex);

    /* setup i2c client */
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        err = -ENODEV;
        goto error_1;
    }
    i2c_set_clientdata(client, bma222);
    bma222->client = client;

    /* detect and init hardware */
    if ((err = bma222_detect(client, NULL))) {
        goto error_1;
    }
    dev_info(&client->dev, "%s found\n", id->name);

    bma222_hw_init(bma222);
    bma222_set_delay(&client->dev, BMA222_DEFAULT_DELAY);
    bma222_set_position(&client->dev, CONFIG_INPUT_BMA222_POSITION);
    bma222_set_threshold(&client->dev, BMA222_DEFAULT_THRESHOLD);
    
#if defined (CONFIG_S5PC110_HAWK_BOARD) 
    filter_init(bma222);
#endif

    /* setup driver interfaces */
    INIT_DELAYED_WORK(&bma222->work, bma222_work_func);

    err = bma222_input_init(bma222);
    if (err < 0) {
        goto error_1;
    }

    err = sysfs_create_group(&bma222->input->dev.kobj, &bma222_attribute_group);
    if (err < 0) {
        goto error_2;
    }

	bma222->bma222_device.minor = MISC_DYNAMIC_MINOR; 
	bma222->bma222_device.name = "accelerometer";
	bma222->bma222_device.fops = &bma222_fops;

	err = misc_register(&bma222->bma222_device);
	if (err) {
		pr_err("%s: misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

    return 0;

err_misc_register:
	sysfs_remove_group(&bma222->input->dev.kobj, &bma222_attribute_group);
error_2:
    bma222_input_fini(bma222);
error_1:
    kfree(bma222);
error_0:
    return err;
}

static int bma222_remove(struct i2c_client *client)
{
    struct bma222_data *bma222 = i2c_get_clientdata(client);

    bma222_set_enable(&client->dev, 0);

    sysfs_remove_group(&bma222->input->dev.kobj, &bma222_attribute_group);
    bma222_input_fini(bma222);
    kfree(bma222);

    return 0;
}

static int bma222_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct bma222_data *bma222 = i2c_get_clientdata(client);

    TRACE_FUNC();

    mutex_lock(&bma222->enable_mutex);

    if (bma222_get_enable(&client->dev)) {
        cancel_delayed_work_sync(&bma222->work);
        bma222_power_down(bma222);
    }
#if DEBUG
    bma222->suspend = 1;
#endif

    mutex_unlock(&bma222->enable_mutex);

    return 0;
}

static int bma222_resume(struct i2c_client *client)
{
    struct bma222_data *bma222 = i2c_get_clientdata(client);
    int delay = atomic_read(&bma222->delay);

    TRACE_FUNC();

    bma222_hw_init(bma222);
    bma222_set_delay(&client->dev, delay);

    mutex_lock(&bma222->enable_mutex);

    if (bma222_get_enable(&client->dev)) {
        bma222_power_up(bma222);
        schedule_delayed_work(&bma222->work, delay_to_jiffies(delay) + 1);
    }
#if DEBUG
    bma222->suspend = 0;
#endif

    mutex_unlock(&bma222->enable_mutex);

    return 0;
}

static const struct i2c_device_id bma222_id[] = {
    {BMA222_NAME, 0},
    {},
};

MODULE_DEVICE_TABLE(i2c, bma222_id);

struct i2c_driver bma222_driver ={
    .driver = {
        .name = "bma222",
        .owner = THIS_MODULE,
    },
    .probe = bma222_probe,
    .remove = bma222_remove,
    .suspend = bma222_suspend,
    .resume = bma222_resume,
    .id_table = bma222_id,
};

/*
 * Module init and exit
 */
static int __init bma222_init(void)
{
    return i2c_add_driver(&bma222_driver);
}
module_init(bma222_init);

static void __exit bma222_exit(void)
{
    i2c_del_driver(&bma222_driver);
}
module_exit(bma222_exit);

MODULE_DESCRIPTION("BMA222 accelerometer driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(BMA222_VERSION);
