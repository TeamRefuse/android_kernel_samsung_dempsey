#define SR030pc30_COMPLETE
//#undef SR030pc30_COMPLETE
/*
 * Driver for SR030pc30 (VGA camera) from Samsung Electronics
 * 
 * 1/4" 2.0Mp CMOS Image Sensor SoC with an Embedded Image Processor
 *
 * Copyright (C) 2009, Jinsung Yang <jsgood.yang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __SR030pc30_H__
#define __SR030pc30_H__

struct SR030pc30_reg {
	unsigned int addr;
	unsigned int val;
};

struct SR030pc30_regset_type {
	unsigned int *regset;
	int len;
};

/*
 * Macro
 */
#define REGSET_LENGTH(x)	(sizeof(x)/sizeof(SR030pc30_reg))

/*
 * User defined commands
 */
/* S/W defined features for tune */
#define REG_DELAY	0xFF00	/* in ms */
#define REG_CMD		0xFFFF	/* Followed by command */

/* Following order should not be changed */
enum image_size_SR030pc30 {
	/* This SoC supports upto UXGA (1600*1200) */
#if 0
	QQVGA,	/* 160*120*/
	QCIF,	/* 176*144 */
	QVGA,	/* 320*240 */
	CIF,	/* 352*288 */
	VGA,	/* 640*480 */
#endif
	SVGA,	/* 800*600 */
#if 0
	HD720P,	/* 1280*720 */
	SXGA,	/* 1280*1024 */
	UXGA,	/* 1600*1200 */
#endif
};

/*
 * Following values describe controls of camera
 * in user aspect and must be match with index of SR030pc30_regset[]
 * These values indicates each controls and should be used
 * to control each control
 */
enum SR030pc30_control {
	SR030pc30_INIT,
	SR030pc30_EV,
	SR030pc30_AWB,
	SR030pc30_MWB,
	SR030pc30_EFFECT,
	SR030pc30_CONTRAST,
	SR030pc30_SATURATION,
	SR030pc30_SHARPNESS,
};

#define SR030pc30_REGSET(x)	{	\
	.regset = x,			\
	.len = sizeof(x)/sizeof(SR030pc30_reg),}

/*
 * VGA Self shot init setting
 */
 
