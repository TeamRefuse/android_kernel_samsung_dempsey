/*
 * Driver for camera from SAMSUNG ELECTRONICS
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

// have to match count at seccamera.h
#ifndef __MEDIA_CAMSENSOR_TYPE_H__ 
#define __MEDIA_CAMSENSOR_TYPE_H__

//NAGSM_HQ_CAMERA_LEESUNGKOO_20101231 : to support two type front camera
enum CAMERA_SENSOR_ID
{
	CAMERA_SENSOR_ID_BACK_ORG = 0 ,
	CAMERA_SENSOR_ID_BACK_M5MO=CAMERA_SENSOR_ID_BACK_ORG,		
	CAMERA_SENSOR_ID_FRONT_ORG,
	CAMERA_SENSOR_ID_FRONT_SR130PC10=CAMERA_SENSOR_ID_FRONT_ORG,//NAGSM_HQ_CAMERA_LEESUNGKOO_20110223 : to support two type front camera	
	CAMERA_SENSOR_ID_FRONT_START,	
	CAMERA_SENSOR_ID_FRONT_S5K6AAFX=CAMERA_SENSOR_ID_FRONT_START,
	CAMERA_SENSOR_ID_FRONT_END
}; 
//NAGSM_HQ_CAMERA_LEESUNGKOO_20101231 : to support two type front camera
#endif
