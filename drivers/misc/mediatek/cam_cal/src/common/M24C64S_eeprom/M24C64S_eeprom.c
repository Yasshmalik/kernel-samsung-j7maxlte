/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/*
 * Driver for CAM_CAL
 *
 *
 */

#if 0
/*#include <linux/i2c.h>*/
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
/*#include "kd_camera_hw.h"*/
#include "eeprom.h"
#include "eeprom_define.h"
#include "GT24c32a.h"
#include <asm/system.h>  /* for SMP */
#else

#ifndef CONFIG_MTK_I2C_EXTENSION
#define CONFIG_MTK_I2C_EXTENSION
#endif
#include <linux/i2c.h>
#undef CONFIG_MTK_I2C_EXTENSION
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include "kd_camera_hw.h"
#include "cam_cal.h"
#include "cam_cal_define.h"
#include <linux/delay.h>
#include "M24C64S_eeprom.h"
/*#include <asm/system.h>//for SMP*/
#endif
/* #define EEPROMGETDLT_DEBUG */
#define EEPROM_DEBUG
#ifdef EEPROM_DEBUG
#define EEPROMDB pr_debug
#else
#define EEPROMDB(x, ...)
#endif

/*#define M24C64S_DRIVER_ON 0*/
#ifdef M24C64S_DRIVER_ON
#define EEPROM_I2C_BUSNUM 1
static struct i2c_board_info kd_eeprom_dev __initdata = { I2C_BOARD_INFO("CAM_CAL_DRV", 0xAA >> 1)};

/*******************************************************************************
*
********************************************************************************/
#define EEPROM_ICS_REVISION 1 /* seanlin111208 */
/*******************************************************************************
*
********************************************************************************/
#define EEPROM_DRVNAME "CAM_CAL_DRV"
#define EEPROM_I2C_GROUP_ID 0

/*******************************************************************************
*
********************************************************************************/
/* #define FM50AF_EEPROM_I2C_ID 0x28 */
#define FM50AF_EEPROM_I2C_ID 0xA1


/*******************************************************************************
* define LSC data for M24C08F EEPROM on L10 project
********************************************************************************/
#define SampleNum 221
#define Boundary_Address 256
#define EEPROM_Address_Offset 0xC
#define EEPROM_DEV_MAJOR_NUMBER 226

/* EEPROM READ/WRITE ID */
#define S24CS64A_DEVICE_ID							0xAA



/*******************************************************************************
*
********************************************************************************/


/* 81 is used for V4L driver */
static dev_t g_EEPROMdevno = MKDEV(EEPROM_DEV_MAJOR_NUMBER, 0);
static struct cdev *g_pEEPROM_CharDrv;

static struct class *EEPROM_class;
static atomic_t g_EEPROMatomic;
/* static DEFINE_SPINLOCK(kdeeprom_drv_lock); */
/* spin_lock(&kdeeprom_drv_lock); */
/* spin_unlock(&kdeeprom_drv_lock); */
#endif/*M24C64S_DRIVER_ON*/

#define Read_NUMofEEPROM 2
static struct i2c_client *g_pstI2Cclient;

/*static DEFINE_SPINLOCK(g_EEPROMLock);  for SMP */


/*******************************************************************************
*
********************************************************************************/






/*******************************************************************************
*
********************************************************************************/
/* maximun read length is limited at "I2C_FIFO_SIZE" in I2c-mt6516.c which is 8 bytes */
#if 0
static int iWriteEEPROM(u16 a_u2Addr, u32 a_u4Bytes, u8 *puDataInBytes)
{
	u32 u4Index;
	int i4RetValue;
	char puSendCmd[8] = {
		(char)(a_u2Addr >> 8),
		(char)(a_u2Addr & 0xFF),
		0, 0, 0, 0, 0, 0
	};
	if (a_u4Bytes + 2 > 8) {
		EEPROMDB("[M24C64S] exceed I2c-mt65xx.c 8 bytes limitation (include address 2 Byte)\n");
		return -1;
	}

	for (u4Index = 0; u4Index < a_u4Bytes; u4Index += 1)
		puSendCmd[(u4Index + 2)] = puDataInBytes[u4Index];

	i4RetValue = i2c_master_send(g_pstI2Cclient, puSendCmd, (a_u4Bytes + 2));
	if (i4RetValue != (a_u4Bytes + 2)) {
		EEPROMDB("[M24C64S] I2C write  failed!!\n");
		return -1;
	}
	mdelay(10); /* for tWR singnal --> write data form buffer to memory. */

	/* EEPROMDB("[EEPROM] iWriteEEPROM done!!\n"); */
	return 0;
}
#endif

