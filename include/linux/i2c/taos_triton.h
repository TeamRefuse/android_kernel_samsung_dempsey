/*******************************************************************************

	 Author :	 	R&D Team, North America/GSM, 
				Mobile communication division, 
				SAMSUNG ELECTRONICS
			
	 Desription :	Samsung Customized TAOS Triton ambient light 
				and proximity sensor driver

  *******************************************************************************/

/* Interrupt pin declaration */
#define GPIO_PS_ALS_INT_IRQ 	IRQ_EINT2

/* input device for proximity sensor */
#define USE_INPUT_DEVICE 	0  			/* 0 : No Use  ,  1: Use  */
#define I2C_DF_NOTIFY 		0x01		//* notify on bus (de/a)ttaches  
#define I2C_M_WR 			0x0

#define TMD2711_ADDR		0x39

#define TAOS_DEVICE_NAME	"tmd2711"

/* Triton register offsets */
#define CNTRL				0x00
#define ALS_TIME			0X01
#define PRX_TIME			0x02
#define WAIT_TIME			0x03
#define ALS_MINTHRESHLO			0X04
#define ALS_MINTHRESHHI			0X05
#define ALS_MAXTHRESHLO			0X06
#define ALS_MAXTHRESHHI			0X07
#define PRX_MINTHRESHLO			0X08
#define PRX_MINTHRESHHI			0X09
#define PRX_MAXTHRESHLO			0X0A
#define PRX_MAXTHRESHHI			0X0B
#define INTERRUPT			0x0C
#define PRX_CFG				0x0D
#define PRX_COUNT			0x0E
#define GAIN				0x0F
#define REVID				0x11
#define CHIPID				0x12
#define STATUS				0x13
#define ALS_CHAN0LO			0x14
#define ALS_CHAN0HI			0x15
#define ALS_CHAN1LO			0x16
#define ALS_CHAN1HI			0x17
#define PRX_LO				0x18
#define PRX_HI				0x19
#define TEST_STATUS			0x1F
	
/* Triton cmd reg masks */
#define CMD_REG					0X80
#define CMD_BYTE_RW				0x00
#define CMD_WORD_BLK_RW			0x20
#define CMD_SPL_FN				0x60
#define CMD_PROX_INTCLR			0X05
#define CMD_ALS_INTCLR			0X06
#define CMD_PROXALS_INTCLR		0X07
#define CMD_TST_REG				0X08
#define CMD_USER_REG			0X09
	
/* Triton cntrl reg masks */
#define CNTL_REG_CLEAR			0x00
#define CNTL_PROX_INT_ENBL		0X20
#define CNTL_ALS_INT_ENBL		0X10
#define CNTL_WAIT_TMR_ENBL		0X08
#define CNTL_PROX_DET_ENBL		0X04
#define CNTL_ADC_ENBL			0x02
#define CNTL_PWRON				0x01
#define CNTL_ALSPON_ENBL		0x03
#define CNTL_INTALSPON_ENBL		0x13
#define CNTL_PROXPON_ENBL		0x0F
#define CNTL_INTPROXPON_ENBL		0x2F
	
/* Triton status reg masks */
#define STA_ADCVALID			0x01
#define STA_PRXVALID			0x02
#define STA_ADC_PRX_VALID		0x03
#define STA_ADCINTR				0x10
#define STA_PRXINTR				0x20

#define	MAX_LUX				32768

/* ioctl numbers */
#define TAOS_IOCTL_MAGIC		0XCF
#define TAOS_IOCTL_ALS_ON		_IO(TAOS_IOCTL_MAGIC, 1)
#define TAOS_IOCTL_ALS_OFF		_IO(TAOS_IOCTL_MAGIC, 2)
#define TAOS_IOCTL_ALS_DATA		_IOR(TAOS_IOCTL_MAGIC, 3, short)
#define TAOS_IOCTL_ALS_CALIBRATE	_IO(TAOS_IOCTL_MAGIC, 4)
#define TAOS_IOCTL_CONFIG_GET		_IOR(TAOS_IOCTL_MAGIC, 5, struct taos_cfg)
#define TAOS_IOCTL_CONFIG_SET		_IOW(TAOS_IOCTL_MAGIC, 6, struct taos_cfg)
#define TAOS_IOCTL_PROX_ON		_IO(TAOS_IOCTL_MAGIC, 7)
#define TAOS_IOCTL_PROX_OFF		_IO(TAOS_IOCTL_MAGIC, 8)
#define TAOS_IOCTL_PROX_DATA		_IOR(TAOS_IOCTL_MAGIC, 9, struct taos_prox_info)
#define TAOS_IOCTL_PROX_EVENT		_IO(TAOS_IOCTL_MAGIC, 10)
#define TAOS_IOCTL_PROX_CALIBRATE	_IO(TAOS_IOCTL_MAGIC, 11)

/* power control */
#define ON              1
#define OFF				0

/* sensor type */
#define LIGHT           0
#define PROXIMITY		1
#define ALL				2

/* 16 level for premium model*/
typedef enum t_light_state
{
	LIGHT_DIM   = 0,
	LIGHT_LEVEL1   = 1,
	LIGHT_LEVEL2   = 2,
	LIGHT_LEVEL3   = 3,
	LIGHT_LEVEL4   = 4,
	LIGHT_LEVEL5   = 5,
	LIGHT_LEVEL6   = 6,
	LIGHT_LEVEL7   = 7,
	LIGHT_LEVEL8   = 8,
	LIGHT_LEVEL9   = 9,
	LIGHT_LEVEL10   = 10,
	LIGHT_LEVEL11   = 11,
	LIGHT_LEVEL12   = 12,
	LIGHT_LEVEL13   = 13,	
	LIGHT_LEVEL14   = 14,		
	LIGHT_LEVEL15   = 15,	
	LIGHT_LEVEL16   = 16,	
	LIGHT_INIT  = 17,	
}state_type;

/* proximity data */
struct taos_prox_info {
	u16 prox_ch0;
	u16 prox_data;
	int prox_event;
};

typedef enum
{
	TAOS_ALS_CLOSED = 0,
	TAOS_ALS_OPENED = 1,
} TAOS_ALS_FOPS_STATUS;

typedef enum
{
	TAOS_PRX_CLOSED = 0,
	TAOS_PRX_OPENED = 1,
} TAOS_PRX_FOPS_STATUS;

typedef enum
{
	TAOS_PRX_DIST_UNKNOWN = 0,
	TAOS_PRX_DIST_IN = 1,
	TAOS_PRX_DIST_OUT = 2
} TAOS_PRX_DISTANCE_STATUS;

typedef enum
{
	TAOS_CHIP_UNKNOWN = 0,
	TAOS_CHIP_WORKING = 1,
	TAOS_CHIP_SLEEP = 2
} TAOS_CHIP_WORKING_STATUS;


