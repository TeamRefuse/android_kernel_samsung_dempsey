HOW TO BUILD KERNEL 2.6.35 FOR SGH-I997R

1. Visit http://www.codesourcery.com/, download and install Sourcery G++ Lite 2009q3-68 toolchain for ARM EABI.

2. Extract kernel source and move into the top directory.

3. Execute 'make dempseyR_rev00_defconfig'.

4. Execute 'make' or 'make -j<n>' where '<n>' is the number of multiple jobs to be invoked simultaneously.

5. If the kernel is built successfully, you will find following files from the top directory:
	arch/arm/boot/zImage
	crypto/ansi_cprng.ko
	drivers/scsi/scsi_wait_scan.ko
	drivers/net/wireless/bcm4330/dhd.ko
	drivers/misc/vibetonz/vibrator.ko
	drivers/bluetooth/bthid/bthid.ko