/* maximun read length is limited at "I2C_FIFO_SIZE" in I2c-mt65xx.c which is 8 bytes */
static int iReadEEPROM(u16 a_u2Addr, u32 ui4_length, u8 *a_puBuff)
{
	int  i4RetValue = 0;
	char puReadCmd[2] = {(char)(a_u2Addr >> 8), (char)(a_u2Addr & 0xFF)};
	struct i2c_msg msg[2];

	/* EEPROMDB("[EEPROM] iReadEEPROM!!\n"); */

	if (ui4_length > 8) {
		EEPROMDB("[M24C64S] exceed I2c-mt65xx.c 8 bytes limitation\n");
		return -1;
	}
#ifdef CONFIG_MTK_I2C_EXTENSION
	spin_lock(&g_EEPROMLock); /* for SMP */
	g_pstI2Cclient->addr = g_pstI2Cclient->addr & (I2C_MASK_FLAG | I2C_WR_FLAG);
	spin_unlock(&g_EEPROMLock); /* for SMP */
#endif
	
	msg[0].addr = g_pstI2Cclient->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = puReadCmd;
	
	msg[1].addr = g_pstI2Cclient->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = ui4_length;
	msg[1].buf = a_puBuff;
	
	i4RetValue = i2c_transfer(g_pstI2Cclient->adapter, msg, 2);
	if (i4RetValue != 2) {
		EEPROMDB("[M24C64S] I2C read data failed!!\n");
		return -1;
	}
	
#ifdef CONFIG_MTK_I2C_EXTENSION
	spin_lock(&g_EEPROMLock); /* for SMP */
	g_pstI2Cclient->addr = g_pstI2Cclient->addr & I2C_MASK_FLAG;
	spin_unlock(&g_EEPROMLock); /* for SMP */
#endif
	/* EEPROMDB("[M24C64S] iReadEEPROM done!!\n"); */
	return 0;
}

static int iReadData(unsigned int  ui4_offset, unsigned int  ui4_length, unsigned char *pinputdata)
{
	int  i4RetValue = 0;
	int  i4ResidueDataLength;
	u32 u4IncOffset = 0;
	u32 u4CurrentOffset;
	u8 *pBuff;
	/* EEPROMDB("[S24EEPORM] iReadData\n" ); */

	if (ui4_offset + ui4_length >= 0x2000) {
		EEPROMDB("[M24C64S] Read Error!! M24C64S not supprt address >= 0x2000!!\n");
		return -1;
	}

	i4ResidueDataLength = (int)ui4_length;
	u4CurrentOffset = ui4_offset;
	pBuff = pinputdata;
	do {
		if (i4ResidueDataLength >= 8) {
			i4RetValue = iReadEEPROM((u16)u4CurrentOffset, 8, pBuff);
			if (i4RetValue != 0) {
				EEPROMDB("[M24C64S] I2C iReadData failed!!\n");
				return -1;
			}
			u4IncOffset += 8;
			i4ResidueDataLength -= 8;
			u4CurrentOffset = ui4_offset + u4IncOffset;
			pBuff = pinputdata + u4IncOffset;
		} else {
			i4RetValue = iReadEEPROM((u16)u4CurrentOffset, i4ResidueDataLength, pBuff);
			if (i4RetValue != 0) {
				EEPROMDB("[M24C64S] I2C iReadData failed!!\n");
				return -1;
			}
			u4IncOffset += 8;
			i4ResidueDataLength -= 8;
			u4CurrentOffset = ui4_offset + u4IncOffset;
			pBuff = pinputdata + u4IncOffset;
			/* break; */
		}
	} while (i4ResidueDataLength > 0);




	return 0;
}


