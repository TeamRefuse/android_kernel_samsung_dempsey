/* Slave address */
#define MAX17040_SLAVE_ADDR	0x6D

/* Register address */
#define VCELL0_REG			0x02
#define VCELL1_REG			0x03
#define SOC0_REG			0x04
#define SOC1_REG			0x05
#define MODE0_REG			0x06
#define MODE1_REG			0x07
#define RCOMP0_REG			0x0C
#define RCOMP1_REG			0x0D
#define CMD0_REG			0xFE
#define CMD1_REG			0xFF

#if defined(CONFIG_ARIES_NTT)
unsigned int FGPureSOC = 0;
unsigned int prevFGSOC = 0;
unsigned int fg_zero_count = 0;
#endif

int fuel_guage_init = 0;
EXPORT_SYMBOL(fuel_guage_init);

static struct i2c_driver fg_i2c_driver;
static struct i2c_client *fg_i2c_client = NULL;

struct fg_state{
	struct i2c_client	*client;	
};

struct fg_state *fg_state;

static int is_reset_soc = 0;
// [[junghyunseok edit for fuel_int interrupt control of fuel_gauge 20100504
static int rcomp_status;
static int ce_for_fuelgauge = 0;

static int fg_i2c_read(struct i2c_client *client, u8 reg, u8 *data)
{
	int ret;
	u8 buf[1];
	struct i2c_msg msg[2];

	buf[0] = reg; 

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = buf;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buf;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret != 2) 
		return -EIO;

	*data = buf[0];
	
	return 0;
}

static int fg_i2c_write(struct i2c_client *client, u8 reg, u8 *data)
{
	int ret;
	u8 buf[3];
	struct i2c_msg msg[1];

	buf[0] = reg;
	buf[1] = *data;
	buf[2] = *(data + 1);

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 3;
	msg[0].buf = buf;

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret != 1) 
		return -EIO;

	return 0;
}

unsigned int fg_read_vcell(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	u32 vcell = 0;

	if (fg_i2c_read(client, VCELL0_REG, &data[0]) < 0) {
		pr_err("%s: Failed to read VCELL0\n", __func__);
		return -1;
	}
	if (fg_i2c_read(client, VCELL1_REG, &data[1]) < 0) {
		pr_err("%s: Failed to read VCELL1\n", __func__);
		return -1;
	}
	vcell = ((((data[0] << 4) & 0xFF0) | ((data[1] >> 4) & 0xF)) * 125)/100;
	//pr_info("%s: VCELL=%d\n", __func__, vcell);
	return vcell;
}

#if  defined(CONFIG_S5PC110_KEPLER_BOARD) || (defined CONFIG_S5PC110_T959_BOARD) || defined(CONFIG_S5PC110_FLEMING_BOARD) || defined(CONFIG_S5PC110_CELOX_BOARD) || defined(CONFIG_S5PC110_FLEMING_BOARD)
unsigned int fg_read_soc(void)
{
	 struct i2c_client *client = fg_i2c_client;
	 u8 data[2];
	 int FGPureSOC = 0;
	 int FGAdjustSOC = 0;
	 int FGSOC = 0;

	 if(fg_i2c_client==NULL)
		  return -1;
	 if (fg_i2c_read(client, SOC0_REG, &data[0]) < 0) {
		  pr_err("%s: Failed to read SOC0\n", __func__);
		  return -1;
	 }
	 if (fg_i2c_read(client, SOC1_REG, &data[1]) < 0) {
		  pr_err("%s: Failed to read SOC1\n", __func__);
		  return -1;
	 }
 
	 // calculating soc
	 FGPureSOC = data[0]*100+((data[1]*100)/256);
	 	 
	 if(ce_for_fuelgauge){
		 FGAdjustSOC = ((FGPureSOC - 130)*10000)/9720; // (FGPureSOC-EMPTY(1.2))/(FULL-EMPTY(?))*100
	 }
	 else{
		 FGAdjustSOC = ((FGPureSOC - 130)*10000)/9430; // (FGPureSOC-EMPTY(1.2))/(FULL-EMPTY(?))*100
	 }

//	 printk("\n[FUEL] PSOC = %d, ASOC = %d\n", FGPureSOC, FGAdjustSOC);
	 
	 // rounding off and Changing to percentage.
	 FGSOC=FGAdjustSOC/100;

	if((FGSOC==4)&&FGAdjustSOC%100 >= 80){
		FGSOC+=1;
	 	}
	 if((FGSOC== 0)&&(FGAdjustSOC>0)){
	       FGSOC = 1;  
	 	}
	 if(FGAdjustSOC <= 0){
	       FGSOC = 0;  	 	
	 	}
	 if(FGSOC>=100)
	 {
		  FGSOC=100;
	 }
	 
 return FGSOC;
 
}

