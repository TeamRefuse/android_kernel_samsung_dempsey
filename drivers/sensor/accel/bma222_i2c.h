#ifndef __BMA222_I2C_HEADER__
#define __BMA222_I2C_HEADER__

char  i2c_acc_bma222_read (u8, u8 *, unsigned int);
char  i2c_acc_bma222_write(u8 reg, u8 *val);

int  i2c_acc_bma222_init(void);
void i2c_acc_bma222_exit(void);

#endif