/*******************************************************************************
* for sysfs
********************************************************************************/
#define SYSFSLOG(fmt, arg...) pr_err("[%s] " fmt, __func__, ##arg)
#define OTP_RELOAD 0
#define OLD_EEPROM_MAP_MODULE_ID

// common type and function
typedef enum {
	TYPE_STRING,
	TYPE_UINT,
	TYPE_MODULEID,
} t_loop_datatype;

typedef struct {
	unsigned int offset;
	unsigned int size;
	u8 *data;
	u8 *dataname;
	t_loop_datatype datatype;
} t_loop_data;

static void sprintf_sysfsdata(char* buff, unsigned int bufmaxsize, t_loop_datatype datatype, u8 *data) {
	if(TYPE_STRING == datatype) {
		if(bufmaxsize-1 >= strlen(data))
			snprintf(buff, bufmaxsize, "%s", (u8*)data);
	}
	else if(TYPE_UINT == datatype) {
		if(bufmaxsize-1 >= 10) // strlen("4294967295") == 10
			snprintf(buff, bufmaxsize, "%u", *(unsigned int*)data);
	}
	else if(TYPE_MODULEID == datatype) {
		if(bufmaxsize-1 >= 15)
			snprintf(buff, bufmaxsize, "%c%c%c%c%c%02x%02x%02x%02x%02x",
				data[0], data[1], data[2], data[3], data[4],
				data[5], data[6], data[7], data[8], data[9]);
	}
};

#define OTP_MFR_INFO_LEN 11
#define OTP_MODULEID_LEN 10
#define OTP_AF_CAL_LEN 4
#define OTP_MAX_DATA_STRING_LEN_IN_LOG 100

// rear and front sysfs data
typedef struct {
	u8 mfr_info[OTP_MFR_INFO_LEN+1]; // manufacturer_information
	u8 module_id[OTP_MODULEID_LEN+1];
} t_front_otp_info;

static t_front_otp_info front_otp_info;

static t_loop_data front_loop_data[] = {
	{0x20, OTP_MFR_INFO_LEN, front_otp_info.mfr_info, "manufacturer_information", TYPE_STRING},
#ifdef OLD_EEPROM_MAP_MODULE_ID
	{0xAE, OTP_MODULEID_LEN, front_otp_info.module_id, "module_id", TYPE_MODULEID},
	{0x50, OTP_MODULEID_LEN, front_otp_info.module_id, "module_id", TYPE_MODULEID},
#else
	{0xAE, OTP_MODULEID_LEN, front_otp_info.module_id, "module_id", TYPE_MODULEID},
#endif
};
static unsigned int front_loop_len = sizeof(front_loop_data)/sizeof(front_loop_data[0]);

#ifdef OLD_EEPROM_MAP_MODULE_ID
static int is_valid_module_id(u8 *mId)
{
	int i, is_valid = 1;
	for(i=0; i<(OTP_MODULEID_LEN/2); ++i) {
		if( !( (('0'<=mId[i])&&(mId[i]<='9')) || (('a'<=(mId[i]|0x20))&&((mId[i]|0x20)<='z')) ) ) {
			is_valid = 0;
			break;
		}
	}
	return is_valid;
}
#endif

// update function
static int update_cam_sysfs(char* eeprom_name, t_loop_data *loop_data, unsigned int loop_len)
{
	unsigned int i;

	for(i=0; i<loop_len; ++i) {
		int ret = 0;
		t_loop_data* it = &loop_data[i];
		char data_string_to_print[OTP_MAX_DATA_STRING_LEN_IN_LOG];
#ifdef OLD_EEPROM_MAP_MODULE_ID
		if(0x50 == it->offset) {
			if(is_valid_module_id(it->data))
				continue;
			SYSFSLOG("(%s) retry to init module_id with offset 0x50 instead of 0xAE because of EEPROM MAP change\n", eeprom_name);
		}
#endif
		ret = iReadData(it->offset, it->size, it->data);
		sprintf_sysfsdata(data_string_to_print, OTP_MAX_DATA_STRING_LEN_IN_LOG, it->datatype, it->data);
		SYSFSLOG("(%s) %s to init %s = addr[0x%X],size[%d],val[%s]\n", eeprom_name, ((ret!=0)?"failed":"passed"),
			it->dataname, it->offset, it->size, data_string_to_print);
		if ( ret != 0 )
			return 1;
	}

	return 0;
}