static unsigned short SR030pc30_init_reg[] = {			 
0x0300,
0x01f1,
0x01f3,
0x01f1,

0x0320,
0x100c,
0x0322,
0x107b,

//Page0
0x0300,
0x1000,
0x11b1, //On Var Frame - None : 90, X Flip : 91, Y Flip :
0x1224,//PCLK Inversion
0x2000,
0x2106,
0x2200,
0x2306,
0x2401,
0x25e0,
0x2602,
0x2780,
0x4001, //Hblank 336
0x4150,

0x4201, //Vblank 300
0x432c,

//BLC
0x803e,
0x8196,
0x8290,
0x8300,
0x842c,

0x900f, //BLC_TIME_TH_ON
0x910f, //BLC_TIME_TH_OFF 
0x9278, //BLC_AG_TH_ON
0x9370, //BLC_AG_TH_OFF
0x9488,
0x9580,

0x9820,
0xa040,
0xa241,
0xa440,
0xa641,
0xa844,
0xaa43,
0xac44,
0xae43,

//Page2
0x0302,
0x1000,
0x1300,
0x181C,
0x1900,
0x1a00,
0x1b08,
0x1C00,
0x1D00,
0x2033,
0x21aa,
0x22a6,
0x23b0,
0x3199,
0x3200,
0x3300,
0x343C,
0x5021,
0x5430,
0x56fe,
0x6278,
0x639E,
0x6478,
0x659E,
0x727A,
0x739A,
0x747A,
0x759A,
0x8209,
0x8409,
0x8609,
0xa003,
0xa81D,
0xaa49,
0xb98A,
0xbb8A,
0xbc04,
0xbd10,
0xbe04,
0xbf10,

//Page10
0x0310,//
0x1001,//ISPCTL1
0x1230,//Y offet, dy offseet enable
0x4080,
0x4102,//
0x5078,//

0x601f,
0x6182,
0x6282,
0x63f0,
0x6480,

//Page11
0x0311,
0x1099,
0x110e,
0x2129,
0X5005,
0x600f,
0x6243,
0x6363,
0x7408,
0x7508,

//Page12
0x0312,
0x4023,
0x413b,
0x5005,
0x701d,
0x7405,
0x7504,
0x7720,
0x7810,
0x9134,
0xb0c9,
0xd0b1,

//Page13
0x0313,
0x103b,
0x1103,
0x1200,
0x1302,
0x1418,

0x2002,
0x2101,
0x2324,
0x2406,
0x254a,
0x2800,
0x2978,
0x30ff,
0x800D,
0x8113,

0x835d,

0x9003,
0x9102,
0x933d,
0x9403,
0x958f,

//Page14
0x0314,
0x1001,
0x2080,
0x2178,
0x2253,
0x2340,
0x243e,

//Page 15 CmC
0x0315,
0x1003,

0x143c,
0x162c,
0x172f,

0x30cb,
0x3161,
0x3216,
0x3323,
0x34ce,
0x352b,
0x3601,
0x3734,
0x3875,

0x4087,
0x4118,
0x4291,
0x4394,
0x449f,
0x4533,
0x4600,
0x4794,
0x4814,

0x0316,
0x1001,
0x3000,
0x3109,
0x321b,
0x3335,
0x345d,
0x357a,
0x3693,
0x37a7,
0x38b8,
0x39c6,
0x3ad2,
0x3be4,
0x3cf1,
0x3df9,
0x3eff,


//Page20 AE
0x0320,
0x100c,
0x1104,

0x2001,
0x283f,
0x29a3,
0x2af0,
0x2bf4,

0x30f8,

0x60d5,

0x6834,
0x696e,
0x6A28,
0x6Bc8,

0x7034,//Y Target

0x7812,//Yth 1
0x7911,//Yth 2
0x7A23,

0x8300,//ExpTime 30fps
0x84c3,
0x8550,

0x8600,//ExpMin
0x87fa,

0x8802, //EXP Max 8.00 fps 
0x89dc, 
0x8a6c, 

0x8b3a,//Exp100
0x8c98,

0x8d30,//Exp120
0x8ed4,

0x8f04,
0x9093,

0x9102,
0x92d2,
0x93a8,

0x988C,
0x9923,

0x9c05, //EXP Limit 1000.00 fps 
0x9ddc, 
0x9e00,//4s unit
0x9ffa,

0xb014,
0xb114,
0xb2a0, //d0
0xb314,
0xb41c,
0xb548,
0xb632,
0xb72b,
0xb827,
0xb925,
0xba23,
0xbb22,
0xbc22,
0xbd21,

0xc014,

0xc870,
0xc980,

//Page 22 AWB
0x0322,
0x10e2,
0x1126,
0x2104,

0x3080,
0x3180,
0x3811,
0x3933,
0x40f0,
0x4133,
0x4233,
0x43f3,
0x4455,
0x4544,
0x4602,

0x50d0,
0x51a0,
0x52aa,

0x8045,
0x8120,
0x8248,

0x8354,//54
0x8422,//22 29 27
0x8552,//54 53 50
0x8620,//22

0x874c,
0x8834,
0x8930,
0x8a22,

0x8b00,
0x8d22,
0x8e71,

0x8f5e, //63
0x9059, //60
0x9156, //5c
0x9251, //59
0x934b, //55
0x9445, //50
0x953a, //48
0x962e, //3e
0x9727, //37
0x9821, //30
0x991e, //29
0x9a19, //26
0x9b09, //09
0xb47b,

0x0322,
0x10fb,

0x0320,
0x108c,

0x0300,
0x11b1, //3 Bad frame skip, Fixed disable 
0x01f0,
};

//==========================================================
//	CAMERA INITIAL for VT Preview 7 Fixed Frame (VGA SETTING)
//==========================================================

