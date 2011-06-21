/*
 ***************************************************************************************************
 *
 * (C) All rights reserved by ROBERT BOSCH GMBH
 *
 **************************************************************************************************/
/*  $Date: yyyy/mm/dd
 *  $Revision: 1.0 $
 *
 */

/**************************************************************************************************
* Copyright (C) 2007 Bosch Sensortec GmbH
*
* bma222.c
*
* Usage:        Sensor Driver for BMA222 Triaxial acceleration sensor
*
* Author:       yogananth.k@in.bosch.com
**************************************************************************************************/
/*************************************************************************************************/
/*  Disclaimer
*
* Common:
* Bosch Sensortec products are developed for the consumer goods industry. They may only be used
* within the parameters of the respective valid product data sheet.  Bosch Sensortec products are
* provided with the express understanding that there is no warranty of fitness for a particular purpose.
* They are not fit for use in life-sustaining, safety or security sensitive systems or any system or device
* that may lead to bodily harm or property damage if the system or device malfunctions. In addition,
* Bosch Sensortec products are not fit for use in products which interact with motor vehicle systems.
* The resale and/or use of products are at the purchaser’s own risk and his own responsibility. The
* examination of fitness for the intended use is the sole responsibility of the Purchaser.
*
* The purchaser shall indemnify Bosch Sensortec from all third party claims, including any claims for
* incidental, or consequential damages, arising from any product use not covered by the parameters of
* the respective valid product data sheet or not approved by Bosch Sensortec and reimburse Bosch
* Sensortec for all costs in connection with such claims.
*
* The purchaser must monitor the market for the purchased products, particularly with regard to
* product safety and inform Bosch Sensortec without delay of all security relevant incidents.
*
* Engineering Samples are marked with an asterisk (*) or (e). Samples may vary from the valid
* technical specifications of the product series. They are therefore not intended or fit for resale to third
* parties or for use in end products. Their sole purpose is internal client testing. The testing of an
* engineering sample may in no way replace the testing of a product series. Bosch Sensortec
* assumes no liability for the use of engineering samples. By accepting the engineering samples, the
* Purchaser agrees to indemnify Bosch Sensortec from all claims arising from the use of engineering
* samples.
*
* Special:
* This software module (hereinafter called "Software") and any information on application-sheets
* (hereinafter called "Information") is provided free of charge for the sole purpose to support your
* application work. The Software and Information is subject to the following terms and conditions:
*
* The Software is specifically designed for the exclusive use for Bosch Sensortec products by
* personnel who have special experience and training. Do not use this Software if you do not have the
* proper experience or training.
*
* This Software package is provided `` as is `` and without any expressed or implied warranties,
* including without limitation, the implied warranties of merchantability and fitness for a particular
* purpose.
*
* Bosch Sensortec and their representatives and agents deny any liability for the functional impairment
* of this Software in terms of fitness, performance and safety. Bosch Sensortec and their
* representatives and agents shall not be liable for any direct or indirect damages or injury, except as
* otherwise stipulated in mandatory applicable law.
*
* The Information provided is believed to be accurate and reliable. Bosch Sensortec assumes no
* responsibility for the consequences of use of such Information nor for any infringement of patents or
* other rights of third parties which may result from its use. No license is granted by implication or
* otherwise under any patent or patent rights of Bosch. Specifications mentioned in the Information are
* subject to change without notice.
*
* It is not allowed to deliver the source code of the Software to any third party without permission of
* Bosch Sensortec.
*/
/*************************************************************************************************/
/*! \file <File Name>
    \brief <Description about file> */
//#include "define.h"
#include "bma222.h"
/* user defined code to be added here ... */

//Example....
//#define YOUR_H_DEFINE         /**< <Description for YOUR_H_DEFINE> */
/* user defined code to be added here ... */

//Example...
//typedef enum {
//E_YOURDATA1, /**< <DOXY Comment for E_YOURDATA1> */
//E_YOURDATA2  /**< <DOXY Comment for E_YOURDATA2> */
//}te_YourData;
/* user defined code to be added here ... */

//Example.....
//char v_Variable_u8r /**\brief <Description for v_Variable_u8r>*/

bma222_t * p_bma222;
unsigned char bma222_bitwidth  =2;
/* user defined code to be added here ... */