static int sysfs_updated = 0;

#if defined(titan6757_phone_n) && 0
extern int g_is_s5k3m3_open;
#endif
static int is_camera_sensor_opened(void)
{
#if defined(titan6757_phone_n) && 0
	// In J7Max, EEPROM m24c64s can be accessed during s5k3m3 sensor is opened.
	return g_is_s5k3m3_open;
#else
	// In other projects, it's not certain which sensor should be checked.
	return true;
#endif
}

// interface functions
void m24c64s_update_sysfs_information(void)
{
	int isErr = 0;
	if(!OTP_RELOAD && sysfs_updated) {
		return;
	}
	isErr = update_cam_sysfs("M24C64S", front_loop_data, front_loop_len);
	if(!isErr)
		sysfs_updated = 1;
}

void get_front_mfr_info(u8 *data)
{
	if(sysfs_updated == 0)
		SYSFSLOG("(M24C64S) error: all sysfs wasn't updated, mfr info = [%s]\n", front_otp_info.mfr_info);
	memcpy((void *)data, (void *)front_otp_info.mfr_info, OTP_MFR_INFO_LEN);
	data[OTP_MFR_INFO_LEN] = '\0';
}
void get_front_moduleid(u8 *data)
{
	if(sysfs_updated == 0)
		SYSFSLOG("(M24C64S) error: all sysfs wasn't updated, moduleid = [%s]\n", front_otp_info.module_id);
	memcpy((void *)data, (void *)front_otp_info.module_id, OTP_MODULEID_LEN);
	data[OTP_MODULEID_LEN] = '\0';
}
#undef SYSFSLOG
///////////////////////////////////////////////////////////////////////

unsigned int m24c64s_selective_read_region(struct i2c_client *client, unsigned int addr,
	unsigned char *data, unsigned int size)
{
	int retry = 50;

	EEPROMDB("[M24C64S] Start Read\n");
	g_pstI2Cclient = client;

	// EEPROM m24c64s can be accessed during the sensor is opened.
	while(is_camera_sensor_opened()==0) {
		if(retry <= 0) {
			pr_err("[M24C64S] sel_read sensor is not ready. but no more wait\n");
			break;
		}
		msleep(10);
		pr_err("[M24C64S] sel_read sensor is not ready. wait 10ms retry=%d\n", retry);
		retry--;
	}
	m24c64s_update_sysfs_information();

	if (iReadData(addr, size, data) == 0) {
		EEPROMDB("[M24C64S] Read success \n");
		return size;
	} else {
		EEPROMDB("[M24C64S] Read failed \n");
		return 0;
	}
}




/*#define M24C64S_DRIVER_ON 0*/
#ifdef M24C64S_DRIVER_ON

/*******************************************************************************
*
********************************************************************************/
#define NEW_UNLOCK_IOCTL
#ifndef NEW_UNLOCK_IOCTL
static int EEPROM_Ioctl(struct inode *a_pstInode,
			struct file *a_pstFile,
			unsigned int a_u4Command,
			unsigned long a_u4Param)