static unsigned short SR030pc30_init_vt_reg[] = {			
0x0300, //this is for VT-call
0x01f1,
0x01f3,
0x01f1,

0x0320, //page 3
0x100c, //ae off
0x0322, //page 4
0x107b, //awb off

//Page0
0x0300,
0x1000,
0x1195, //On Fix Frame - None : 94, X Flip : 95, Y Flip : 96, XY : 97
0x1204,
0x2000,
0x2106,
0x2200,
0x2306,
0x2401,
0x25e0,
0x2602,
0x2780,
0x4001, //Hblank 336
0x4150, //

0x4200, //Vsync 20
0x4314, //

//BLC
0x803e,
0x8196,
0x8290,
0x8300,  
0x842c,  

0x900e,
0x910f,
0x923a,
0x9330,
0x9488,
0x9580,

0x9820,
0xa040,
0xa241,
0xa440,
0xa641,
0xa844,
0xaa43,
0xac44,
0xae43,

//Page2
0x0302, //
0x1000, //
0x1300, //
0x181C, //
0x1900, //
0x1a00, //
0x1b08, //
0x1C00, //
0x1D00, //
0x2033, //
0x21aa, //
0x22a6, //
0x23b0, //
0x3199, //
0x3200, //
0x3300, //
0x343C, //
0x5021, //
0x5430, //
0x56fe, //
0x6278, //
0x639E, //
0x6478, //
0x659E, //
0x727A, //
0x739A, //
0x747A, //
0x759A, //
0x8209, // 
0x8409, // 
0x8609, // 
0xa003, //
0xa81D, //
0xaa49, //
0xb98A, // 
0xbb8A, // 
0xbc04, // 
0xbd10, // 
0xbe04, // 
0xbf10, //

//Page10
0x0310,//
0x1001,//ISPCTL1 0x03
0x1230,//Y offet, dy offseet enable
0x4080, //8e
0x4118,
0x5078,

0x601f,
0x6180,//CB//82
0x6280,//CR//83
0x63f0,
0x6480,

//Page 11
0x0311,
0x1099,
0x110e,
0x2129, 
0x5005,
0x600f,
0x6243,
0x6363,
0x7408,
0x7508,

//Page12
0x0312,
0x4023,
0x413b,
0x5005,
0x701d,
0x7405,
0x7504,
0x7720,
0x7810,
0x9134,
0xb0c9,
0xd0b1,

//Page13
0x0313,
0x103b,
0x1103,
0x1200,
0x1302,
0x1418,

0x2002,
0x2101,
0x2324,
0x2406,
0x254a,
0x2800,
0x2978,
0x30ff,
0x800D,
0x8113,

0x835d,

0x9003,
0x9102,
0x933d,
0x9403,
0x958f,

//Page14
0x0314,
0x1001,
0x2080,
0x2178,
0x2253,
0x2340,
0x2442,//3e

//Page15 CMC
0x0315,
0x1003,

0x143c,
0x162c,
0x172f,

0x30cb,
0x3161,
0x3216,
0x3323,
0x34ce,
0x352b,
0x3601,
0x3734,
0x3875,

0x4087,
0x4118,
0x4291,
0x4394,
0x449f,
0x4533,
0x4600,
0x4794,
0x4814,

0x0316,
0x1001,
0x3000,
0x3114,
0x3223,
0x333b,
0x345d,
0x3579,
0x368e,
0x379f,
0x38af,
0x39bd,
0x3aca,
0x3bdd,
0x3cec,
0x3df7,
0x3eff,


//Page 20 AE
0x0320, //
0x100c, //
0x1104, //

0x2001,  //AE weight
0x283f,  //AE 4Shared speed up Enable
0x29a3,
0x2a93,  // Fixed

0x2bf5,  // 2/120 Anti banding
0x30f8,	// 0xf8->0x78 2/120 Anti banding

0x60c0, //	weight  80->6a

0x683c,
0x6964,
0x6A28,
0x6Bc8,

0x7042, //Y LVL

0x7812, //yth1
0x7916, //yth2
0x7A23, //yth3

0x8300,	//30fps
0x84c3,
0x8550,

0x8600,	// Exp min
0x87fa,

0x8802,	// Exp Max 7fps
0x89dc,
0x8a6c,

0x8b3a,	//Exp100Hz
0x8c98,

0x8d30,  //Exp120Hz
0x8ed4,

0x8f04, //
0x9093, //

0x9103, //Fixed 7
0x923b, //
0x9349, //

0x988C, //outdoor th1
0x9923, //outdoor th2

0x9c06, //4s_limit
0x9dd6, //
0x9e00, //4s_unit
0x9ffa, //

0xb014, //
0xb114, // 08->14
0xb2b0, // a0->e0
0xb314, //
0xb414, //
0xb538, //
0xb626, //
0xb720, //
0xb81d, //
0xb91b, //
0xba1a, //
0xbb19, //
0xbc19, //
0xbd18, //

0xc014, //skygain

0xc8b0, //
0xc980, //

//Page22 AWB
0x0322, //
0x10e2,
0x1126, // AD CMC off
0x2140, //

0x3080, //
0x3180, //
0x3811, //
0x3933,
0x40f0, //
0x4133,
0x4233,
0x43f3, //
0x4455,
0x4544,
0x4602, //

0x8040, //
0x8120, //
0x8243, //

0x8366, // RMAX Default : 50 
0x841f, // RMIN Default : 20
0x8561, // BMAX Default : 50
0x8620, // BMIN Default : 20

0x8750, // RMAXB Default : 50 
0x8845, // RMINB Default : 3e
0x892d, // BMAXB Default : 2e
0x8a22, // BMINB Default : 20

0x8b00,
0x8d21,
0x8e71,

0x8f63,
0x9060,
0x915c,
0x9259,
0x9355,
0x9450,
0x9548,
0x963e,
0x9737,
0x9830,
0x9929,
0x9a26,
0x9b09,

0x0322, //AWB On
0x10fb,

0x0320, //AE On
0x108c,

0x01f0,
};

