#ifndef __BMA222_ACC_HEADER__
#define __BMA222_ACC_HEADER__

#include "bma222_i2c.h"
#include "bma222.h"

#define ACC_DEV_NAME "accelerometer"
#define ACC_DEV_MAJOR 241

#define BMA222_MAJOR 	100

/* bma222 ioctl command label */
#define IOCTL_BMA222_GET_ACC_VALUE		0
#define DCM_IOC_MAGIC			's'
#define IOC_SET_ACCELEROMETER	_IO (DCM_IOC_MAGIC, 0x64)


#define bma222_POWER_OFF               0
#define bma222_POWER_ON                1

#endif