#else
static long EEPROM_Ioctl(
	struct file *file,
	unsigned int a_u4Command,
	unsigned long a_u4Param
)
#endif
{
	int i4RetValue = 0;
	u8 *pBuff = NULL;
	u8 *pWorkingBuff = NULL;
	stCAM_CAL_INFO_STRUCT *ptempbuf;
	ssize_t writeSize;
	u8 readTryagain = 0;

#ifdef EEPROMGETDLT_DEBUG
	struct timeval ktv1, ktv2;
	unsigned long TimeIntervalUS;
#endif

	int retry = 50;
	while(is_camera_sensor_opened()==0) {
		if(retry <= 0) {
			pr_err("[M24C64S] ioctl sensor is not ready. but no more wait\n");
			break;
		}
		msleep(10);
		pr_err("[M24C64S] ioctl sensor is not ready. wait 10ms retry=%d\n", retry);
		retry--;
	}

	if (_IOC_DIR(a_u4Command) != _IOC_NONE) {
		pBuff = kmalloc(sizeof(stCAM_CAL_INFO_STRUCT), GFP_KERNEL);

		if (pBuff == NULL) {
			EEPROMDB("[M24C64S] ioctl allocate mem failed\n");
			return -ENOMEM;
		}

		if (_IOC_WRITE & _IOC_DIR(a_u4Command)) {
			if (copy_from_user((u8 *) pBuff, (u8 *) a_u4Param, sizeof(stCAM_CAL_INFO_STRUCT))) {
				/* get input structure address */
				kfree(pBuff);
				EEPROMDB("[M24C64S] ioctl copy from user failed\n");
				return -EFAULT;
			}
		}
	}

	ptempbuf = (stCAM_CAL_INFO_STRUCT *)pBuff;
	pWorkingBuff = kmalloc(ptempbuf->u4Length, GFP_KERNEL);
	if (pWorkingBuff == NULL) {
		kfree(pBuff);
		EEPROMDB("[M24C64S] ioctl allocate mem failed\n");
		return -ENOMEM;
	}
	EEPROMDB("[M24C64S] init Working buffer address 0x%8x  command is 0x%8x\n", (u32)pWorkingBuff,
	(u32)a_u4Command);

	if (copy_from_user((u8 *)pWorkingBuff, (u8 *)ptempbuf->pu1Params, ptempbuf->u4Length)) {
		kfree(pBuff);
		kfree(pWorkingBuff);
		EEPROMDB("[M24C64S] ioctl copy from user failed\n");
		return -EFAULT;
	}

	switch (a_u4Command) {
	case CAM_CALIOC_S_WRITE:
		EEPROMDB("[M24C64S] Write CMD\n");
#ifdef EEPROMGETDLT_DEBUG
		do_gettimeofday(&ktv1);
#endif
		i4RetValue = iWriteData((u16)ptempbuf->u4Offset, ptempbuf->u4Length, pWorkingBuff);
#ifdef EEPROMGETDLT_DEBUG
		do_gettimeofday(&ktv2);
		if (ktv2.tv_sec > ktv1.tv_sec)
			TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
		else
			TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;

		EEPROMDB("Write data %d bytes take %lu us\n", ptempbuf->u4Length, TimeIntervalUS);
#endif
		break;
	case CAM_CALIOC_G_READ:
		EEPROMDB("[M24C64S] Read CMD\n");
#ifdef EEPROMGETDLT_DEBUG
		do_gettimeofday(&ktv1);
#endif
		EEPROMDB("[M24C64S] offset %x\n", ptempbuf->u4Offset);
		EEPROMDB("[M24C64S] length %x\n", ptempbuf->u4Length);
		EEPROMDB("[M24C64S] Before read Working buffer address 0x%8x\n", (u32)pWorkingBuff);


		if (ptempbuf->u4Offset == 0x0024C32a) {
			*(u32 *)pWorkingBuff = 0x0124C32a;
		} else {
			readTryagain = 3;
			while (readTryagain > 0) {
				i4RetValue =  iReadDataFromM24C64S((u16)ptempbuf->u4Offset, ptempbuf->u4Length,
				pWorkingBuff);
				EEPROMDB("[M24C64S] error (%d) Read retry (%d)\n", i4RetValue, readTryagain);
				if (i4RetValue != 0)
					readTryagain--;
				else
					readTryagain = 0;
			}


		}
		EEPROMDB("[M24C64S] After read Working buffer data  0x%4x\n", *pWorkingBuff);


#ifdef EEPROMGETDLT_DEBUG
		do_gettimeofday(&ktv2);
		if (ktv2.tv_sec > ktv1.tv_sec)
			TimeIntervalUS = ktv1.tv_usec + 1000000 - ktv2.tv_usec;
		else
			TimeIntervalUS = ktv2.tv_usec - ktv1.tv_usec;

		EEPROMDB("Read data %d bytes take %lu us\n", ptempbuf->u4Length, TimeIntervalUS);
#endif

		break;
	default:
		EEPROMDB("[M24C64S] No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	if (_IOC_READ & _IOC_DIR(a_u4Command)) {
		/* copy data to user space buffer, keep other input paremeter unchange. */
		EEPROMDB("[M24C64S] to user length %d\n", ptempbuf->u4Length);
		EEPROMDB("[M24C64S] to user  Working buffer address 0x%8x\n", (u32)pWorkingBuff);
		if (copy_to_user((u8 __user *) ptempbuf->pu1Params, (u8 *)pWorkingBuff,
		ptempbuf->u4Length)) {
			kfree(pBuff);
			kfree(pWorkingBuff);
			EEPROMDB("[M24C64S] ioctl copy to user failed\n");
			return -EFAULT;
		}
	}

	kfree(pBuff);
	kfree(pWorkingBuff);
	return i4RetValue;
}


static u32 g_u4Opened;
/* #define */
/* Main jobs: */
/* 1.check for device-specified errors, device not ready. */
/* 2.Initialize the device if it is opened for the first time. */
static int EEPROM_Open(struct inode *a_pstInode, struct file *a_pstFile)
{
	int ret = 0;

	EEPROMDB("[M24C64S] EEPROM_Open\n");
	spin_lock(&g_EEPROMLock);
	if (g_u4Opened) {
		spin_unlock(&g_EEPROMLock);
		ret = -EBUSY;
	} else {
		g_u4Opened = 1;
		atomic_set(&g_EEPROMatomic, 0);
	}
	spin_unlock(&g_EEPROMLock);

	return ret;
}

/* Main jobs: */
/* 1.Deallocate anything that "open" allocated in private_data. */
/* 2.Shut down the device on last close. */
/* 3.Only called once on last time. */
/* Q1 : Try release multiple times. */
static int EEPROM_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	spin_lock(&g_EEPROMLock);

	g_u4Opened = 0;

	atomic_set(&g_EEPROMatomic, 0);

	spin_unlock(&g_EEPROMLock);

	return 0;
}

