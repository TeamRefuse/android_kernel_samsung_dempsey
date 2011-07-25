/* linux/arch/arm/mach-s5pv210/pm.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * S5PV210 - Power Management support
 *
 * Based on arch/arm/mach-s3c2410/pm.c
 * Copyright (c) 2006 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/io.h>

#include <plat/cpu.h>
#include <plat/pm.h>
#include <plat/regs-timer.h>

#include <mach/regs-irq.h>
#include <mach/regs-clock.h>
#include <mach/regs-mem.h>
#include <mach/power-domain.h>

#include <mach/regs-gpio.h>
#include <linux/io.h>
extern void PMIC_dump(void);

static struct sleep_save core_save[] = {
	/* PLL Control */
	SAVE_ITEM(S5P_APLL_CON),
	SAVE_ITEM(S5P_MPLL_CON),
	SAVE_ITEM(S5P_EPLL_CON),
	SAVE_ITEM(S5P_VPLL_CON),

	/* Clock source */
	SAVE_ITEM(S5P_CLK_SRC0),
	SAVE_ITEM(S5P_CLK_SRC1),
	SAVE_ITEM(S5P_CLK_SRC2),
	SAVE_ITEM(S5P_CLK_SRC3),
	SAVE_ITEM(S5P_CLK_SRC4),
	SAVE_ITEM(S5P_CLK_SRC5),
	SAVE_ITEM(S5P_CLK_SRC6),

	/* Clock source Mask */
	SAVE_ITEM(S5P_CLK_SRC_MASK0),
	SAVE_ITEM(S5P_CLK_SRC_MASK1),

	/* Clock Divider */
	SAVE_ITEM(S5P_CLK_DIV0),
	SAVE_ITEM(S5P_CLK_DIV1),
	SAVE_ITEM(S5P_CLK_DIV2),
	SAVE_ITEM(S5P_CLK_DIV3),
	SAVE_ITEM(S5P_CLK_DIV4),
	SAVE_ITEM(S5P_CLK_DIV5),
	SAVE_ITEM(S5P_CLK_DIV6),
	SAVE_ITEM(S5P_CLK_DIV7),

	/* Clock Main Gate */
	SAVE_ITEM(S5P_CLKGATE_MAIN0),
	SAVE_ITEM(S5P_CLKGATE_MAIN1),
	SAVE_ITEM(S5P_CLKGATE_MAIN2),

	/* Clock source Peri Gate */
	SAVE_ITEM(S5P_CLKGATE_PERI0),
	SAVE_ITEM(S5P_CLKGATE_PERI1),

	/* Clock source SCLK Gate */
	SAVE_ITEM(S5P_CLKGATE_SCLK0),
	SAVE_ITEM(S5P_CLKGATE_SCLK1),

	/* Clock IP Clock gate */
	SAVE_ITEM(S5P_CLKGATE_IP0),
	SAVE_ITEM(S5P_CLKGATE_IP1),
	SAVE_ITEM(S5P_CLKGATE_IP2),
	SAVE_ITEM(S5P_CLKGATE_IP3),
	SAVE_ITEM(S5P_CLKGATE_IP4),

	/* Clock Blcok and Bus gate */
	SAVE_ITEM(S5P_CLKGATE_BLOCK),
	SAVE_ITEM(S5P_CLKGATE_IP5),

	/* Clock ETC */
	SAVE_ITEM(S5P_CLK_OUT),
	SAVE_ITEM(S5P_MDNIE_SEL),

	/* PWM Register */
	SAVE_ITEM(S3C2410_TCFG0),
	SAVE_ITEM(S3C2410_TCFG1),
	SAVE_ITEM(S3C64XX_TINT_CSTAT),
	SAVE_ITEM(S3C2410_TCON),
	SAVE_ITEM(S3C2410_TCNTB(0)),
	SAVE_ITEM(S3C2410_TCMPB(0)),
	SAVE_ITEM(S3C2410_TCNTO(0)),
};

