/*
 * param.c
 *
 *
 * COPYRIGHT(C) Jonathan Jason Dennis [Meticulus] 
 *		theonejohnnyd@gmail.com
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include <linux/file.h>
#include <mach/hardware.h>
#include <mach/param.h>

#define DRIVER_AUTHOR "Jonathan Jason Dennis <theonejohnnyd@gmail.com>"
#define DRIVER_DESC   "Generic Samsung Param Driver for Galaxy S devices"

/* This module is just enough to set the reboot mode.
That is all this does. Could fully implement but 
don't see the point. */


#define J4FS_PARTITION "/dev/block/mmcblk0p9"
#define PARAMBLK_OFFSET 	0x99000
/* Each one of these is a 4 byte figure */
#define SERIAL_SPEED_OFFSET	0x9900C
#define LOAD_RAMDISK_OFFSET	0x99014
#define BOOT_DELAY_OFFSET	0x9901C
#define LCD_LEVEL_OFFSET	0x99024
#define SWITCH_SEL_OFFSET	0x9902C
#define PHONE_DBG_ON_OFFSET	0x99034
#define LCD_DIM_LEVEL_OFFSET	0x9903C
#define LCD_DIM_TIME_OFFSET	0x99044
#define MELODY_MODE_OFFSET	0x9904C
#define REBOOT_MODE_OFFSET	0x99054
#define NATION_SELL_OFFSET	0x9905C
#define	LANGUAGE_SEL_OFFSET	0x99064
#define SET_DEF_PARAM_OFFSET	0x9906C
#define PARAM_INT13		0x99074
#define PARAM_INT14		0x9907C
/* length 9? lots of space after */
#define	VERSION			0x99084
//#define CMDLINE		0x?????
#define DELTA_LOC		0x9988C

#define PARAM_FILE_NAME	J4FS_PARTITION

static int reboot_mode = 0;

static bool manual_mode = false;

static int write_int(int offset, void *value)
{
	struct file *filp;
	mm_segment_t fs;

	int ret;
	char *buffer = (char *)value;

	filp = filp_open(PARAM_FILE_NAME, O_RDWR|O_SYNC, 0);

	if (IS_ERR(filp)) {
		pr_err("%s: filp_open failed. (%ld)\n", __FUNCTION__,
				PTR_ERR(filp));

		return -1;
	}

	fs = get_fs();
	set_fs(get_ds());
	
	filp->f_flags |= O_NONBLOCK;
	ret = filp->f_op->llseek(filp, offset, SEEK_SET);
	if(ret < 0) printk("PARAM: seek error");
	ret = filp->f_op->write(filp, buffer, sizeof(int), &filp->f_pos);
	if(ret < 0) printk("PARAM: write error");
	filp->f_flags &= ~O_NONBLOCK;
	
	set_fs(fs);
	filp_close(filp, NULL);
	
	printk("PARAM: wrote %d at address %d\n", *(int *)buffer, offset);
	return ret;
}

static int read_int(int offset)
{
	struct file *filp;
	mm_segment_t fs;

	int ret, err;
	char *buffer = "000";

	filp = filp_open(PARAM_FILE_NAME, O_RDONLY, 0);

	if (IS_ERR(filp)) {
		pr_err("%s: filp_open failed. (%ld)\n", __FUNCTION__,
				PTR_ERR(filp));

		return -1;
	}

	fs = get_fs();
	set_fs(get_ds());
	
	filp->f_flags |= O_NONBLOCK;
	err = filp->f_op->llseek(filp, offset, SEEK_SET);
	if(err < 0) printk("PARAM: seek error");
	err = filp->f_op->read(filp, buffer, sizeof(int), &filp->f_pos);
	if(err < 0) printk("PARAM: read error");
	filp->f_flags &= ~O_NONBLOCK;
	
	set_fs(fs);
	filp_close(filp, NULL);

	ret =  * (int *)buffer;
	
	return ret;
}

void sec_set_param_value(int idx, void *value)
{
	printk("%s: idx=%d value=%d\n", __func__,idx, *(int *)value);

	if(manual_mode) return;

	switch(idx)
	{
		case __SWITCH_SEL:
			write_int(SWITCH_SEL_OFFSET, value);
			break;
		case __REBOOT_MODE:
			write_int(REBOOT_MODE_OFFSET, value);
			break;
	}

}

void sec_get_param_value(int idx, void *value)
{
	int result = -1;

	switch(idx)
	{
		case __SWITCH_SEL:
			result = read_int(SWITCH_SEL_OFFSET);
			break;
		case __REBOOT_MODE:
			result = read_int(REBOOT_MODE_OFFSET);
			break;
	}
	value = &result;

	printk("%s: idx=%d value=%d\n", __func__,idx, *(int *)value);
}

static int set_reboot_mode(const char *val, struct kernel_param *kp)
{
	param_set_int(val,kp);
	manual_mode = true;
	write_int(REBOOT_MODE_OFFSET, &reboot_mode);

	reboot_mode = read_int(REBOOT_MODE_OFFSET);

	return 0;
}

static int __init param_init(void)
{
	reboot_mode = read_int(REBOOT_MODE_OFFSET);
	return 0;
}

late_initcall(param_init);

module_param(manual_mode, bool, 0664);
MODULE_PARM_DESC(manual_mode, "If true, does not process system requests.");

module_param_call(reboot_mode, set_reboot_mode, param_get_int, &reboot_mode, 0664);
MODULE_PARM_DESC(reboot_mode, "Sets the reboot mode. Automatically enables manual_mode.");

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);	
MODULE_DESCRIPTION(DRIVER_DESC);
