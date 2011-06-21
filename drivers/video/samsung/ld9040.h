#ifndef LD9040_H
#define LD9040_H

//#define SUPPORT_AMS427_LCD		// 4.3" LCD

#define SLEEPMSEC		0x1000
#define ENDDEF			0x2000
#define	DEFMASK			0xFF00
#define COMMAND_ONLY		0xFE
#define DATA_ONLY		0xFF

extern const unsigned short ld9040_SETTING[];
extern const unsigned short ld9040_DISPLAY_ON[];
extern const unsigned short ld9040_DISPLAY_OFF[];
extern const unsigned short ld9040_STANDBY_ON[];
extern const unsigned short ld9040_STANDBY_OFF[];
extern const unsigned short ld9040_gamma_update[];

extern const unsigned short ld9040_22gamma_300cd[];
extern const unsigned short ld9040_22gamma_290cd[];
extern const unsigned short ld9040_22gamma_280cd[];
extern const unsigned short ld9040_22gamma_270cd[];
extern const unsigned short ld9040_22gamma_260cd[];
extern const unsigned short ld9040_22gamma_250cd[];
extern const unsigned short ld9040_22gamma_240cd[];
extern const unsigned short ld9040_22gamma_230cd[];
extern const unsigned short ld9040_22gamma_220cd[];
extern const unsigned short ld9040_22gamma_210cd[];
extern const unsigned short ld9040_22gamma_200cd[];
extern const unsigned short ld9040_22gamma_190cd[];
extern const unsigned short ld9040_22gamma_180cd[];
extern const unsigned short ld9040_22gamma_170cd[];
extern const unsigned short ld9040_22gamma_160cd[];
extern const unsigned short ld9040_22gamma_150cd[];
extern const unsigned short ld9040_22gamma_140cd[];
extern const unsigned short ld9040_22gamma_130cd[];
extern const unsigned short ld9040_22gamma_120cd[];
extern const unsigned short ld9040_22gamma_110cd[];
extern const unsigned short ld9040_22gamma_100cd[];
extern const unsigned short ld9040_22gamma_90cd[];
extern const unsigned short ld9040_22gamma_80cd[];
extern const unsigned short ld9040_22gamma_70cd[];
extern const unsigned short ld9040_22gamma_50cd[];
extern const unsigned short ld9040_22gamma_40cd[];
extern const unsigned short ld9040_22gamma_30cd[];

extern const unsigned short ld9040_19gamma_300cd[];
extern const unsigned short ld9040_19gamma_290cd[];
extern const unsigned short ld9040_19gamma_280cd[];
extern const unsigned short ld9040_19gamma_270cd[];
extern const unsigned short ld9040_19gamma_260cd[];
extern const unsigned short ld9040_19gamma_250cd[];
extern const unsigned short ld9040_19gamma_240cd[];
extern const unsigned short ld9040_19gamma_230cd[];
extern const unsigned short ld9040_19gamma_220cd[];
extern const unsigned short ld9040_19gamma_210cd[];
extern const unsigned short ld9040_19gamma_200cd[];
extern const unsigned short ld9040_19gamma_190cd[];
extern const unsigned short ld9040_19gamma_180cd[];
extern const unsigned short ld9040_19gamma_170cd[];
extern const unsigned short ld9040_19gamma_160cd[];
extern const unsigned short ld9040_19gamma_150cd[];
extern const unsigned short ld9040_19gamma_140cd[];
extern const unsigned short ld9040_19gamma_130cd[];
extern const unsigned short ld9040_19gamma_120cd[];
extern const unsigned short ld9040_19gamma_110cd[];
extern const unsigned short ld9040_19gamma_100cd[];
extern const unsigned short ld9040_19gamma_90cd[];
extern const unsigned short ld9040_19gamma_80cd[];
extern const unsigned short ld9040_19gamma_70cd[];
extern const unsigned short ld9040_19gamma_50cd[];
extern const unsigned short ld9040_19gamma_40cd[];
extern const unsigned short ld9040_19gamma_30cd[];

