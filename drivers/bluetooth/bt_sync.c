#include <asm/delay.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>

static struct mutex read_lock;
static struct mutex write_lock;

struct btsync{
    unsigned char* name;
    unsigned char flags;
   	read_proc_t *read_proc;
	write_proc_t *write_proc;
};

static int btsync_read_proc_btstate(char *page, char **start, off_t offset,
                                              int count, int *eof, void *data);

static int btsync_write_proc_btstate(struct file *file, const char *buffer,
                                            unsigned long count, void *data);

static int btsync_read_proc_fmstate(char *page, char **start, off_t offset,
                                              int count, int *eof, void *data);

static int btsync_write_proc_fmstate(struct file *file, const char *buffer,
                                            unsigned long count, void *data);

struct btsync bts[] = {
    {"bt_state", 0, btsync_read_proc_btstate, btsync_write_proc_btstate},
    {"fm_state", 0, btsync_read_proc_fmstate, btsync_write_proc_fmstate},
    {NULL, 0, (read_proc_t*)NULL, (write_proc_t*)NULL},
};

static int btsync_read_proc_btstate(char *page, char **start, off_t offset,
                                              int count, int *eof, void *data)
{
    int len;

    mutex_lock(&read_lock);
    len = sprintf(page, "%d\n", bts[0].flags);
	  //printk("read btstate %c\n", bts[0].flags);
    mutex_unlock(&read_lock);

    *eof = 1;
    return len;
}

static int btsync_write_proc_btstate(struct file *file, const char *buffer,
                                            unsigned long count, void *data)
{
    char str;

    if (count < 1)
        return -EINVAL;

    if (copy_from_user(&str, buffer, 1))
        return -EFAULT;

    mutex_lock(&write_lock);
    //printk("write btstate %c\n", str);
    bts[0].flags = str - 0x30;
    mutex_unlock(&write_lock);
    return count;
}

static int btsync_read_proc_fmstate(char *page, char **start, off_t offset,
                                              int count, int *eof, void *data)
{
    int len;

    mutex_lock(&read_lock);
    len = sprintf(page, "%d\n", bts[1].flags);
	//printk("read fmstate %c\n", bts[1].flags);
    mutex_unlock(&read_lock);

    *eof = 1;
    return len;
}

static int btsync_write_proc_fmstate(struct file *file, const char *buffer,
                                            unsigned long count, void *data)
{
    char str;

    if (count < 1)
        return -EINVAL;

    if (copy_from_user(&str, buffer, 1))
        return -EFAULT;

    mutex_lock(&write_lock);
    //printk("write fmstate %c\n", str);
    bts[1].flags = str - 0x30;
    mutex_unlock(&write_lock);
    return count;
}

static struct proc_dir_entry *btsync_dir;

static int __init bt_sync_init(void)
{
    int i;
    int retval;
    struct proc_dir_entry *ent;

    mutex_init(&read_lock);
    mutex_init(&write_lock);

    for (i=0; bts[i].name!=NULL; i++) {
        bts[i].flags = 0;
    }

    btsync_dir = proc_mkdir("btsync", NULL);
    if (btsync_dir == NULL) {
        printk("Unable to create /proc/btsync directory");
        return -ENOMEM;
    }

    for (i=0; bts[i].name!=NULL; i++) {
        ent = create_proc_entry(bts[i].name, 0666, btsync_dir);
        if (ent == NULL) {
            printk("Unable to create /proc/%s/%s entry", "btsync", bts[i].name);
            retval = -ENOMEM;
            goto fail;
        }
        ent->read_proc = bts[i].read_proc;
        ent->write_proc = bts[i].write_proc;
    }

    return 0;

fail:
    for (i=0; bts[i].name!=NULL; i++) {
        remove_proc_entry(bts[i].name, btsync_dir);
    }
    remove_proc_entry("btsync", NULL);
    return retval;
}

module_init(bt_sync_init);

static void __exit bt_sync_exit(void)
{
    int i;

    for (i=0; bts[i].name!=NULL; i++) {
        remove_proc_entry(bts[i].name, btsync_dir);
    }
    remove_proc_entry("btsync", NULL);
}

//////////////////////////////////////////////////////////////////////
module_exit(bt_sync_exit);

//////////////////////////////////////////////////////////////////////
MODULE_LICENSE("GPL");
MODULE_AUTHOR("buyunshi@broadcom.com>");
MODULE_DESCRIPTION("\"bt_sync\" Bluetooth process sync for Broadcom solution");
MODULE_VERSION("1.0");