/************** Exposure Value Setting ****************/
/*
 * EV bias
 */
#if 0	/* unused variables */
static unsigned short SR030pc30_ev_m5[] = {	
0x0310,
0x40d0,
};
#endif	/* unused variables */
static unsigned short SR030pc30_ev_m4[] = {	
0x0310,
0x40d0,
};

static unsigned short SR030pc30_ev_m3[] = {	
0x0310,
0x40B0,
};

static unsigned short SR030pc30_ev_m2[] = {	
0x0310,
0x40A0,
};

static unsigned short SR030pc30_ev_m1[] = {	
0x0310,
0x4090,
};

static unsigned short SR030pc30_ev_default[] = {	
0x0310,
0x4080,
};

static unsigned short SR030pc30_ev_p1[] = {	
0x0310,
0x4010,
};

static unsigned short SR030pc30_ev_p2[] = {	
0x0310,
0x4020,
};

static unsigned short SR030pc30_ev_p3[] = {	
0x0310,
0x4030,
};

static unsigned short SR030pc30_ev_p4[] = {	
0x0310,
0x4050,
};
#if 0	/* unused variables */
static unsigned short SR030pc30_ev_p5[] = {	
0x0310,
0x40d0,
};
#endif	/* unused variables */
/*
 * EV bias for VT
 */
#if 0	/* unused variables */
static unsigned short SR030pc30_ev_vt_m5[] = {	
0x0310,
0x40d0,
};
#endif	/* unused variables */
static unsigned short SR030pc30_ev_vt_m4[] = {	
0x0310,
0x40d0,
};

static unsigned short SR030pc30_ev_vt_m3[] = {	
0x0310,
0x40d0,
};

static unsigned short SR030pc30_ev_vt_m2[] = {	
0x0310,
0x40d0,
};