extern const unsigned short ld9040_ams427_22gamma_300cd[];
extern const unsigned short ld9040_ams427_22gamma_290cd[];
extern const unsigned short ld9040_ams427_22gamma_280cd[];
extern const unsigned short ld9040_ams427_22gamma_270cd[];
extern const unsigned short ld9040_ams427_22gamma_260cd[];
extern const unsigned short ld9040_ams427_22gamma_250cd[];
extern const unsigned short ld9040_ams427_22gamma_240cd[];
extern const unsigned short ld9040_ams427_22gamma_230cd[];
extern const unsigned short ld9040_ams427_22gamma_220cd[];
extern const unsigned short ld9040_ams427_22gamma_210cd[];
extern const unsigned short ld9040_ams427_22gamma_200cd[];
extern const unsigned short ld9040_ams427_22gamma_190cd[];
extern const unsigned short ld9040_ams427_22gamma_180cd[];
extern const unsigned short ld9040_ams427_22gamma_170cd[];
extern const unsigned short ld9040_ams427_22gamma_160cd[];
extern const unsigned short ld9040_ams427_22gamma_150cd[];
extern const unsigned short ld9040_ams427_22gamma_140cd[];
extern const unsigned short ld9040_ams427_22gamma_130cd[];
extern const unsigned short ld9040_ams427_22gamma_120cd[];
extern const unsigned short ld9040_ams427_22gamma_110cd[];
extern const unsigned short ld9040_ams427_22gamma_100cd[];
extern const unsigned short ld9040_ams427_22gamma_90cd[];
extern const unsigned short ld9040_ams427_22gamma_80cd[];
extern const unsigned short ld9040_ams427_22gamma_70cd[];
extern const unsigned short ld9040_ams427_22gamma_50cd[];
//extern const unsigned short ld9040_ams427_22gamma_40cd[];
//extern const unsigned short ld9040_ams427_22gamma_30cd[];

extern const unsigned short ld9040_ams427_19gamma_300cd[];
extern const unsigned short ld9040_ams427_19gamma_290cd[];
extern const unsigned short ld9040_ams427_19gamma_280cd[];
extern const unsigned short ld9040_ams427_19gamma_270cd[];
extern const unsigned short ld9040_ams427_19gamma_260cd[];
extern const unsigned short ld9040_ams427_19gamma_250cd[];
extern const unsigned short ld9040_ams427_19gamma_240cd[];
extern const unsigned short ld9040_ams427_19gamma_230cd[];
extern const unsigned short ld9040_ams427_19gamma_220cd[];
extern const unsigned short ld9040_ams427_19gamma_210cd[];
extern const unsigned short ld9040_ams427_19gamma_200cd[];
extern const unsigned short ld9040_ams427_19gamma_190cd[];
extern const unsigned short ld9040_ams427_19gamma_180cd[];
extern const unsigned short ld9040_ams427_19gamma_170cd[];
extern const unsigned short ld9040_ams427_19gamma_160cd[];
extern const unsigned short ld9040_ams427_19gamma_150cd[];
extern const unsigned short ld9040_ams427_19gamma_140cd[];
extern const unsigned short ld9040_ams427_19gamma_130cd[];
extern const unsigned short ld9040_ams427_19gamma_120cd[];
extern const unsigned short ld9040_ams427_19gamma_110cd[];
extern const unsigned short ld9040_ams427_19gamma_100cd[];
extern const unsigned short ld9040_ams427_19gamma_90cd[];
extern const unsigned short ld9040_ams427_19gamma_80cd[];
extern const unsigned short ld9040_ams427_19gamma_70cd[];
extern const unsigned short ld9040_ams427_19gamma_50cd[];
//extern const unsigned short ld9040_ams427_19gamma_40cd[];
//extern const unsigned short ld9040_ams427_19gamma_30cd[];

extern const unsigned short ld9040_4DOT52_22gamma_300cd[];

static const unsigned short *ld9040_p22Gamma_set[] = {        
#if !defined(SUPPORT_AMS427_LCD)
 	ld9040_22gamma_30cd,
 	ld9040_22gamma_40cd,
 	//ld9040_22gamma_50cd,
	ld9040_22gamma_70cd,
	//ld9040_22gamma_80cd,
	ld9040_22gamma_90cd,
	ld9040_22gamma_100cd,
	ld9040_22gamma_110cd,
	ld9040_22gamma_120cd,
	ld9040_22gamma_130cd,
	ld9040_22gamma_140cd,
	ld9040_22gamma_150cd,
	ld9040_22gamma_160cd,
	ld9040_22gamma_170cd,
	ld9040_22gamma_180cd,
	ld9040_22gamma_190cd,
	ld9040_22gamma_200cd,
	ld9040_22gamma_210cd,
	ld9040_22gamma_220cd,
	ld9040_22gamma_230cd,
	ld9040_22gamma_240cd,
	ld9040_22gamma_250cd,
	ld9040_22gamma_260cd,
	ld9040_22gamma_270cd,
	ld9040_22gamma_280cd,
	ld9040_22gamma_290cd,
	ld9040_22gamma_300cd,
#else
 	ld9040_ams427_22gamma_50cd,
	ld9040_ams427_22gamma_70cd,
	ld9040_ams427_22gamma_80cd,
	ld9040_ams427_22gamma_90cd,
	ld9040_ams427_22gamma_100cd,
	ld9040_ams427_22gamma_110cd,
	ld9040_ams427_22gamma_120cd,
	ld9040_ams427_22gamma_130cd,
	ld9040_ams427_22gamma_140cd,
	ld9040_ams427_22gamma_150cd,
	ld9040_ams427_22gamma_160cd,
	ld9040_ams427_22gamma_170cd,
	ld9040_ams427_22gamma_180cd,
	ld9040_ams427_22gamma_190cd,
	ld9040_ams427_22gamma_200cd,
	ld9040_ams427_22gamma_210cd,
	ld9040_ams427_22gamma_220cd,
	ld9040_ams427_22gamma_230cd,
	ld9040_ams427_22gamma_240cd,
	ld9040_ams427_22gamma_250cd,
	ld9040_ams427_22gamma_260cd,
	ld9040_ams427_22gamma_270cd,
	ld9040_ams427_22gamma_280cd,
	ld9040_ams427_22gamma_290cd,
	ld9040_ams427_22gamma_300cd,
#endif
};                                             
                                                