#elif (defined CONFIG_S5PC110_VIBRANTPLUS_BOARD)
unsigned int fg_read_soc(void)
{
	 struct i2c_client *client = fg_i2c_client;
	 u8 data[2];
	 int FGPureSOC = 0;
	 int FGAdjustSOC = 0;
	 int FGSOC = 0;

	 if(fg_i2c_client==NULL)
		  return -1;
	 if (fg_i2c_read(client, SOC0_REG, &data[0]) < 0) {
		  pr_err("%s: Failed to read SOC0\n", __func__);
		  return -1;
	 }
	 if (fg_i2c_read(client, SOC1_REG, &data[1]) < 0) {
		  pr_err("%s: Failed to read SOC1\n", __func__);
		  return -1;
	 }
 
	 // calculating soc
	 FGPureSOC = data[0]*100+((data[1]*100)/256);
	 	 
		FGAdjustSOC = ((FGPureSOC - 30)*10000)/9660; // empty 0.3   // (FGPureSOC-EMPTY(1.2))/(FULL-EMPTY(?))*100

	 // rounding off and Changing to percentage.
	 FGSOC=FGAdjustSOC/100;

	//if((FGSOC==4)&&FGAdjustSOC%100 >= 80){
	//	FGSOC+=1;
	// 	}
	 //if((FGSOC== 0)&&(FGAdjustSOC>0)){
	 if((FGSOC== 0)&&(FGAdjustSOC>=10)){
	       FGSOC = 1;  
	 	}
	 if(FGAdjustSOC <= 0){
	       FGSOC = 0;  	 	
	 	}
	 if(FGSOC>=100)
	 {
		  FGSOC=100;
	 }
	 
 return FGSOC;
 
}

#elif (defined CONFIG_S5PC110_HAWK_BOARD)
unsigned int fg_read_soc(void)
{
	 struct i2c_client *client = fg_i2c_client;
	 u8 data[2];
	 int FGPureSOC = 0;
	 int FGAdjustSOC = 0;
	 int FGSOC = 0;

	 if(fg_i2c_client==NULL)
		  return -1;
	 if (fg_i2c_read(client, SOC0_REG, &data[0]) < 0) {
		  pr_err("%s: Failed to read SOC0\n", __func__);
		  return -1;
	 }
	 if (fg_i2c_read(client, SOC1_REG, &data[1]) < 0) {
		  pr_err("%s: Failed to read SOC1\n", __func__);
		  return -1;
	 }
 
	 // calculating soc
	 FGPureSOC = data[0]*100+((data[1]*100)/256);
	 	 
	// if(ce_for_fuelgauge){
	//	 FGAdjustSOC = ((FGPureSOC - 130)*10000)/9720; // (FGPureSOC-EMPTY(1.2))/(FULL-EMPTY(?))*100
		FGAdjustSOC = ((FGPureSOC - 80)*10000)/9820; // (FGPureSOC-EMPTY(1.0))/(FULL-EMPTY(?))*100
	// }
	// else{
	//	 FGAdjustSOC = ((FGPureSOC - 80)*10000)/9430; // (FGPureSOC-EMPTY(1.2))/(FULL-EMPTY(?))*100
	// }

//	 printk("\n[FUEL] PSOC = %d, ASOC = %d\n", FGPureSOC, FGAdjustSOC);
	 
	 // rounding off and Changing to percentage.
	 FGSOC=FGAdjustSOC/100;

	//if((FGSOC==4)&&FGAdjustSOC%100 >= 80){
	//	FGSOC+=1;
	// 	}
	 //if((FGSOC== 0)&&(FGAdjustSOC>0)){
	 if((FGSOC== 0)&&(FGAdjustSOC>=10)){
	       FGSOC = 1;  
	 	}
	 if(FGAdjustSOC <= 0){
	       FGSOC = 0;  	 	
	 	}
	 if(FGSOC>=100)
	 {
		  FGSOC=100;
	 }
	 
 return FGSOC;
 
}