static unsigned short SR030pc30_ev_vt_m1[] = {	
0x0310,
0x40d0,
};

static unsigned short SR030pc30_ev_vt_default[] = {	
0x0310,
0x40d0,
};

static unsigned short SR030pc30_ev_vt_p1[] = {	
0x0310,
0x40d0,
};

static unsigned short SR030pc30_ev_vt_p2[] = {	
0x0310,
0x40d0,
};

static unsigned short SR030pc30_ev_vt_p3[] = {	
0x0310,
0x40d0,
};

static unsigned short SR030pc30_ev_vt_p4[] = {	
0x0310,
0x40d0,
};
#if 0	/* unused variables */
static unsigned short SR030pc30_ev_vt_p5[] = {	
0x0310,
0x40d0,
};
#endif	/* unused variables */
/************** White Balance Setting ******************/
static unsigned short SR030pc30_wb_auto[] = {	
0x0322,
0x10e2,
0x1126,
0x8045, 
0x8120, 
0x8248,
0x8366, //66 RMAX Default : 50 
0x8426, //28 26 RMIN Default : 20
0x8555, //58 5a BMAX Default : 50
0x8620, //20 BMIN Default : 20
0x10fb,
};

static unsigned short SR030pc30_wb_tungsten[] = {	
0x0322,
0x107b,
0x1126,
0x8052, 
0x8120, 
0x8230, 
0x8353, 
0x8448, 
0x8535, 
0x862b, 
0x10fb,
0xffff,
};

static unsigned short SR030pc30_wb_fluorescent[] = {	
0x0322,
0x107b,
0x1126,
0x8042,
0x8120,
0x8251,
0x834a, //4c 4e RMAX
0x843a, //40
0x8555, //58
0x8645, //48 4a BMIN
0x10fb,
};

static unsigned short SR030pc30_wb_sunny[] = {	
0x0322,
0x107b,
0x1126,
0x8052, 
0x8120, 
0x8230, 
0x8353, 
0x8448, 
0x8535, 
0x862b, 
0x10fb,
0xffff,
};

static unsigned short SR030pc30_wb_cloudy[] = {	
0x0322,
0x107b,
0x1126,
0x8052,
0x8120,
0x8230,
0x837f, //7f 7d
0x8470, //7d 7d 7f RMIN
0x851f, //1c 1e 1a 21 BMAX
0x8610, //10 1e 
0x10fb,
};

/************** Effect Setting ********************/
static unsigned short SR030pc30_effect_none[] = {	
0x0310,
0x1103,
0x1230,
0x0313,
0x103b,
0x2002,
};

static unsigned short SR030pc30_effect_gray[] = {	
0x0310,
0x1103,
0x1233,
0x4480,
0x4580,
0x0313,
0x103b,
0x2002,
};

static unsigned short SR030pc30_effect_sepia[] = {	
0x0310,
0x1103,
0x1233,
0x4470,
0x4598,
0x0313,
0x103b,
0x2002,
};

static unsigned short SR030pc30_effect_negative[] = {	
0x0310,
0x1103,
0x1238,
0x0313,
0x103b,
0x2002,
};

static unsigned short SR030pc30_effect_aqua[] = {	
0x0310,
0x1103,
0x1233,
0x44b0,
0x4540,
0x0313,
0x103b,
0x2002,
};

/************** Blur Setting ********************/
/*Self shot*/
static unsigned short SR030pc30_blur_none[] = {	
0x0316,
0x3000,
0x3109,
0x321b,
0x3335,
0x345d,
0x357a,
0x3693,
0x37a7,
0x38b8,
0x39c6,
0x3ad2,
0x3be4,
0x3cf1,
0x3df9,
0x3eff,
};

static unsigned short SR030pc30_blur_p1[] = {	
0x0316,
0x3000,
0x3115,
0x3225,
0x3344,
0x3476,
0x359c,
0x36b8,
0x37cd,
0x38db,
0x39e6,
0x3aed,
0x3bf7,
0x3cfc,
0x3dfe,
0x3eff,
};

