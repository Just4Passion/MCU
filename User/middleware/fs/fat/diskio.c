/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2025        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/
#include <stdio.h>

#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"

#include "kservice.h"
#include "drv_spi_flash.h"
#include "drv_rtc.h"

#include "ff.h"			/* Basic definitions of FatFs */
#include "diskio.h"		/* Declarations FatFs MAI */

/* Example: Declarations of the platform and disk functions in the project */
//#include "platform.h"

/* Example: Mapping of physical drive number for each drive */
//#define DEV_FLASH	0	/* Map FTL to physical drive 0 */
//#define DEV_MMC		1	/* Map MMC/SD card to physical drive 1 */
//#define DEV_USB		2	/* Map USB MSD to physical drive 2 */
#define DEV_SD_CARD 0
#define DEV_SPI_FLASH 1

#define DEV_DISK_OP_TIMEOUT		1000


dy_device_t *g_spi_flash = NULL;
dy_device_t *g_rtc = NULL;
/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	int ret = 0;
	DSTATUS stat = STA_NOINIT;
	uint8_t device_id[3] = {0};

	switch (pdrv) {
		#if 0
		case DEV_MMC :
			// translate the reslut code here
			return stat;
		case DEV_USB :
			// translate the reslut code here
			return stat;
		#endif
		case DEV_SPI_FLASH:
			if (NULL == g_spi_flash)
			{
				return STA_NOINIT;
			}
			/*读取设备ID*/
			ret = g_spi_flash->ops->control(g_spi_flash, W25Q128_READ_CHIP_ID, (void*)device_id);
			if (ret != DY_EOK)
			{
				printf("read chip id failed\r\n");
				return STA_NOINIT;
			}
			stat = STA_READY;
			return stat;
		case DEV_SD_CARD:
			return stat;
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	int ret = 0;
	DSTATUS stat = STA_NOINIT;
	uint32_t timeout = DEV_DISK_OP_TIMEOUT;

	switch (pdrv) {
		#if 0
		case DEV_MMC :
			//ret = MMC_disk_initialize();
			// translate the reslut code here
			return stat;
		case DEV_USB :
			//ret = USB_disk_initialize();
			// translate the reslut code here
			return stat;
		#endif
		case DEV_SPI_FLASH:
			g_spi_flash = dy_find_device("flash_16MB");
			if (NULL == g_spi_flash)
			{
				printf("fat: spi_flash not found\r\n");
				return STA_NODISK;
			}
			g_rtc = dy_find_device("rtc");
			if (NULL == g_rtc)
			{
				printf("fat: rtc_dev not found\r\n");
				return STA_NOINIT;
			}
			/*唤醒flash*/
			ret = g_spi_flash->ops->control(g_spi_flash, W25Q128_WAKE_UP, NULL);
			if (ret != DY_EOK)
			{
				printf("fat: spi_flash wakeup failed\r\n");
				return STA_NOINIT;
			}
			/*获取状态*/
			stat = disk_status(DEV_SPI_FLASH);
			return stat;
		case DEV_SD_CARD:
			return stat;
	}
	return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive: 设备物理编号 */
	BYTE *buff,		/* Data buffer to store read data: 数据缓冲区 */
	LBA_t sector,	/* Start sector in LBA: 扇区首地址 */
	UINT count		/* Number of sectors to read: 扇区个数 */
)
{
	int ret = 0;
	DRESULT res = RES_ERROR;
	uint32_t rd_addr = 0;
	uint32_t rd_len = 0;

	switch (pdrv) {
		#if 0
		case DEV_MMC:
			// translate the arguments here
			// result = MMC_disk_read(buff, sector, count);
			// translate the reslut code here
			return res;
		case DEV_USB:
			// translate the arguments here
			// result = USB_disk_read(buff, sector, count);
			// translate the reslut code here
			return res;
		#endif
		case DEV_SPI_FLASH:
			if (NULL == g_spi_flash)
			{
				printf("fat: disk_read spi_flash not init\r\n");
				return RES_PARERR;
			}
			/*扇区偏移6MB, 外部Flash文件系统空间放在SPI Flash后面10M空间*/
			/*W25Q128FV: 每个扇区4KB, 共有16MB字节空间*/
			/*后部分10MB给FatFs使用 6 * 1024KB / 4KB = 6 * 256 = 1536 sector */
			sector += 1536;
			rd_addr = (sector << 12);		//一个扇区4KB, 2的12次方
			rd_len = (count << 12);
			/*扇区首地址, 然后读取扇区的个数*/

			/*设置要读取的地址*/
			g_spi_flash->ops->control(g_spi_flash, W25Q128_SET_MEM_ADDR, (void*)&rd_addr);
			/*设置要读取的数据*/
			ret = g_spi_flash->ops->read(g_spi_flash, (void*)buff, rd_len);
			if (ret != rd_len)
			{
				printf("fat: disk_read error\r\n");
				return RES_ERROR;
			}
			return RES_OK;
		case DEV_SD_CARD:
			return res;
	}
	return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	int ret = 0;
	DRESULT res = RES_PARERR;
	uint32_t wr_addr = 0;
	uint32_t wr_len = 0;

	switch (pdrv) {
		#if 0
		case DEV_MMC :
			// translate the arguments here
			// result = MMC_disk_write(buff, sector, count);
			// translate the reslut code here
			return res;
		case DEV_USB :
			// translate the arguments here
			// result = USB_disk_write(buff, sector, count);
			// translate the reslut code here
			return res;
		#endif
		case DEV_SPI_FLASH:
			if (NULL == g_spi_flash)
			{
				printf("fat: disk_write spi_flash not init\r\n");
				return RES_PARERR;
			}
			/*扇区偏移: W25Q128, 后续10M作为SPI Flash空间*/
			sector += 1536;
			wr_addr = (sector << 12);
			wr_len = (count << 12);

			g_spi_flash->ops->control(g_spi_flash, W25Q128_SET_MEM_ADDR, (void*)&wr_addr);
			ret = g_spi_flash->ops->write(g_spi_flash, (void*)buff, wr_len);
			if (ret != wr_len)
			{
				printf("fat: disk_write error\r\n");
				return RES_ERROR;
			}
			return RES_OK;
		case DEV_SD_CARD:
			return res;
	}

	return RES_PARERR;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res = RES_PARERR;

	switch (pdrv) {
		#if 0
		case DEV_MMC :
			// Process of the command for the MMC/SD card
			return res;
		case DEV_USB :
			// Process of the command the USB drive
			return res;
		#endif
		case DEV_SPI_FLASH:
			switch(cmd)
			{
				/*扇区数量*/
				case GET_SECTOR_COUNT:
					*(DWORD*)buff = 2560;	//10 * 1024/4 = 10 * 256
					break;
				/*扇区大小*/
				case GET_SECTOR_SIZE:
					*(WORD*)buff = 4096;
					break;
				/*同时擦除扇区个数*/
				case GET_BLOCK_SIZE:
					*(DWORD*)buff = 1;
					break;
			}
			res = RES_OK;
			return res;
		case DEV_SD_CARD:
			return res;
	}

	return RES_PARERR;
}

__weak DWORD get_fattime(void)
{
	 /* 返回当前时间戳 */
	 /*最好使用RTC功能来*/
	int ret = 0;
	rtc_date_time date_time;
	if (NULL == g_rtc)
	{
		printf("rtc not init\r\n");
		return 0;
	}
	ret = g_rtc->ops->control(g_rtc, RTC_GET_DATE_TIME, (void*)&date_time);
	if (ret != DY_EOK)
	{
		printf("rtc get time error\r\n");
		return 0;
	}

    return    ((DWORD)(date_time.year - 1980) << 25)  /* Year 2015 */
            | ((DWORD)date_time.month << 21)        /* Month 1 */
            | ((DWORD)date_time.date << 16)        /* Mday 1 */
            | ((DWORD)date_time.hours << 11)        /* Hour 0 */
            | ((DWORD)date_time.minutes << 5)         /* Min 0 */
            | ((DWORD)date_time.seconds >> 1);        /* Sec 0 */
}