#elif (defined CONFIG_S5PC110_DEMPSEY_BOARD)
unsigned int fg_read_soc(void)
{
	 struct i2c_client *client = fg_i2c_client;
	 u8 data[2];
	 int FGPureSOC = 0;
	 int FGAdjustSOC = 0;
	 int FGSOC = 0;

	 if(fg_i2c_client==NULL)
		  return -1;
	 if (fg_i2c_read(client, SOC0_REG, &data[0]) < 0) {
		  pr_err("%s: Failed to read SOC0\n", __func__);
		  return -1;
	 }
	 if (fg_i2c_read(client, SOC1_REG, &data[1]) < 0) {
		  pr_err("%s: Failed to read SOC1\n", __func__);
		  return -1;
	 }
 
	 // calculating soc
	 FGPureSOC = data[0]*100+((data[1]*100)/256);
	 	 
		FGAdjustSOC = ((FGPureSOC - 50)*10000)/9550; // empty 0.5   // (FGPureSOC-EMPTY(2))/(FULL-EMPTY2))*100

	 //printk("\n[FUEL] PSOC = %d, ASOC = %d\n", FGPureSOC, FGAdjustSOC);
	 // rounding off and Changing to percentage.
	 FGSOC=FGAdjustSOC/100;

	//if((FGSOC==4)&&FGAdjustSOC%100 >= 80){
	//	FGSOC+=1;
	// 	}
	 //if((FGSOC== 0)&&(FGAdjustSOC>0)){
	 if((FGSOC== 0)&&(FGAdjustSOC>=10)){
	       FGSOC = 1;  
	 	}
	 if(FGAdjustSOC <= 0){
	       FGSOC = 0;  	 	
	 	}
	 if(FGSOC>=100)
	 {
		  FGSOC=100;
	 }
	 
 return FGSOC;
 
}

#else
unsigned int fg_read_soc(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 data[2];
	unsigned int FGPureSOC = 0;
	unsigned int FGAdjustSOC = 0;
	unsigned int FGSOC = 0;

	if(fg_i2c_client==NULL)
		return -1;

	if (fg_i2c_read(client, SOC0_REG, &data[0]) < 0) {
		pr_err("%s: Failed to read SOC0\n", __func__);
		return -1;
	}
	if (fg_i2c_read(client, SOC1_REG, &data[1]) < 0) {
		pr_err("%s: Failed to read SOC1\n", __func__);
		return -1;
	}
	
//	pr_info("%s: data[0]=%d, data[1]=%d\n", __func__, data[0], ((data[1]*100)/256));
	
	// calculating soc
	FGPureSOC = data[0]*100+((data[1]*100)/256);

#if defined(CONFIG_ARIES_NTT)

#if 1 /* test7, DF06, change the rcomp to C0 */
	if(FGPureSOC >= 60)
	{
		if(FGPureSOC >= 460)
		{
			FGAdjustSOC = (FGPureSOC - 460)*8650/8740 + 1350;
		}
		else
		{
			FGAdjustSOC = (FGPureSOC - 60)*1350/400;
		}

		if(FGAdjustSOC < 100)
			FGAdjustSOC = 100; //1%
	}
	else
	{
		FGAdjustSOC = 0; //0%
	}
#endif

	// rounding off and Changing to percentage.
	FGSOC=FGAdjustSOC/100;

	if(FGAdjustSOC%100 >= 50 )
	{
		FGSOC+=1;
	}

	if(FGSOC>=100)
	{
		FGSOC=100;
	}

	/* we judge real 0% after 3 continuous counting */
	if(FGSOC == 0)
	{
		fg_zero_count++;

		if(fg_zero_count >= 3)
		{
			FGSOC = 0;
			fg_zero_count = 0;
		}
		else
		{
			FGSOC = prevFGSOC;
		}
	}
	else
	{
		fg_zero_count=0;
	}

	prevFGSOC = FGSOC;

#else // CONFIG_ARIES_NTT

	if(FGPureSOC >= 100)
	{
		FGAdjustSOC = FGPureSOC;
	}
	else
	{
		if(FGPureSOC >= 70)
			FGAdjustSOC = 100; //1%
		else
			FGAdjustSOC = 0; //0%
	}

	// rounding off and Changing to percentage.
	FGSOC=FGAdjustSOC/100;

	if(FGAdjustSOC%100 >= 50 )
	{
		FGSOC+=1;
	}

	if(FGSOC>=26)
	{
		FGSOC+=4;
	}
	else
	{
		FGSOC=(30*FGAdjustSOC)/26/100;
	}

	if(FGSOC>=100)
	{
		FGSOC=100;
	}

#endif // CONFIG_ARIES_NTT

	return FGSOC;
	
}
#endif