static unsigned short SR030pc30_blur_p2[] = {	
0x0316,
0x3000,
0x3118,
0x322e,
0x3357,
0x3495,
0x35bd,
0x36d6,
0x37e5,
0x38ef,
0x39f5,
0x3af9,
0x3bfd,
0x3cfe,
0x3dff,
0x3eff,
};

static unsigned short SR030pc30_blur_p3[] = {	
0x0316,
0x3000,
0x311b,
0x3238,
0x336c,
0x34b0,
0x35d4,
0x36e8,
0x37f2,
0x38f8,
0x39fb,
0x3afd,
0x3bff,
0x3cff,
0x3dff,
0x3eff,
};

/*vt call*/
static unsigned short SR030pc30_blur_vt_none[] = {	
0x0316,
0x1001,
0x3000,
0x3119,
0x3226,
0x333b,
0x345d,
0x3579,
0x368e,
0x379f,
0x38af,
0x39bd,
0x3aca,
0x3bdd,
0x3cec,
0x3df7,
0x3eff,
};

static unsigned short SR030pc30_blur_vt_p1[] = {	
0x0316,
0x3000,
0x3119,
0x3226,
0x3341,
0x3472,
0x3597,
0x36b3,
0x37c8,
0x38d7,
0x39e3,
0x3aeb,
0x3bf5,
0x3cfb,
0x3dfe,
0x3eff,
};

static unsigned short SR030pc30_blur_vt_p2[] = {	
0x0316,
0x3000,
0x3119,
0x322c,
0x3356,
0x3496,
0x35bf,
0x36d8,
0x37e8,
0x38f1,
0x39f6,
0x3afa,
0x3bfd,
0x3cff,
0x3dff,
0x3eff,
};

static unsigned short SR030pc30_blur_vt_p3[] = {	
0x0316,
0x3000,
0x311a,
0x323a,
0x3374,
0x34bd,
0x35e0,
0x36f0,
0x37f8,
0x38fc,
0x39fd,
0x3afe,
0x3bff,
0x3cff,
0x3dff,
0x3eff,
};

static unsigned short SR030pc30_dataline[] = {
0x0300,
0x5005, //Test Pattern

0x0311,
0x1098,

0x0312,
0x4022,
0x701c,

0x0313,
0x103a,
0x800c,

0x0314,
0x1000,

0x0315,
0x1002,

0x0316,
0x1000,

0x0320,
0x100c,

0x0322,
0x107b,
};

static unsigned short SR030pc30_dataline_stop[] = {	
0x0300,
0x5000, //Test Pattern

0x0311,
0x1099,

0x0312,
0x4023,
0x701d,

0x0313,
0x103b,
0x800d,

0x0314,
0x1001,

0x0315,
0x1003,

0x0316,
0x1001,

0x0320,
0x108c,

0x0322,
0x10fb,
};


/************** FPS********************/
static unsigned short SR030pc30_fps_7[] = {	
0x0300, //7

0x1000,

0xffaa, //170ms 

0x01f1,//Sleep On

0x11b1,//Fixed Disable


0x4001, //Hblank 336
0x4150, //
0x4200, //Vsync 20
0x4314, //

0x0320,
0x100c, // AE off 



0x2a90,
0x2bf5,

0x7034, // Ylvl 34
0x7911, // yth2

0x8801,//Expmax 20fps
0x8924,
0x8af8,

0x9103,//Fixed 7 FPS frame
0x923b,
0x9349,

0x9c05,//4s limit
0x9d5f,
0x9e00,//4s unit
0x9ffa,


0x108c, // AE on


0x0300,
0x1000,
0x11b5, //Fixed enable
0x01f0, //Sleep Off	 
};