//Example....
//char const C_ConstData /**\brief <Description for C_ConstData> */
/* user defined code to be added here ... */
//Example...
//void Local_1(void);       /**\brief <Description for Local_1 function> */
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_init(bma222_t *bma222)
{
   int comres= C_Zero_U8X ;
   unsigned char data;
   
   p_bma222 = bma222;                                                                             /* assign bma222 ptr */
   p_bma222->dev_addr = BMA222_I2C_ADDR;                                                          /* preset bma222 I2C_addr */
   comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_CHIP_ID__REG, &data, 1);     /* read Chip Id */
   
   p_bma222->chip_id = data ;                                          /* get bitslice */
   
   comres += p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ML_VERSION__REG, &data, 1); /* read Version reg */
   p_bma222->ml_version = BMA222_GET_BITSLICE(data, BMA222_ML_VERSION);                            /* get ML Version */
   p_bma222->al_version = BMA222_GET_BITSLICE(data, BMA222_AL_VERSION);                            /* get AL Version */
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_soft_reset(void)
{
   int comres = C_Zero_U8X ;
   unsigned char data = BMA222_EN_SOFT_RESET_VALUE ;
   if (p_bma222 == C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_WRITE_FUNC(p_bma222->dev_addr, BMA222_EN_SOFT_RESET__REG,&data ,C_One_U8X);
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_write_reg(unsigned char addr, unsigned char *data, unsigned char len)
{
   int comres = C_Zero_U8X ;
   if (p_bma222 == C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_WRITE_FUNC(p_bma222->dev_addr, addr, data, len);
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_read_reg(unsigned char addr, unsigned char *data, unsigned char len)
{
   int comres = C_Zero_U8X ;
   if (p_bma222 == C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, addr, data, len);
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
unsigned char bma222_set_mode(unsigned char Mode)
{
   int comres=C_Zero_U8X ;
   unsigned char data1;
   if (p_bma222 == C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      if (Mode < C_Three_U8X)
      {
         comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_EN_LOW_POWER__REG, &data1, C_One_U8X );
         switch (Mode)
         {
            case BMA222_MODE_NORMAL:
               data1  = BMA222_SET_BITSLICE(data1, BMA222_EN_LOW_POWER, C_Zero_U8X);
               data1  = BMA222_SET_BITSLICE(data1, BMA222_EN_SUSPEND, C_Zero_U8X);
               break;
            case BMA222_MODE_LOWPOWER:
               data1  = BMA222_SET_BITSLICE(data1, BMA222_EN_LOW_POWER, C_One_U8X);
               data1  = BMA222_SET_BITSLICE(data1, BMA222_EN_SUSPEND, C_Zero_U8X);
               break;
            case BMA222_MODE_SUSPEND:
               data1  = BMA222_SET_BITSLICE(data1, BMA222_EN_LOW_POWER, C_Zero_U8X);
               data1  = BMA222_SET_BITSLICE(data1, BMA222_EN_SUSPEND, C_One_U8X);
               break;
            default:
               break;
         }
         comres += p_bma222->BMA222_BUS_WRITE_FUNC(p_bma222->dev_addr, BMA222_EN_LOW_POWER__REG, &data1, C_One_U8X);
         p_bma222->mode = Mode;
      }
      else
      {
         comres = E_OUT_OF_RANGE ;
      }
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_get_mode(unsigned char * Mode )
{
   int comres= C_Zero_U8X;
   if (p_bma222 == C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_EN_LOW_POWER__REG, Mode, C_One_U8X );
      *Mode  = (*Mode) >> C_Six_U8X;
      p_bma222->mode = *Mode;
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_set_range(unsigned char Range)
{
   int comres=C_Zero_U8X ;
   unsigned char data1;
   if (p_bma222 == C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      if (Range < C_Four_U8X)
      {
         comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_RANGE_SEL_REG, &data1, C_One_U8X );
         switch (Range)
         {
            case C_Zero_U8X:
               data1  = BMA222_SET_BITSLICE(data1, BMA222_RANGE_SEL, C_Three_U8X);
               break;
            case C_One_U8X:
               data1  = BMA222_SET_BITSLICE(data1, BMA222_RANGE_SEL, C_Five_U8X);
               break;
            case C_Two_U8X:
               data1  = BMA222_SET_BITSLICE(data1, BMA222_RANGE_SEL, C_Eight_U8X);
               break;
            case C_Three_U8X:
               data1  = BMA222_SET_BITSLICE(data1, BMA222_RANGE_SEL, C_Twelve_U8X);
               break;
            default:
               break;
         }
         comres += p_bma222->BMA222_BUS_WRITE_FUNC(p_bma222->dev_addr, BMA222_RANGE_SEL_REG, &data1, C_One_U8X);
      }
      else
      {
         comres = E_OUT_OF_RANGE ;
      }
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_get_range(unsigned char * Range )
{
   int comres = C_Zero_U8X ;
   unsigned char data;
   if (p_bma222 == C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_RANGE_SEL__REG, &data, C_One_U8X );
      data = BMA222_GET_BITSLICE(data, BMA222_RANGE_SEL);
      *Range = data;
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_set_bandwidth(unsigned char BW)
{
   int comres = C_Zero_U8X ;
   unsigned char data;
   int Bandwidth ;
   if (p_bma222 == C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      if (BW < C_Eight_U8X)
      {
         switch (BW)
         {
            case C_Zero_U8X:
               Bandwidth = BMA222_BW_7_81HZ;
               
               /*  7.81 Hz      64000 uS   */
               break;
            case C_One_U8X:
               Bandwidth = BMA222_BW_15_63HZ;
               
               /*  15.63 Hz     32000 uS   */
               break;
            case C_Two_U8X:
               Bandwidth = BMA222_BW_31_25HZ;
               
               /*  31.25 Hz     16000 uS   */
               break;
            case C_Three_U8X:
               Bandwidth = BMA222_BW_62_50HZ;
               
               /*  62.50 Hz     8000 uS   */
               break;
            case C_Four_U8X:
               Bandwidth = BMA222_BW_125HZ;
               
               /*  125 Hz       4000 uS   */
               break;
            case C_Five_U8X:
               Bandwidth = BMA222_BW_250HZ;
               
               /*  250 Hz       2000 uS   */
               break;
            case C_Six_U8X:
               Bandwidth = BMA222_BW_500HZ;
               
               /*  500 Hz       1000 uS   */
               break;
            case C_Seven_U8X:
               Bandwidth = BMA222_BW_1000HZ;
               
               /*  1000 Hz      500 uS   */
               break;
            default:
               break;
         }
         comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_BANDWIDTH__REG, &data, C_One_U8X );
         data = BMA222_SET_BITSLICE(data, BMA222_BANDWIDTH, Bandwidth );
         comres += p_bma222->BMA222_BUS_WRITE_FUNC(p_bma222->dev_addr, BMA222_BANDWIDTH__REG, &data, C_One_U8X );
      }
      else
      {
         comres = E_OUT_OF_RANGE ;
      }
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_get_bandwidth(unsigned char * BW)
{
   int comres = C_Zero_U8X ;
   unsigned char data;
   if (p_bma222 == C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_BANDWIDTH__REG, &data, C_One_U8X );
      data = BMA222_GET_BITSLICE(data, BMA222_BANDWIDTH);
      if (data <= C_Eight_U8X)
      {
         *BW = C_Zero_U8X;
      }
      else
      {
         if (data >= 0x0F)
         {
            *BW = C_Seven_U8X;
         }
         else
         {
            *BW = data - C_Eight_U8X;
         }
      }
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_read_accel_xyz(bma222acc_t * acc)
{
   int comres;
   unsigned char data[6];
   if (p_bma222==C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      switch (bma222_bitwidth)
      {
         case 0:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_X12_LSB__REG, data, 6);
            acc->x = BMA222_GET_BITSLICE(data[0],BMA222_ACC_X12_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_X_MSB)<<(BMA222_ACC_X12_LSB__LEN));
            acc->x = acc->x << (sizeof(short)*8-(BMA222_ACC_X12_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            acc->x = acc->x >> (sizeof(short)*8-(BMA222_ACC_X12_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            
            acc->y = BMA222_GET_BITSLICE(data[2],BMA222_ACC_Y12_LSB)| (BMA222_GET_BITSLICE(data[3],BMA222_ACC_Y_MSB)<<(BMA222_ACC_Y12_LSB__LEN ));
            acc->y = acc->y << (sizeof(short)*8-(BMA222_ACC_Y12_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            acc->y = acc->y >> (sizeof(short)*8-(BMA222_ACC_Y12_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            
            acc->z = BMA222_GET_BITSLICE(data[4],BMA222_ACC_Z12_LSB)| (BMA222_GET_BITSLICE(data[5],BMA222_ACC_Z_MSB)<<(BMA222_ACC_Z12_LSB__LEN));
            acc->z = acc->z << (sizeof(short)*8-(BMA222_ACC_Z12_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            acc->z = acc->z >> (sizeof(short)*8-(BMA222_ACC_Z12_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            break;
         case 1:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_X10_LSB__REG, data, 6);
            acc->x = BMA222_GET_BITSLICE(data[0],BMA222_ACC_X10_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_X_MSB)<<(BMA222_ACC_X10_LSB__LEN));
            acc->x = acc->x << (sizeof(short)*8-(BMA222_ACC_X10_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            acc->x = acc->x >> (sizeof(short)*8-(BMA222_ACC_X10_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            
            acc->y = BMA222_GET_BITSLICE(data[2],BMA222_ACC_Y10_LSB)| (BMA222_GET_BITSLICE(data[3],BMA222_ACC_Y_MSB)<<(BMA222_ACC_Y10_LSB__LEN ));
            acc->y = acc->y << (sizeof(short)*8-(BMA222_ACC_Y10_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            acc->y = acc->y >> (sizeof(short)*8-(BMA222_ACC_Y10_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            
            acc->z = BMA222_GET_BITSLICE(data[4],BMA222_ACC_Z10_LSB)| (BMA222_GET_BITSLICE(data[5],BMA222_ACC_Z_MSB)<<(BMA222_ACC_Z10_LSB__LEN));
            acc->z = acc->z << (sizeof(short)*8-(BMA222_ACC_Z10_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            acc->z = acc->z >> (sizeof(short)*8-(BMA222_ACC_Z10_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            break;
         case 2:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_X8_LSB__REG, data, 6);
            acc->x = BMA222_GET_BITSLICE(data[0],BMA222_ACC_X8_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_X_MSB)<<(BMA222_ACC_X8_LSB__LEN));
            acc->x = acc->x << (sizeof(short)*8-(BMA222_ACC_X8_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            acc->x = acc->x >> (sizeof(short)*8-(BMA222_ACC_X8_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            
            acc->y = BMA222_GET_BITSLICE(data[2],BMA222_ACC_Y8_LSB)| (BMA222_GET_BITSLICE(data[3],BMA222_ACC_Y_MSB)<<(BMA222_ACC_Y8_LSB__LEN ));
            acc->y = acc->y << (sizeof(short)*8-(BMA222_ACC_Y8_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            acc->y = acc->y >> (sizeof(short)*8-(BMA222_ACC_Y8_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            
            acc->z = BMA222_GET_BITSLICE(data[4],BMA222_ACC_Z8_LSB)| (BMA222_GET_BITSLICE(data[5],BMA222_ACC_Z_MSB)<<(BMA222_ACC_Z8_LSB__LEN));
            acc->z = acc->z << (sizeof(short)*8-(BMA222_ACC_Z8_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            acc->z = acc->z >> (sizeof(short)*8-(BMA222_ACC_Z8_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            break;
         case 3:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_X14_LSB__REG, data, 6);
            acc->x = BMA222_GET_BITSLICE(data[0],BMA222_ACC_X14_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_X_MSB)<<(BMA222_ACC_X14_LSB__LEN));
            acc->x = acc->x << (sizeof(short)*8-(BMA222_ACC_X14_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            acc->x = acc->x >> (sizeof(short)*8-(BMA222_ACC_X14_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            
            acc->y = BMA222_GET_BITSLICE(data[2],BMA222_ACC_Y14_LSB)| (BMA222_GET_BITSLICE(data[3],BMA222_ACC_Y_MSB)<<(BMA222_ACC_Y14_LSB__LEN ));
            acc->y = acc->y << (sizeof(short)*8-(BMA222_ACC_Y14_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            acc->y = acc->y >> (sizeof(short)*8-(BMA222_ACC_Y14_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            
            acc->z = BMA222_GET_BITSLICE(data[4],BMA222_ACC_Z14_LSB)| (BMA222_GET_BITSLICE(data[5],BMA222_ACC_Z_MSB)<<(BMA222_ACC_Z14_LSB__LEN));
            acc->z = acc->z << (sizeof(short)*8-(BMA222_ACC_Z14_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            acc->z = acc->z >> (sizeof(short)*8-(BMA222_ACC_Z14_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            break;
         default:
            break;
      }
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_read_accel_x(short *a_x)
{
   int comres;
   unsigned char data[2];
   if (p_bma222==C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      switch (bma222_bitwidth)
      {
         case 0:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_X12_LSB__REG, data, 2);
            *a_x = BMA222_GET_BITSLICE(data[0],BMA222_ACC_X12_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_X_MSB)<<(BMA222_ACC_X12_LSB__LEN));
            *a_x = *a_x << (sizeof(short)*8-(BMA222_ACC_X12_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            *a_x = *a_x >> (sizeof(short)*8-(BMA222_ACC_X12_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            break;
         case 1:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_X10_LSB__REG, data, 2);
            *a_x = BMA222_GET_BITSLICE(data[0],BMA222_ACC_X10_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_X_MSB)<<(BMA222_ACC_X10_LSB__LEN));
            *a_x = *a_x << (sizeof(short)*8-(BMA222_ACC_X10_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            *a_x = *a_x >> (sizeof(short)*8-(BMA222_ACC_X10_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            break;
         case 2:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_X8_LSB__REG, data, 2);
            *a_x = BMA222_GET_BITSLICE(data[0],BMA222_ACC_X8_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_X_MSB)<<(BMA222_ACC_X8_LSB__LEN));
            *a_x = *a_x << (sizeof(short)*8-(BMA222_ACC_X8_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            *a_x = *a_x >> (sizeof(short)*8-(BMA222_ACC_X8_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            break;
         case 3:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_X14_LSB__REG, data, 2);
            *a_x = BMA222_GET_BITSLICE(data[0],BMA222_ACC_X14_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_X_MSB)<<(BMA222_ACC_X14_LSB__LEN));
            *a_x = *a_x << (sizeof(short)*8-(BMA222_ACC_X14_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            *a_x = *a_x >> (sizeof(short)*8-(BMA222_ACC_X14_LSB__LEN + BMA222_ACC_X_MSB__LEN));
            break;
         default:
            break;
      }
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_read_accel_y(short *a_y)
{
   int comres;
   unsigned char data[2];
   if (p_bma222==C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      switch (bma222_bitwidth)
      {
         case 0:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_Y12_LSB__REG, data, 2);
            *a_y = BMA222_GET_BITSLICE(data[0],BMA222_ACC_Y12_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_Y_MSB)<<(BMA222_ACC_Y12_LSB__LEN ));
            *a_y = *a_y << (sizeof(short)*8-(BMA222_ACC_Y12_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            *a_y = *a_y >> (sizeof(short)*8-(BMA222_ACC_Y12_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            break;
         case 1:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_Y10_LSB__REG, data, 2);
            *a_y = BMA222_GET_BITSLICE(data[0],BMA222_ACC_Y10_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_Y_MSB)<<(BMA222_ACC_Y10_LSB__LEN ));
            *a_y = *a_y << (sizeof(short)*8-(BMA222_ACC_Y10_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            *a_y = *a_y >> (sizeof(short)*8-(BMA222_ACC_Y10_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            break;
         case 2:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_Y8_LSB__REG, data, 2);
            *a_y = BMA222_GET_BITSLICE(data[0],BMA222_ACC_Y8_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_Y_MSB)<<(BMA222_ACC_Y8_LSB__LEN ));
            *a_y = *a_y << (sizeof(short)*8-(BMA222_ACC_Y8_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            *a_y = *a_y >> (sizeof(short)*8-(BMA222_ACC_Y8_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            break;
         case 3:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_Y14_LSB__REG, data, 2);
            *a_y = BMA222_GET_BITSLICE(data[0],BMA222_ACC_Y14_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_Y_MSB)<<(BMA222_ACC_Y14_LSB__LEN ));
            *a_y = *a_y << (sizeof(short)*8-(BMA222_ACC_Y14_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            *a_y = *a_y >> (sizeof(short)*8-(BMA222_ACC_Y14_LSB__LEN + BMA222_ACC_Y_MSB__LEN));
            break;
         default:
            break;
      }
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_read_accel_z(short *a_z)
{
   int comres;
   unsigned char data[2];
   if (p_bma222==C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      switch (bma222_bitwidth)
      {
         case 0:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_Z12_LSB__REG, data, 2);
            *a_z = BMA222_GET_BITSLICE(data[0],BMA222_ACC_Z12_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_Z_MSB)<<(BMA222_ACC_Z12_LSB__LEN));
            *a_z = *a_z << (sizeof(short)*8-(BMA222_ACC_Z12_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            *a_z = *a_z >> (sizeof(short)*8-(BMA222_ACC_Z12_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            break;
         case 1:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_Z10_LSB__REG, data, 2);
            *a_z = BMA222_GET_BITSLICE(data[0],BMA222_ACC_Z10_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_Z_MSB)<<(BMA222_ACC_Z10_LSB__LEN));
            *a_z = *a_z << (sizeof(short)*8-(BMA222_ACC_Z10_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            *a_z = *a_z >> (sizeof(short)*8-(BMA222_ACC_Z10_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            break;
         case 2:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_Z8_LSB__REG, data, 2);
            *a_z = BMA222_GET_BITSLICE(data[0],BMA222_ACC_Z8_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_Z_MSB)<<(BMA222_ACC_Z8_LSB__LEN));
            *a_z = *a_z << (sizeof(short)*8-(BMA222_ACC_Z8_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            *a_z = *a_z >> (sizeof(short)*8-(BMA222_ACC_Z8_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            break;
         case 3:
            comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_ACC_Z14_LSB__REG, data, 2);
            *a_z = BMA222_GET_BITSLICE(data[0],BMA222_ACC_Z14_LSB)| (BMA222_GET_BITSLICE(data[1],BMA222_ACC_Z_MSB)<<(BMA222_ACC_Z14_LSB__LEN));
            *a_z = *a_z << (sizeof(short)*8-(BMA222_ACC_Z14_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            *a_z = *a_z >> (sizeof(short)*8-(BMA222_ACC_Z14_LSB__LEN + BMA222_ACC_Z_MSB__LEN));
            break;
         default:
            break;
      }
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_update_image(void)
{
   int comres;
   unsigned char data=0;
   if (p_bma222==C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_UPDATE_IMAGE__REG, &data, C_One_U8X);
      data = BMA222_SET_BITSLICE(data, BMA222_UPDATE_IMAGE, C_One_U8X);
      comres = p_bma222->BMA222_BUS_WRITE_FUNC(p_bma222->dev_addr, BMA222_UPDATE_IMAGE__REG, &data,C_One_U8X);
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_reset_interrupt(void)
{
   int comres=C_Zero_U8X;
   unsigned char data;
   if (p_bma222==C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_INT_RESET_LATCHED__REG, &data, C_One_U8X );
      data = BMA222_SET_BITSLICE(data, BMA222_INT_RESET_LATCHED, C_One_U8X );
      comres = p_bma222->BMA222_BUS_WRITE_FUNC(p_bma222->dev_addr, BMA222_INT_RESET_LATCHED__REG, &data, C_One_U8X);
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_get_interrupt_status(unsigned char * ist)
{
   int comres = C_Zero_U8X;
   if (p_bma222 == C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_STATUS1_REG, ist, C_Four_U8X);
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_set_Int_Mode(unsigned char Mode )
{
   int comres=C_Zero_U8X;
   unsigned char data;
   if (p_bma222==C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_INT_MODE_SEL__REG, &data, C_One_U8X );
      data = BMA222_SET_BITSLICE(data, BMA222_INT_MODE_SEL, Mode );
      comres = p_bma222->BMA222_BUS_WRITE_FUNC(p_bma222->dev_addr, BMA222_INT_MODE_SEL__REG, &data, C_One_U8X);
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_get_Int_Mode(unsigned char * Mode )
{
   int comres= C_Zero_U8X;
   unsigned char data1;
   if (p_bma222 == C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_INT_MODE_SEL__REG, &data1, C_One_U8X );
      data1  = BMA222_GET_BITSLICE(data1, BMA222_INT_MODE_SEL);
      *Mode = data1 ;
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_set_Int_Enable(unsigned char InterruptType , unsigned char value )
{
   int comres=C_Zero_U8X;
   unsigned char data1,data2;
   if (p_bma222==C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_INT_ENABLE1_REG, &data1, C_One_U8X );
      comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_INT_ENABLE2_REG, &data2, C_One_U8X );
      
      value = value & C_One_U8X;
      switch (InterruptType)
      {
         case C_Zero_U8X:
            /* Low G Interrupt  */
            data2 = BMA222_SET_BITSLICE(data2, BMA222_EN_LOWG_INT, value );
            break;
         case C_One_U8X:
            /* High G X Interrupt */
            data2 = BMA222_SET_BITSLICE(data2, BMA222_EN_HIGHG_X_INT, value );
            break;
         case C_Two_U8X:
            /* High G Y Interrupt */
            data2 = BMA222_SET_BITSLICE(data2, BMA222_EN_HIGHG_Y_INT, value );
            break;
         case C_Three_U8X:
            /* High G Z Interrupt */
            data2 = BMA222_SET_BITSLICE(data2, BMA222_EN_HIGHG_Z_INT, value );
            break;
         case C_Four_U8X:
            /* New Data Interrupt  */
            data2 = BMA222_SET_BITSLICE(data2, BMA222_EN_NEW_DATA_INT, value );
            break;
         case C_Five_U8X:
            /* Slope X Interrupt */
            data1 = BMA222_SET_BITSLICE(data1, BMA222_EN_SLOPE_X_INT, value );
            break;
         case C_Six_U8X:
            /* Slope Y Interrupt */
            data1 = BMA222_SET_BITSLICE(data1, BMA222_EN_SLOPE_Y_INT, value );
            break;
         case C_Seven_U8X:
            /* Slope Z Interrupt */
            data1 = BMA222_SET_BITSLICE(data1, BMA222_EN_SLOPE_Z_INT, value );
            break;
         case C_Eight_U8X:
            /* Single Tap Interrupt */
            data1 = BMA222_SET_BITSLICE(data1, BMA222_EN_SINGLE_TAP_INT, value );
            break;
         case C_Nine_U8X:
            /* Double Tap Interrupt */
            data1 = BMA222_SET_BITSLICE(data1, BMA222_EN_DOUBLE_TAP_INT, value );
            break;
         case C_Ten_U8X:
            /* Orient Interrupt  */
            data1 = BMA222_SET_BITSLICE(data1, BMA222_EN_ORIENT_INT, value );
            break;
         case C_Eleven_U8X:
            /* Flat Interrupt */
            data1 = BMA222_SET_BITSLICE(data1, BMA222_EN_FLAT_INT, value );
            break;
         default:
            break;
      }
      comres = p_bma222->BMA222_BUS_WRITE_FUNC(p_bma222->dev_addr, BMA222_INT_ENABLE1_REG, &data1, C_One_U8X );
      comres = p_bma222->BMA222_BUS_WRITE_FUNC(p_bma222->dev_addr, BMA222_INT_ENABLE2_REG, &data2, C_One_U8X );
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_set_nvmprog_mode(unsigned char prgmode)
{
   unsigned char data;
         int comres;
   if (p_bma222==C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR ;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_UNLOCK_EE_PROG_MODE__REG,&data, C_One_U8X);
      data = BMA222_SET_BITSLICE(data, BMA222_UNLOCK_EE_PROG_MODE, prgmode);
      comres = p_bma222->BMA222_BUS_WRITE_FUNC(p_bma222->dev_addr, BMA222_UNLOCK_EE_PROG_MODE__REG, &data, C_One_U8X);
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_set_nvprog_trig(unsigned char trig)
{
   unsigned char data;
         int comres;
   if (p_bma222==C_Zero_U8X)
   {
      comres = E_SMB_NULL_PTR ;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_START_EE_PROG_TRIG__REG,&data, C_One_U8X);
      data = BMA222_SET_BITSLICE(data, BMA222_START_EE_PROG_TRIG, trig);
      comres = p_bma222->BMA222_BUS_WRITE_FUNC(p_bma222->dev_addr, BMA222_START_EE_PROG_TRIG__REG, &data, C_One_U8X);
   }
   return comres;
}
/* Compiler Switch if applicable
#ifdef

#endif
*/
/*******************************************************************************
 * Description: *//**\brief
 *
 *
 *
 *
 *  \param
 *
 *
 *
 *  \return
 *
 *
 ******************************************************************************/
/* Scheduling:
 *
 *
 *
 * Usage guide:
 *
 *
 * Remarks:
 *
 ******************************************************************************/
int bma222_get_nvmprog_ready(unsigned char *ready)
{
   int comres;
   if (p_bma222==C_Zero_U8X)
   {
      comres =E_SMB_NULL_PTR ;
   }
   else
   {
      comres = p_bma222->BMA222_BUS_READ_FUNC(p_bma222->dev_addr, BMA222_EE_PROG_READY__REG,ready, C_One_U8X);
      *ready = BMA222_GET_BITSLICE(*ready, BMA222_EE_PROG_READY);
   }
   return comres;
}