static const unsigned short *ld9040_4DOT52_p22Gamma_set[] = {        
 	ld9040_22gamma_30cd,
 	ld9040_22gamma_40cd,
	ld9040_22gamma_70cd,
	ld9040_22gamma_90cd,
	ld9040_22gamma_100cd,
	ld9040_22gamma_110cd,
	ld9040_22gamma_120cd,
	ld9040_22gamma_130cd,
	ld9040_22gamma_140cd,
	ld9040_22gamma_150cd,
	ld9040_22gamma_160cd,
	ld9040_22gamma_170cd,
	ld9040_22gamma_180cd,
	ld9040_22gamma_190cd,
	ld9040_22gamma_200cd,
	ld9040_22gamma_210cd,
	ld9040_22gamma_220cd,
	ld9040_22gamma_230cd,
	ld9040_22gamma_240cd,
	ld9040_22gamma_250cd,
	ld9040_22gamma_260cd,
	ld9040_22gamma_270cd,
	ld9040_22gamma_280cd,
	ld9040_22gamma_290cd,
	ld9040_4DOT52_22gamma_300cd,
};


static const unsigned short SEQ_ELVSS_49[] = {
	0xB2, 
	0x110,
	0x110,
	0x110,
	0x110,
	ENDDEF, 0x0000
};

static const unsigned short SEQ_ELVSS_42[] = {
	0xB2, 
	0x117,
	0x117,
	0x117,
	0x117,
	ENDDEF, 0x0000
};


static const unsigned short SEQ_ELVSS_39[] = {
	0xB2, 
	0x11A,
	0x11A,
	0x11A,
	0x11A,
	ENDDEF, 0x0000
};

static const unsigned short SEQ_ELVSS_35[] = {
	0xB2, 
	0x11E,
	0x11E,
	0x11E,
	0x11E,
	ENDDEF, 0x0000
};

static const unsigned short *ld9040_p19Gamma_set[] = {    
#if !defined(SUPPORT_AMS427_LCD)
 	ld9040_19gamma_30cd,
 	ld9040_19gamma_40cd,
 	//ld9040_19gamma_50cd,
	ld9040_19gamma_70cd,
	//ld9040_19gamma_80cd,
	ld9040_19gamma_90cd,
	ld9040_19gamma_100cd,
	ld9040_19gamma_110cd,
	ld9040_19gamma_120cd,
	ld9040_19gamma_130cd,
	ld9040_19gamma_140cd,
	ld9040_19gamma_150cd,
	ld9040_19gamma_160cd,
	ld9040_19gamma_170cd,
	ld9040_19gamma_180cd,
	ld9040_19gamma_190cd,
	ld9040_19gamma_200cd,
	ld9040_19gamma_210cd,
	ld9040_19gamma_220cd,
	ld9040_19gamma_230cd,
	ld9040_19gamma_240cd,
	ld9040_19gamma_250cd,
	ld9040_19gamma_260cd,
	ld9040_19gamma_270cd,
	ld9040_19gamma_280cd,
	ld9040_19gamma_290cd,
	ld9040_19gamma_300cd,
#else
 	ld9040_ams427_19gamma_50cd,
	ld9040_ams427_19gamma_70cd,
	ld9040_ams427_19gamma_80cd,
	ld9040_ams427_19gamma_90cd,
	ld9040_ams427_19gamma_100cd,
	ld9040_ams427_19gamma_110cd,
	ld9040_ams427_19gamma_120cd,
	ld9040_ams427_19gamma_130cd,
	ld9040_ams427_19gamma_140cd,
	ld9040_ams427_19gamma_150cd,
	ld9040_ams427_19gamma_160cd,
	ld9040_ams427_19gamma_170cd,
	ld9040_ams427_19gamma_180cd,
	ld9040_ams427_19gamma_190cd,
	ld9040_ams427_19gamma_200cd,
	ld9040_ams427_19gamma_210cd,
	ld9040_ams427_19gamma_220cd,
	ld9040_ams427_19gamma_230cd,
	ld9040_ams427_19gamma_240cd,
	ld9040_ams427_19gamma_250cd,
	ld9040_ams427_19gamma_260cd,
	ld9040_ams427_19gamma_270cd,
	ld9040_ams427_19gamma_280cd,
	ld9040_ams427_19gamma_290cd,
	ld9040_ams427_19gamma_300cd,
#endif
}; 


#endif