void s5pv210_cpu_suspend(void)
{
	unsigned long tmp;

	/* issue the standby signal into the pm unit. Note, we
	 * issue a write-buffer drain just in case */

#if 0    // sleep GPIO read,  pull up down

      unsigned int conreg;
      unsigned int datareg;
      unsigned int pullupdown;
      unsigned int PNData;
      unsigned int PDNcon;
      unsigned int PDNpullupdown;
	  
      unsigned int baseaddr;
      unsigned int Maxbaseaddr;
	  
	int bitNumber;  
	int MAXbitNumber;  

	int conregBuff[8];
	int dataregBuff[8];
	int pullupdownBuff[8];
	int PDNconBuff[8];
	int PDNpullupdownBuff[8];
	int PDNdataregBuff[8];

	char regNameStringBuff[10];
	char * regNameString = &regNameStringBuff[0];
	
	char configregStringBuff[10];
	char * configString = &configregStringBuff[0];

	char dataStringBuff[10];
	char * dataString = &dataStringBuff[0];	

	char pullDownStringBuff[10];
	char * pullDownString = &pullDownStringBuff[0];		

	char PDNconStringBuff[10];
	char * PDNconString = &PDNconStringBuff[0];	
	
	char PDNpullUDStringBuff[10];
	char * PDNpullPDString = &PDNpullUDStringBuff[0];	
	char PDNdataStringBuff[10];
	char * PDNdataString = &PDNdataStringBuff[0];	
	  
     baseaddr = (unsigned int) S5PV210_GPA0_BASE;
     Maxbaseaddr = (unsigned int) S5PV210_MP07_BASE;	 
     MAXbitNumber = 8;

     for(; (baseaddr <= (unsigned int)S5PV210_GPH3_BASE)  ; baseaddr += 0x20)
     	{
            if (( baseaddr > (unsigned int)S5PV210_MP07_BASE)  &&  ( baseaddr < (unsigned int)S5PV210_GPH0_BASE) )
		 continue;		
	
	     conreg 	 =  readl(baseaddr);
	     datareg 	 =  readl(baseaddr + 0x04);
	     pullupdown =  readl(baseaddr + 0x08);
	     PDNcon      =    readl(baseaddr + 0x10);
	     PDNpullupdown =  readl(baseaddr + 0x14);

	    if (baseaddr == (unsigned int) S5PV210_GPA0_BASE)
	    	{
					regNameString = "GPA0";
					MAXbitNumber = 8;
	    	}			
          else if(baseaddr == (unsigned int) S5PV210_GPA1_BASE)   
		{  	
					regNameString = "GPA1";
                                MAXbitNumber = 4; 					
          	}	
          else if(baseaddr == (unsigned int) S5PV210_GPB_BASE)   
          	{
					regNameString = "GPB";
					MAXbitNumber = 8;
          	}			
          else if(baseaddr == (unsigned int) S5PV210_GPC0_BASE)   
          	{
					regNameString = "GPC0";
					MAXbitNumber = 5;
          	}			
          else if(baseaddr == (unsigned int) S5PV210_GPC1_BASE)   
          	{
					regNameString = "GPC1";
					MAXbitNumber = 5;
          	}			
          else if(baseaddr == (unsigned int) S5PV210_GPD0_BASE)   
          	{
					regNameString = "GPD0";
					MAXbitNumber = 4;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_GPD1_BASE)   
          	{
					regNameString = "GPD1";
					MAXbitNumber = 6;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_GPE0_BASE)   
          	{
					regNameString = "GPE0";
					MAXbitNumber = 8;
          	}	
          else if(baseaddr == (unsigned int) S5PV210_GPE1_BASE)   
          	{
					regNameString = "GPE1";
					MAXbitNumber = 5;
          	}			  
          else if(baseaddr == (unsigned int) S5PV210_GPF0_BASE)   
          	{
					regNameString = "GPF0";
					MAXbitNumber = 8;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_GPF1_BASE)   
          	{
					regNameString = "GPF1";
					MAXbitNumber = 8;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_GPF2_BASE)   
          	{
					regNameString = "GPF2";
					MAXbitNumber = 8;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_GPF3_BASE)   
          	{
					regNameString = "GPF3";
					MAXbitNumber = 6;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_GPG0_BASE)   
          	{
					regNameString = "GPG0";
					MAXbitNumber = 7;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_GPG1_BASE)   
          	{
					regNameString = "GPG1";
					MAXbitNumber = 7;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_GPG2_BASE)   
          	{
					regNameString = "GPG2";
					MAXbitNumber = 7;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_GPG3_BASE)   
          	{
					regNameString = "GPG3";
					MAXbitNumber = 7;
          	}	

          else if(baseaddr == (unsigned int)S5PV210_GPH0_BASE)   
          	{
					regNameString = "GPH0";
					MAXbitNumber = 8;
          	}
          else if(baseaddr == (unsigned int)S5PV210_GPH1_BASE)   
          	{
					regNameString = "GPH1";
					MAXbitNumber = 8;
          	}
          else if(baseaddr == (unsigned int)S5PV210_GPH2_BASE)   
          	{
					regNameString = "GPH2";
					MAXbitNumber = 8;
          	}
          else if(baseaddr == (unsigned int)S5PV210_GPH3_BASE)   
          	{
					regNameString = "GPH3";
					MAXbitNumber = 8;
          	}
          else if(baseaddr == (unsigned int)S5PV210_GPI_BASE)   
          	{
					regNameString = "GPI";
					MAXbitNumber = 7;
          	}		  
          else if(baseaddr == (unsigned int)S5PV210_GPJ0_BASE)   
          	{
					regNameString = "GPJ0";
					MAXbitNumber = 8;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_GPJ1_BASE)   
          	{
					regNameString = "GPJ1";
					MAXbitNumber = 6;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_GPJ2_BASE)   
          	{
					regNameString = "GPJ2";
					MAXbitNumber = 8;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_GPJ3_BASE)   
          	{
					regNameString = "GPJ3";
					MAXbitNumber = 8;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_GPJ4_BASE)   
          	{
					regNameString = "GPJ4";
					MAXbitNumber = 5;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_MP01_BASE)   
          	{
					regNameString = "MP01";
					MAXbitNumber = 8;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_MP02_BASE)   
          	{
					regNameString = "MP02";
					MAXbitNumber = 4;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_MP03_BASE)   
          	{
					regNameString = "MP03";
					MAXbitNumber = 8;
          	}			  
           else if(baseaddr == (unsigned int)S5PV210_MP04_BASE)   
          	{
					regNameString = "MP04";
					MAXbitNumber = 8;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_MP05_BASE)   
          	{
					regNameString = "MP05";
					MAXbitNumber = 8;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_MP06_BASE)   
          	{
					regNameString = "MP06";
					MAXbitNumber = 8;
          	}	
          else if(baseaddr == (unsigned int)S5PV210_MP07_BASE)   
          	{
					regNameString = "MP07";
					MAXbitNumber = 8;
          	}
	    else
	    	{
					regNameString = "NONE";
					MAXbitNumber = 8;	    	
	    	}
		  
	 
	     for(bitNumber=0; bitNumber < MAXbitNumber;bitNumber++)
	     {
	     	    conregBuff[bitNumber] =  (conreg >> (4*bitNumber)) & 0xf;
	     	    dataregBuff[bitNumber] =  (datareg >> (1*bitNumber)) & 0x1;
	     	    pullupdownBuff[bitNumber] =  (pullupdown >> (2*bitNumber)) & 0x3;
	     	    PDNconBuff[bitNumber] =  (PDNcon >> (2*bitNumber)) & 0x3;
	     	    PDNpullupdownBuff[bitNumber] =  (PDNpullupdown >> (2*bitNumber)) & 0x3;

                switch(conregBuff[bitNumber])
                	{
		           case 0x0:
				   	configString = "In  ";
					break;
		           case 0x1:
				   	configString = "Out ";
					break;
		           case 0xf:
				   	configString = "INT ";
					break;	
			     default: 
				   	configString = "FUNC";
					break;					 	
					
                	}		


                switch(dataregBuff[bitNumber])
                	{
		           case 0x0:
		           		if(conregBuff[bitNumber] == 0x0 || conregBuff[bitNumber] == 0x1 || conregBuff[bitNumber] == 0xf)
				   		dataString = "L     ";
				   	else
				   		dataString = "X     ";
					break;
		           case 0x1:
		           		if(conregBuff[bitNumber] == 0x0 || conregBuff[bitNumber] == 0x1 || conregBuff[bitNumber] == 0xf)
				   		dataString = "H     ";
				   	else
				   		dataString = "X     ";
					break;
	    	          default: 
				   	dataString = "X     ";
					break;					 	
                	}

                switch(pullupdownBuff[bitNumber])
                	{
		           case 0x0:
				   	pullDownString = "NP   ";
				   	if(dataregBuff[bitNumber] == 0x0)
				   		dataString = "L     ";
				   	else if(dataregBuff[bitNumber] == 0x1)
				   		dataString = "H     ";					
					break;
		           case 0x1:
				   	pullDownString = "PD   ";
					break;
		           case 0x2:
				   	pullDownString = "PU   ";
					break;
		           case 0x3:
				   	pullDownString = "Reserved";
					break;					
	    	          default: 
				   	pullDownString = "Reserved";
					break;					 	
                	}

                switch(PDNconBuff[bitNumber])
                	{
		           case 0x0:
				   	PDNconString = "OUT0   ";
				   	PDNdataString = "L    ";
					break;
		           case 0x1:
				   	PDNconString = "OUT1   ";
				   	PDNdataString = "H    ";
					break;
		           case 0x2:
				   	PDNconString = "InX    ";
				   	PDNdataString = "X    ";
					break;
		           case 0x3:
				   	PDNconString = "Prev  ";
				   	PDNdataString = "X    ";
					break;					
	    	          default: 
				   	PDNconString = "Prev  ";
				   	PDNdataString = "X    ";
					break;					 	
                	}

                switch(PDNpullupdownBuff[bitNumber])
                	{
		           case 0x0:
				   	PDNpullPDString = "NP	   ";
					break;
		           case 0x1:
				   	PDNpullPDString = "PD      ";
					break;
		           case 0x2:
				   	PDNpullPDString = "PU      ";
					break;
		           case 0x3:
				   	PDNpullPDString = "Reserved";
					break;					
	    	          default: 
				   	PDNpullPDString = "Reserved";
					break;					 	
                	}


		   printk("%s[%d],Con=%s,%x D= %s, P_UD = %s, PD_CON=%s, PD_PD= %s \n", \
			regNameString,bitNumber, configString , conregBuff[bitNumber], dataString, pullDownString, \ 
			 PDNconString,PDNpullPDString);		


/*
//for Excel file *****************************************************
		   printk(" %s[%d] %s %s %s %s %s %s \n", \
			regNameString, bitNumber, \
			configString, pullDownString, dataString,  \ 
			PDNconString, PDNpullPDString, PDNdataString);
*/		   	
/*				
		   printk("BA= %s, ConReg[%d]= %x, DataReg[%d]= %d, PullUDReg[%d] = %d , PDNcon[%d] = %d , PDNpullPD[%d] = %d \n", \
			baseaddr,bitNumber, conregBuff[bitNumber] , bitNumber, dataregBuff[bitNumber],bitNumber, pullupdownBuff[bitNumber], \ 
			bitNumber, PDNconBuff[bitNumber],bitNumber,PDNpullupdownBuff[bitNumber]);
*/			
	     }
	     //printk("\n");
    }
    printk("\n");
#endif

	tmp = 0;

	asm("b 1f\n\t"
	    ".align 5\n\t"
	    "1:\n\t"
	    "mcr p15, 0, %0, c7, c10, 5\n\t"
	    "mcr p15, 0, %0, c7, c10, 4\n\t"
	    "wfi" : : "r" (tmp));

	/* we should never get past here */
	panic("sleep resumed to originator?");
}