static const struct file_operations g_stEEPROM_fops = {
	.owner = THIS_MODULE,
	.open = EEPROM_Open,
	.release = EEPROM_Release,
	/* .ioctl = EEPROM_Ioctl */
	.unlocked_ioctl = EEPROM_Ioctl
};

#define EEPROM_DYNAMIC_ALLOCATE_DEVNO 1
static inline int RegisterEEPROMCharDrv(void)
{
	struct device *EEPROM_device = NULL;

#if EEPROM_DYNAMIC_ALLOCATE_DEVNO
	if (alloc_chrdev_region(&g_EEPROMdevno, 0, 1, EEPROM_DRVNAME)) {
		EEPROMDB("[M24C64S] Allocate device no failed\n");

		return -EAGAIN;
	}
#else
	if (register_chrdev_region(g_EEPROMdevno, 1, EEPROM_DRVNAME)) {
		EEPROMDB("[M24C64S] Register device no failed\n");

		return -EAGAIN;
	}
#endif

	/* Allocate driver */
	g_pEEPROM_CharDrv = cdev_alloc();

	if (g_pEEPROM_CharDrv == NULL) {
		unregister_chrdev_region(g_EEPROMdevno, 1);

		EEPROMDB("[M24C64S] Allocate mem for kobject failed\n");

		return -ENOMEM;
	}

	/* Attatch file operation. */
	cdev_init(g_pEEPROM_CharDrv, &g_stEEPROM_fops);

	g_pEEPROM_CharDrv->owner = THIS_MODULE;

	/* Add to system */
	if (cdev_add(g_pEEPROM_CharDrv, g_EEPROMdevno, 1)) {
		EEPROMDB("[M24C64S] Attatch file operation failed\n");

		unregister_chrdev_region(g_EEPROMdevno, 1);

		return -EAGAIN;
	}

	EEPROM_class = class_create(THIS_MODULE, "EEPROMdrv");
	if (IS_ERR(EEPROM_class)) {
		int ret = PTR_ERR(EEPROM_class);

		EEPROMDB("Unable to create class, err = %d\n", ret);
		return ret;
	}
	EEPROM_device = device_create(EEPROM_class, NULL, g_EEPROMdevno, NULL, EEPROM_DRVNAME);
	if (IS_ERR(EEPROM_device)) {
		EEPROMDB("Unable to create M24C64S EEPROM device \n");
	}

	return 0;
}

