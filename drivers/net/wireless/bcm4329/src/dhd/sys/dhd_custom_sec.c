#include <typedefs.h>
#include <osl.h>

#include <epivers.h>
#include <bcmutils.h>

#include <bcmendian.h>
#include <dngl_stats.h>
#include <dhd.h>

#include <dhd_dbg.h>

extern int dhdcdc_set_ioctl(dhd_pub_t *dhd, int ifidx, uint cmd, void *buf, uint len);

void sec_dhd_config_pm(dhd_pub_t *dhd, uint power_mode)
{
	struct file *fp      = NULL;
	char* filepath       = "/data/.psm.info";
	char iovbuf[WL_EVENTING_MASK_LEN + 12];

	/* Set PowerSave mode */
	fp = filp_open(filepath, O_RDONLY, 0);
	if(IS_ERR(fp))// the file is not exist
	{
		/* Set PowerSave mode */
		dhdcdc_set_ioctl(dhd, 0, WLC_SET_PM, (char *)&power_mode, sizeof(power_mode));

		fp = filp_open(filepath, O_RDWR | O_CREAT, 0666);
		if(IS_ERR(fp)||(fp==NULL))
		{
			DHD_ERROR(("[WIFI] %s: File open error\n", filepath));
		}
		else
		{
			char buffer[2]   = {1};
			if(fp->f_mode & FMODE_WRITE)
			{
				sprintf(buffer,"1\n");
				fp->f_op->write(fp, (const char *)buffer, sizeof(buffer), &fp->f_pos);
			}
		}
	}
	else
	{
		char buffer[1]   = {0};
		kernel_read(fp, fp->f_pos, buffer, 1);
		if(strncmp(buffer, "1",1)==0)
		{
			/* Set PowerSave mode */
			dhdcdc_set_ioctl(dhd, 0, WLC_SET_PM, (char *)&power_mode, sizeof(power_mode));
		}
		else
		{
			/*Disable Power save features for CERTIFICATION*/
			power_mode = 0;
		 
			/* Set PowerSave mode */
			dhdcdc_set_ioctl(dhd, 0, WLC_SET_PM, (char *)&power_mode, sizeof(power_mode));
		 
			/* Disable MPC */    
			bcm_mkiovar("mpc", (char *)&power_mode, 4, iovbuf, sizeof(iovbuf));
			dhdcdc_set_ioctl(dhd, 0, WLC_SET_VAR, iovbuf, sizeof(iovbuf));

			fp = filp_open(filepath, O_RDWR | O_CREAT, 0666);
			if(IS_ERR(fp)||(fp==NULL))
			{
				DHD_ERROR(("[WIFI] %s: File open error\n", filepath));
			}
			else
			{
				char buffer[2]   = {1};
				if(fp->f_mode & FMODE_WRITE)
				{
					sprintf(buffer,"1\n");
					fp->f_op->write(fp, (const char *)buffer, sizeof(buffer), &fp->f_pos);
				}
			}
		}
	}

	if(fp)
		filp_close(fp, NULL);
}