static void s5pv210_pm_prepare(void)
{
	unsigned int tmp;

	/* ensure at least INFORM0 has the resume address */
	__raw_writel(virt_to_phys(s3c_cpu_resume), S5P_INFORM0);

	/* WFI for SLEEP mode configuration by SYSCON */
	tmp = __raw_readl(S5P_PWR_CFG);
	tmp &= S5P_CFG_WFI_CLEAN;
	tmp |= S5P_CFG_WFI_SLEEP;
	__raw_writel(tmp, S5P_PWR_CFG);

	/* SYSCON interrupt handling disable */
	tmp = __raw_readl(S5P_OTHERS);
	tmp |= S5P_OTHER_SYSC_INTOFF;
	__raw_writel(tmp, S5P_OTHERS);

	__raw_writel(0xffffffff, (VA_VIC0 + VIC_INT_ENABLE_CLEAR));
	__raw_writel(0xffffffff, (VA_VIC1 + VIC_INT_ENABLE_CLEAR));
	__raw_writel(0xffffffff, (VA_VIC2 + VIC_INT_ENABLE_CLEAR));
	__raw_writel(0xffffffff, (VA_VIC3 + VIC_INT_ENABLE_CLEAR));

	s3c_pm_do_save(core_save, ARRAY_SIZE(core_save));
}

static void s5pv210_pm_resume(void)
{
	u32 tmp;

	tmp = __raw_readl(S5P_OTHERS);
	tmp |= (S5P_OTHERS_RET_IO | S5P_OTHERS_RET_CF |\
		S5P_OTHERS_RET_MMC | S5P_OTHERS_RET_UART);
	__raw_writel(tmp , S5P_OTHERS);

	s3c_pm_do_restore_core(core_save, ARRAY_SIZE(core_save));

}

static __init int s5pv210_pm_drvinit(void)
{
	pm_cpu_prep = s5pv210_pm_prepare;
	pm_cpu_sleep = s5pv210_cpu_suspend;
	pm_cpu_restore = s5pv210_pm_resume;
	return 0;
}

arch_initcall(s5pv210_pm_drvinit);