static inline void UnregisterEEPROMCharDrv(void)
{
	/* Release char driver */
	cdev_del(g_pEEPROM_CharDrv);

	unregister_chrdev_region(g_EEPROMdevno, 1);

	device_destroy(EEPROM_class, g_EEPROMdevno);
	class_destroy(EEPROM_class);
}


/* //////////////////////////////////////////////////////////////////// */
#ifndef EEPROM_ICS_REVISION
static int EEPROM_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info);
#elif 0
static int EEPROM_i2c_detect(struct i2c_client *client, struct i2c_board_info *info);
#else
#endif
static int EEPROM_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int EEPROM_i2c_remove(struct i2c_client *);

static const struct i2c_device_id EEPROM_i2c_id[] = {{EEPROM_DRVNAME, 0}, {} };
#if 0 /* test110314 Please use the same I2C Group ID as Sensor */
static unsigned short force[] = {EEPROM_I2C_GROUP_ID, S24CS64A_DEVICE_ID, I2C_CLIENT_END, I2C_CLIENT_END};
#else



#endif
/* static const unsigned short * const forces[] = { force, NULL }; */
/* static struct i2c_client_address_data addr_data = { .forces = forces,}; */


static struct i2c_driver EEPROM_i2c_driver = {
	.probe = EEPROM_i2c_probe,
	.remove = EEPROM_i2c_remove,
	/* .detect = EEPROM_i2c_detect, */
	.driver.name = EEPROM_DRVNAME,
	.id_table = EEPROM_i2c_id,
};

#ifndef EEPROM_ICS_REVISION
static int EEPROM_i2c_detect(struct i2c_client *client, int kind, struct i2c_board_info *info)
{
	strcpy(info->type, EEPROM_DRVNAME);
	return 0;
}
#endif
static int EEPROM_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i4RetValue = 0;

	EEPROMDB("[M24C64S] Attach I2C\n");
	/* spin_lock_init(&g_EEPROMLock); */

	/* get sensor i2c client */
	spin_lock(&g_EEPROMLock); /* for SMP */
	g_pstI2Cclient = client;
	g_pstI2Cclient->addr = S24CS64A_DEVICE_ID >> 1;
	spin_unlock(&g_EEPROMLock); /* for SMP */

	EEPROMDB("[M24C64S] g_pstI2Cclient->addr = 0x%8x\n", g_pstI2Cclient->addr);
	/* Register char driver */
	i4RetValue = RegisterEEPROMCharDrv();

	if (i4RetValue) {
		EEPROMDB("[M24C64S] register char device failed!\n");
		return i4RetValue;
	}


	EEPROMDB("[M24C64S] Attached!!\n");
	return 0;
}

static int EEPROM_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static int EEPROM_probe(struct platform_device *pdev)
{
	return i2c_add_driver(&EEPROM_i2c_driver);
}

static int EEPROM_remove(struct platform_device *pdev)
{
	i2c_del_driver(&EEPROM_i2c_driver);
	return 0;
}

/* platform structure */
static struct platform_driver g_stEEPROM_Driver = {
	.probe      = EEPROM_probe,
	.remove = EEPROM_remove,
	.driver     = {
		.name   = EEPROM_DRVNAME,
		.owner  = THIS_MODULE,
	}
};


static struct platform_device g_stEEPROM_Device = {
	.name = EEPROM_DRVNAME,
	.id = 0,
	.dev = {
	}
};

static int __init EEPROM_i2C_init(void)
{
	i2c_register_board_info(EEPROM_I2C_BUSNUM, &kd_eeprom_dev, 1);
	if (platform_driver_register(&g_stEEPROM_Driver)) {
		EEPROMDB("failed to register M24C64S driver\n");
		return -ENODEV;
	}

	if (platform_device_register(&g_stEEPROM_Device)) {
		EEPROMDB("failed to register M24C64S driver, 2nd time\n");
		return -ENODEV;
	}

	return 0;
}

static void __exit EEPROM_i2C_exit(void)
{
	platform_driver_unregister(&g_stEEPROM_Driver);
}

module_init(EEPROM_i2C_init);
module_exit(EEPROM_i2C_exit);

MODULE_DESCRIPTION("EEPROM driver");
MODULE_AUTHOR("Sean Lin <Sean.Lin@Mediatek.com>");
MODULE_LICENSE("GPL");

#endif