unsigned int fg_reset_soc(void)
{
	struct i2c_client *client = fg_i2c_client;
	u8 rst_cmd[2];
	s32 ret = 0;

	is_reset_soc = 1;
	/* Quick-start */
	rst_cmd[0] = 0x40;
	rst_cmd[1] = 0x00;

	ret = fg_i2c_write(client, MODE0_REG, rst_cmd);
	if (ret)
		pr_info("%s: failed reset SOC(%d)\n", __func__, ret);

	msleep(500);
	is_reset_soc = 0;
	return ret;
}


// [[junghyunseok edit for fuel_int interrupt control of fuel_gauge 20100504
void fuel_gauge_rcomp(int mode)
{
	struct i2c_client *client = fg_i2c_client;
	u8 rst_cmd[2];
	s32 ret = 0;

	printk("fuel_gauge_rcomp %d\n",mode);
	
#if defined(CONFIG_S5PC110_KEPLER_BOARD) || (defined CONFIG_S5PC110_T959_BOARD) || defined(CONFIG_S5PC110_FLEMING_BOARD)  || defined(CONFIG_S5PC110_DEMPSEY_BOARD) || defined(CONFIG_S5PC110_CELOX_BOARD) || (defined CONFIG_S5PC110_HAWK_BOARD) || (defined CONFIG_S5PC110_VIBRANTPLUS_BOARD)	
	#if defined(CONFIG_KEPLER_VER_B2) || defined(CONFIG_T959_VER_B5)
		switch (mode) {
			case FUEL_INT_1ST:
				rst_cmd[0] = 0xD0;
				rst_cmd[1] = 0x10; // 15%
				rcomp_status = 0;
				break;		
			case FUEL_INT_2ND:
				rst_cmd[0] = 0xD0;
				rst_cmd[1] = 0x1A; // 5%
				rcomp_status = 1;
				break;
			case FUEL_INT_3RD:
				rst_cmd[0] = 0xD0;
				rst_cmd[1] = 0x1E; // 1%
				rcomp_status = 2;
				break;
// [[ junghyunseok edit for exception code 20100511		
			default:
				rst_cmd[0] = 0xD0;
				rst_cmd[1] = 0x1E; // 1%
				rcomp_status = 2;				
				break;		
// ]] junghyunseok edit for exception code 20100511
		}
	#elif defined(CONFIG_S5PC110_HAWK_BOARD)
		switch (mode) {
			case FUEL_INT_1ST:
				rst_cmd[0] = 0xB0;
				rst_cmd[1] = 0x10; // 15%
				rcomp_status = 0;
				break;		
			case FUEL_INT_2ND:
				rst_cmd[0] = 0xB0;
				rst_cmd[1] = 0x1A; // 5%
				rcomp_status = 1;
				break;
			case FUEL_INT_3RD:
				rst_cmd[0] = 0xB0;
				rst_cmd[1] = 0x1F; // 1%
				rcomp_status = 2;
				break;
// [[ junghyunseok edit for exception code 20100511		
			default:
				rst_cmd[0] = 0xB0;
				rst_cmd[1] = 0x1F; // 1%
				rcomp_status = 2;				
				break;		
// ]] junghyunseok edit for exception code 20100511
		}
	#elif defined(CONFIG_S5PC110_VIBRANTPLUS_BOARD)
		switch (mode) {
			case FUEL_INT_1ST:
				rst_cmd[0] = 0xD0;
				rst_cmd[1] = 0x10; // 15%
				rcomp_status = 0;
				break;		
			case FUEL_INT_2ND:
				rst_cmd[0] = 0xD0;
				rst_cmd[1] = 0x1A; // 5%
				rcomp_status = 1;
				break;
			case FUEL_INT_3RD:
				rst_cmd[0] = 0xD0;
				rst_cmd[1] = 0x1F; // 1%
				rcomp_status = 2;
				break;
// [[ junghyunseok edit for exception code 20100511		
			default:
				rst_cmd[0] = 0xD0;
				rst_cmd[1] = 0x1F; // 1%
				rcomp_status = 2;				
				break;		
// ]] junghyunseok edit for exception code 20100511
		}
	#elif defined(CONFIG_S5PC110_DEMPSEY_BOARD)
		switch (mode) {
			case FUEL_INT_1ST:
				rst_cmd[0] = 0xD0;
				rst_cmd[1] = 0x10; // 15%
				rcomp_status = 0;
				break;		
			case FUEL_INT_2ND:
				rst_cmd[0] = 0xD0;
				rst_cmd[1] = 0x1A; // 5%
				rcomp_status = 1;
				break;
			case FUEL_INT_3RD:
				rst_cmd[0] = 0xD0;
				rst_cmd[1] = 0x1F; // 1%
				rcomp_status = 2;
				break;
// [[ junghyunseok edit for exception code 20100511		
			default:
				rst_cmd[0] = 0xD0;
				rst_cmd[1] = 0x1F; // 1%
				rcomp_status = 2;				
				break;		
// ]] junghyunseok edit for exception code 20100511
		}
	
	#else	
		rst_cmd[0] = 0xD0;
		rst_cmd[1] = 0x00;
	#endif	
#else
	rst_cmd[0] = 0xB0;
	rst_cmd[1] = 0x00;
#endif

	ret = fg_i2c_write(client, RCOMP0_REG, rst_cmd);
	if (ret)
		pr_info("%s: failed fuel_gauge_rcomp(%d)\n", __func__, ret);
	
	//msleep(500);
}
// ]]junghyunseok edit for fuel_int interrupt control of fuel_gauge 20100504

static int fg_i2c_remove(struct i2c_client *client)
{
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
		return -ENODEV;
	fg_i2c_client = client;
	return 0;
}

static int fg_i2c_probe(struct i2c_client *client,  const struct i2c_device_id *id)
{
	struct fg_state *fg;

	fg = kzalloc(sizeof(struct fg_state), GFP_KERNEL);
	if (fg == NULL) {		
		printk("failed to allocate memory \n");
		return -ENOMEM;
	}
	
	fg->client = client;
	i2c_set_clientdata(client, fg);
	
	/* rest of the initialisation goes here. */
	
	printk("Fuel guage attach success!!!\n");

	fg_i2c_client = client;

	fuel_guage_init = 1;

	return 0;
}

static const struct i2c_device_id fg_device_id[] = {
	{"fuelgauge", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, fg_device_id);

static struct i2c_driver fg_i2c_driver = {
	.driver = {
		.name = "fuelgauge",
		.owner = THIS_MODULE,
	},
	.probe	= fg_i2c_probe,
	.remove	= fg_i2c_remove,
	.id_table	= fg_device_id,
};