static unsigned short SR030pc30_fps_10[] = {	
0x0300, //15 Fix

0x1000,

0xffaa, //170ms 


0x01f1,//Sleep On
0x11b1,//Fixed Disable

0x4001, //Hblank 336
0x4150, //
0x4200, //Vsync 20
0x4314, //

0x0320,
0x100c, // AE off


0x2a90,
0x2bf5,
0x30f8,

0x7034, // Ylvl 34
0x7812,//Yth 1
0x7911,//Yth 2
0x7A23,

0x8801,//Expmax 20fps
0x8924,
0x8af8,

0x9101,////Fixed 15 FPS frame
0x927c,
0x93dc,

0x9c05,
0x9d5f,
0x9e00,//4
0x9ffa,


0x108c, // AE on

0x0300,
0x11b5, //Fixed enable
0x01f0, //Sleep Off
};

static unsigned short SR030pc30_fps_15[] = {	
0x0300, //15 Fix

0x1000,

0xffaa, //170ms 


0x01f1,//Sleep On
0x11b1,//Fixed Disable

0x4001, //Hblank 336
0x4150, //
0x4200, //Vsync 20
0x4314, //

0x0320,
0x100c, // AE off


0x2a90,
0x2bf5,
0x30f8,

0x7034, // Ylvl 34
0x7812,//Yth 1
0x7911,//Yth 2
0x7A23,

0x8801,//Expmax 20fps
0x8924,
0x8af8,

0x9101,////Fixed 15 FPS frame
0x927c,
0x93dc,

0x9c05,//4
0x9d5f,
0x9e00,//4
0x9ffa,


0x108c, // AE on

0x0300,
0x11b5, //Fixed enable
0x01f0, //Sleep Off 		 
};

static unsigned short SR030pc30_vt_fps_7[] = {	
0x0300, //7

0x1000,

0xffaa, //170ms 

0x01f1,//Sleep On

0x11b1,//Fixed Disable


0x4001, //Hblank 336
0x4150, //
0x4200, //Vsync 20
0x4314, //

0x0320,
0x100c, // AE off 



0x2a90,
0x2bf5,

0x7034, // Ylvl 34
0x7911, // yth2

0x8801,//Expmax 20fps
0x8924,
0x8af8,

0x9103,//Fixed 7 FPS frame
0x923b,
0x9349,

0x9c05,//4s limit
0x9d5f,
0x9e00,//4s unit
0x9ffa,


0x108c, // AE on


0x0300,
0x1000,
0x11b5, //Fixed enable
0x01f0, //Sleep Off	 
};

static unsigned short SR030pc30_vt_fps_10[] = {	
0x0300, //7

0x1000,

0xffaa, //170ms 

0x01f1,//Sleep On

0x11b1,//Fixed Disable


0x4001, //Hblank 336
0x4150, //
0x4200, //Vsync 20
0x4314, //

0x0320,
0x100c, // AE off 



0x2a90,
0x2bf5,

0x7034, // Ylvl 34
0x7911, // yth2

0x8801,//Expmax 20fps
0x8924,
0x8af8,

0x9103,//Fixed 7 FPS frame
0x923b,
0x9349,

0x9c05,//4
0x9d5f,
0x9e00,//4
0x9ffa,


0x108c, // AE on


0x0300,
0x1000,
0x11b5, //Fixed enable
0x01f0, //Sleep Off	 
};

static unsigned short SR030pc30_vt_fps_15[] = {	
0x0300, //15 Fix

0x1000,

0xffaa, //170ms 


0x01f1,//Sleep On
0x11b1,//Fixed Disable

0x4001, //Hblank 336
0x4150, //
0x4200, //Vsync 20
0x4314, //

0x0320,
0x100c, // AE off


0x2a90,
0x2bf5,
0x30f8,

0x7034, // Ylvl 34
0x7812,//Yth 1
0x7911,//Yth 2
0x7A23,

0x8801,//Expmax 20fps
0x8924,
0x8af8,

0x9101,////Fixed 15 FPS frame
0x927c,
0x93dc,

0x9c05,//4
0x9d5f,
0x9e00,//4
0x9ffa,


0x108c, // AE on

0x0300,
0x11b5, //Fixed enable
0x01f0, //Sleep Off
};

#endif
