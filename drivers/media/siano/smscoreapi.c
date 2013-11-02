/****************************************************************

 Siano Mobile Silicon, Inc.
 MDTV receiver kernel modules.
 Copyright (C) 2006-2008, Uri Shkolnik

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 ****************************************************************/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#include <linux/firmware.h>
#include <linux/syscalls.h>
#include <asm/byteorder.h>
#include <linux/slab.h> // add by jiabf

#include "smscoreapi.h"
#include "smsendian.h"
#include "sms-cards.h"
#include "siano_fw_3188.h"
//#include "cmmb_fw.h"

#define MAX_GPIO_PIN_NUMBER	31

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
//#define REQUEST_FIRMWARE_SUPPORTED
//#undef REQUEST_FIRMWARE_SUPPORTED
#define DEFAULT_FW_FILE_PATH "/system/etc/firmware/"
#else
//#define DEFAULT_FW_FILE_PATH "/lib/firmware"
#define DEFAULT_FW_FILE_PATH "/system/etc/firmware/"
#endif

// to enable log
int sms_debug =7;
//int sms_debug =0;
// for loopback
char g_LbResBuf[256]={0};

#ifdef ZTE_CMMB_LOOP_TEST
/*add by wangtao for cmmb 607loop 20111114 ++*/
u32 g_607Buf[261121]={0};		 
u32 g_607num=0;
u32 g_607len=0;
/*add by wangtao for cmmb 607loop 20111114 --*/
#endif

module_param_named(debug, sms_debug, int, 0644);
MODULE_PARM_DESC(debug, "set debug level (info=1, adv=2 (or-able))");

//static int default_mode = 4;
static int default_mode = DEVICE_MODE_CMMB;
module_param(default_mode, int, 0644);
MODULE_PARM_DESC(default_mode, "default firmware id (device mode)");


struct smscore_device_notifyee_t {
	struct list_head entry;
	hotplug_t hotplug;
};

struct smscore_idlist_t {
	struct list_head entry;
	int id;
	int data_type;
};

struct smscore_client_t {
	struct list_head entry;
	struct smscore_device_t *coredev;
	void *context;
	struct list_head idlist;
	onresponse_t onresponse_handler;
	onremove_t onremove_handler;
};

struct smscore_device_t {
	struct list_head entry;

	struct list_head clients;
	struct list_head subclients;
	spinlock_t clientslock;

	struct list_head buffers;
	spinlock_t bufferslock;
	int num_buffers;

	void *common_buffer;
	int common_buffer_size;
	dma_addr_t common_buffer_phys;

	void *context;
	struct device *device;

	char devpath[32];
	unsigned long device_flags;

	setmode_t setmode_handler;
	detectmode_t detectmode_handler;
	sendrequest_t sendrequest_handler;
	preload_t preload_handler;
	postload_t postload_handler;

	int mode, modes_supported;

	struct completion version_ex_done, data_download_done, trigger_done;
	struct completion init_device_done, reload_start_done, resume_done;
	struct completion gpio_configuration_done, gpio_set_level_done;
	struct completion gpio_get_level_done;
// for loopback 	
	struct completion loopback_res_done;
// for loopback
	int gpio_get_res;

	int board_id;

	u8 *fw_buf;
	u32 fw_buf_size;

	wait_queue_head_t buffer_mng_waitq;
};





static struct smscore_device_t* panic_core_dev = NULL ;
extern void sms_redownload_firmware_reset(void);

void smscore_panic_print(void)
{
    if(panic_core_dev)
    {
        printk("common_buffer_size  = [0x%x]\n", panic_core_dev-> common_buffer_size) ;
        printk("common_buffer start addr= [0x%x]\n",(unsigned int) panic_core_dev->common_buffer ) ;
        printk("common_buffer end  addr= [0x%x]\n",
                   (unsigned int) (panic_core_dev->common_buffer + panic_core_dev-> common_buffer_size -1)) ;
        printk("common_buffer_phys start addr = [0x%x]\n",(unsigned int) panic_core_dev->common_buffer_phys) ;
        printk("common_buffer_phys end addr = [0x%x]\n",
                  (unsigned int)  ( panic_core_dev->common_buffer_phys+ panic_core_dev-> common_buffer_size -1)) ;
    }
}

// 
// for loopback

int  AdrLoopbackTest( struct smscore_device_t *coredev );

void smscore_set_board_id(struct smscore_device_t *core, int id)
{
	core->board_id = id;
}

int smscore_get_board_id(struct smscore_device_t *core)
{
	return core->board_id;
}

struct smscore_registry_entry_t {
	struct list_head entry;
	char devpath[32];
	int mode;
	enum sms_device_type_st type;
};

static struct list_head g_smscore_notifyees;
static struct list_head g_smscore_devices;
static struct mutex g_smscore_deviceslock;
static struct list_head g_smscore_registry;
static struct mutex g_smscore_registrylock;

static struct smscore_registry_entry_t *smscore_find_registry(char *devpath)
{
	struct smscore_registry_entry_t *entry;
	struct list_head *next;

	kmutex_lock(&g_smscore_registrylock);
	for (next = g_smscore_registry.next; next != &g_smscore_registry; next
			= next->next) {
		entry = (struct smscore_registry_entry_t *) next;
		if (!strcmp(entry->devpath, devpath)) {
			kmutex_unlock(&g_smscore_registrylock);
			return entry;
		}
	}
	entry = /* (struct smscore_registry_entry_t *) */kmalloc(
			sizeof(struct smscore_registry_entry_t), GFP_KERNEL);
	if (entry) {
		entry->mode = default_mode;
		if(strlen(devpath) >= 32)
		{
			sms_err(" strlen(devpath) >= 32\n");
			return NULL;
		}
		strcpy(entry->devpath, devpath);
		list_add(&entry->entry, &g_smscore_registry);
	} else
		sms_err("failed to create smscore_registry.");
	kmutex_unlock(&g_smscore_registrylock);
	return entry;
}

int smscore_registry_getmode(char *devpath)
{
	struct smscore_registry_entry_t *entry;

	entry = smscore_find_registry(devpath);
	if (entry)
		return entry->mode;
	else
		sms_err("No registry found.");

	return default_mode;
}

static enum sms_device_type_st smscore_registry_gettype(char *devpath)
{
	struct smscore_registry_entry_t *entry;

	entry = smscore_find_registry(devpath);
	if (entry)
		return entry->type;
	else
		sms_err("No registry found.");

	return -1;
}

void smscore_registry_setmode(char *devpath, int mode)
{
	struct smscore_registry_entry_t *entry;

	entry = smscore_find_registry(devpath);
	if (entry)
		entry->mode = mode;
	else
		sms_err("No registry found.");
}

static void smscore_registry_settype(char *devpath,
		enum sms_device_type_st type) {
	struct smscore_registry_entry_t *entry;

	entry = smscore_find_registry(devpath);
	if (entry)
		entry->type = type;
	else
		sms_err("No registry found.");
}

static void list_add_locked(struct list_head *new, struct list_head *head,
		spinlock_t *lock) {
	unsigned long flags;

	spin_lock_irqsave(lock, flags);
	list_add(new, head);
	spin_unlock_irqrestore(lock, flags);
}

/**
 * register a client callback that called when device plugged in/unplugged
 * NOTE: if devices exist callback is called immediately for each device
 *
 * @param hotplug callback
 *
 * @return 0 on success, <0 on error.
 */
int smscore_register_hotplug(hotplug_t hotplug)
{
	struct smscore_device_notifyee_t *notifyee;
	struct list_head *next, *first;
	int rc = 0;

	sms_info(" entering... smscore_register_hotplug \n");
	kmutex_lock(&g_smscore_deviceslock);

	notifyee = kmalloc(sizeof(struct smscore_device_notifyee_t),
		GFP_KERNEL);
	if (notifyee) {
		/* now notify callback about existing devices */
		first = &g_smscore_devices;
		for (next = first->next; next != first && !rc;
			next = next->next) {
			struct smscore_device_t *coredev =
				(struct smscore_device_t *) next;
			rc = hotplug(coredev, coredev->device, 1);
		}

		if (rc >= 0) {
			notifyee->hotplug = hotplug;
			list_add(&notifyee->entry, &g_smscore_notifyees);
		} else
			kfree(notifyee);
	} else
		rc = -ENOMEM;

	kmutex_unlock(&g_smscore_deviceslock);

	return rc;
}

/**
 * unregister a client callback that called when device plugged in/unplugged
 *
 * @param hotplug callback
 *
 */
void smscore_unregister_hotplug(hotplug_t hotplug)
{
	struct list_head *next, *first;

	kmutex_lock(&g_smscore_deviceslock);

	first = &g_smscore_notifyees;

	for (next = first->next; next != first;) {
		struct smscore_device_notifyee_t *notifyee =
				(struct smscore_device_notifyee_t *) next;
		next = next->next;

		if (notifyee->hotplug == hotplug) {
			list_del(&notifyee->entry);
			kfree(notifyee);
		}
	}

	kmutex_unlock(&g_smscore_deviceslock);
}

static void smscore_notify_clients(struct smscore_device_t *coredev)
{
	struct smscore_client_t *client;

	/* the client must call smscore_unregister_client from remove handler */
	while (!list_empty(&coredev->clients)) {
		client = (struct smscore_client_t *) coredev->clients.next;
		client->onremove_handler(client->context);
	}
}

static int smscore_notify_callbacks(struct smscore_device_t *coredev,
		struct device *device, int arrival) {
	struct list_head *next, *first;
	int rc = 0;

	/* note: must be called under g_deviceslock */

	first = &g_smscore_notifyees;

	for (next = first->next; next != first; next = next->next) {
		rc = ((struct smscore_device_notifyee_t *) next)->
			 hotplug(coredev, device, arrival);
		if (rc < 0)
			break;
	}

	return rc;
}

static struct smscore_buffer_t *smscore_createbuffer(u8 *buffer,
		void *common_buffer, dma_addr_t common_buffer_phys) {
	struct smscore_buffer_t *cb = kmalloc(sizeof(struct smscore_buffer_t),
			GFP_KERNEL);
	if (!cb) {
		sms_info("kmalloc(...) failed");
		return NULL;
	}

	cb->p = buffer;
	cb->offset_in_common = buffer - (u8 *) common_buffer;
	cb->phys = common_buffer_phys + cb->offset_in_common;

	return cb;
}

/**
 * creates coredev object for a device, prepares buffers,
 * creates buffer mappings, notifies registered hotplugs about new device.
 *
 * @param params device pointer to struct with device specific parameters
 *               and handlers
 * @param coredev pointer to a value that receives created coredev object
 *
 * @return 0 on success, <0 on error.
 */
int smscore_register_device(struct smsdevice_params_t *params,
		struct smscore_device_t **coredev) {
	struct smscore_device_t *dev;
	u8 *buffer;

	sms_info(" entering....smscore_register_device \n");
	dev = kzalloc(sizeof(struct smscore_device_t), GFP_KERNEL);
	if (!dev) {
		sms_info("kzalloc(...) failed");
		return -ENOMEM;
	}

	/* init list entry so it could be safe in smscore_unregister_device */
	INIT_LIST_HEAD(&dev->entry);

	/* init queues */
	INIT_LIST_HEAD(&dev->clients);
	INIT_LIST_HEAD(&dev->buffers);

	/* init locks */
	spin_lock_init(&dev->clientslock);
	spin_lock_init(&dev->bufferslock);

	/* init completion events */
	init_completion(&dev->version_ex_done);
	init_completion(&dev->data_download_done);
	init_completion(&dev->trigger_done);
	init_completion(&dev->init_device_done);
	init_completion(&dev->reload_start_done);
	init_completion(&dev->resume_done);
	init_completion(&dev->gpio_configuration_done);
	init_completion(&dev->gpio_set_level_done);
	init_completion(&dev->gpio_get_level_done);
// for loopback test
     	init_completion(&dev->loopback_res_done);
	init_waitqueue_head(&dev->buffer_mng_waitq);

	/* alloc common buffer */
	sms_info(" entering...alloc common buffer \n");
	dev->common_buffer_size = params->buffer_size * params->num_buffers;
	dev->common_buffer = dma_alloc_coherent(NULL, dev->common_buffer_size,
			&dev->common_buffer_phys, GFP_KERNEL | GFP_DMA);
	if (!dev->common_buffer) {
		smscore_unregister_device(dev);
		return -ENOMEM;
	}


	/* prepare dma buffers */
	sms_info(" entering...prepare dma buffers \n");


	for (buffer = dev->common_buffer ; dev->num_buffers <
			params->num_buffers ; dev->num_buffers++, buffer
			+= params->buffer_size) {
		struct smscore_buffer_t *cb = smscore_createbuffer(buffer,
				dev->common_buffer, dev->common_buffer_phys);
		if (!cb) {
			smscore_unregister_device(dev);
			return -ENOMEM;
		}

		smscore_putbuffer(dev, cb);
	}

	sms_info("allocated %d buffers", dev->num_buffers);

	dev->mode = DEVICE_MODE_NONE;
	dev->context = params->context;
	dev->device = params->device;
	dev->setmode_handler = params->setmode_handler;
	dev->detectmode_handler = params->detectmode_handler;
	dev->sendrequest_handler = params->sendrequest_handler;
	dev->preload_handler = params->preload_handler;
	dev->postload_handler = params->postload_handler;

	dev->device_flags = params->flags;
	strcpy(dev->devpath, params->devpath);

	smscore_registry_settype(dev->devpath, params->device_type);

	/* add device to devices list */
	kmutex_lock(&g_smscore_deviceslock);
	list_add(&dev->entry, &g_smscore_devices);
	kmutex_unlock(&g_smscore_deviceslock);

	*coredev = dev;
        panic_core_dev = dev ;
	sms_info("device %p created", dev);

	return 0;
}

/**
 * sets initial device mode and notifies client hotplugs that device is ready
 *
 * @param coredev pointer to a coredev object returned by
 * 		  smscore_register_device
 *
 * @return 0 on success, <0 on error.
 */
int smscore_start_device(struct smscore_device_t *coredev)
{
	int rc;

#ifdef REQUEST_FIRMWARE_SUPPORTED
	rc = smscore_set_device_mode(coredev, smscore_registry_getmode(
			coredev->devpath));
	if (rc < 0) {
		sms_info("set device mode faile , rc %d", rc);
		return rc;
	}
#endif

	kmutex_lock(&g_smscore_deviceslock);

	rc = smscore_notify_callbacks(coredev, coredev->device, 1);

	sms_info("device %p started, rc %d", coredev, rc);

	kmutex_unlock(&g_smscore_deviceslock);

	return rc;
}

static int smscore_sendrequest_and_wait(struct smscore_device_t *coredev,
		void *buffer, size_t size, struct completion *completion) {
	int rc = coredev->sendrequest_handler(coredev->context, buffer, size);
	if (rc < 0) {
		sms_info("sendrequest returned error %d", rc);
		return rc;
	}

	return wait_for_completion_timeout(completion,
			msecs_to_jiffies(10000)) ? 0 : -ETIME;
}

static int smscore_load_firmware_family2(struct smscore_device_t *coredev,
		void *buffer, size_t size) {
	struct SmsFirmware_ST *firmware = (struct SmsFirmware_ST *) buffer;
	struct SmsMsgHdr_ST *msg;
	u32 mem_address;
	u8 *payload = firmware->Payload;
	int rc = 0;

	firmware->StartAddress = le32_to_cpu(firmware->StartAddress);
	firmware->Length = le32_to_cpu(firmware->Length);

	mem_address = firmware->StartAddress;

	sms_info("loading FW to addr 0x%x size %d",
			mem_address, firmware->Length);
	if (coredev->preload_handler) {
		rc = coredev->preload_handler(coredev->context);
		if (rc < 0)
			return rc;
	}

	/* PAGE_SIZE buffer shall be enough and dma aligned */
	msg = kmalloc(PAGE_SIZE, GFP_KERNEL | GFP_DMA);
	if (!msg)
		return -ENOMEM;

	if (coredev->mode != DEVICE_MODE_NONE) {
		sms_debug("sending reload command.");
		SMS_INIT_MSG(msg, MSG_SW_RELOAD_START_REQ,
				sizeof(struct SmsMsgHdr_ST));
		smsendian_handle_tx_message((struct SmsMsgHdr_ST *)msg);
		rc = smscore_sendrequest_and_wait(coredev, msg, msg->msgLength,
				&coredev->reload_start_done);
		mem_address = *(u32 *) &payload[20];
	}
       sms_info("start download fw  helike");
	while (size && rc >= 0) {
		struct SmsDataDownload_ST *DataMsg =
				(struct SmsDataDownload_ST *) msg;
		int payload_size = min((int)size, SMS_MAX_PAYLOAD_SIZE);

		SMS_INIT_MSG(msg, MSG_SMS_DATA_DOWNLOAD_REQ,
				(u16) (sizeof(struct SmsMsgHdr_ST) +
						sizeof(u32) + payload_size));

		DataMsg->MemAddr = mem_address;
		memcpy(DataMsg->Payload, payload, payload_size);

		smsendian_handle_tx_message((struct SmsMsgHdr_ST *)msg);
             #if 0//del by jiabf
		if ((coredev->device_flags & SMS_ROM_NO_RESPONSE) &&
				(coredev->mode	== DEVICE_MODE_NONE))
			rc = coredev->sendrequest_handler(coredev->context,
                                DataMsg,
					DataMsg->xMsgHeader.msgLength);
		else
             #endif
			rc = smscore_sendrequest_and_wait(coredev, DataMsg,
					DataMsg->xMsgHeader.msgLength,
					&coredev->data_download_done);

		payload += payload_size;
		size -= payload_size;
		mem_address += payload_size;
	}

	if (rc >= 0) {
		if (coredev->mode == DEVICE_MODE_NONE) {
			struct SmsMsgData_ST *TriggerMsg =
					(struct SmsMsgData_ST *) msg;

			SMS_INIT_MSG(msg, MSG_SMS_SWDOWNLOAD_TRIGGER_REQ,
					sizeof(struct SmsMsgHdr_ST) +
					sizeof(u32) * 5);

			TriggerMsg->msgData[0] = firmware->StartAddress;
			/* Entry point */
			TriggerMsg->msgData[1] = 5; /* Priority */
			TriggerMsg->msgData[2] = 0x200; /* Stack size */
			TriggerMsg->msgData[3] = 0; /* Parameter */
			TriggerMsg->msgData[4] = 4; /* Task ID */

			smsendian_handle_tx_message((struct SmsMsgHdr_ST *)msg);
			if (coredev->device_flags & SMS_ROM_NO_RESPONSE) {
				rc = coredev->sendrequest_handler(coredev->
					context, TriggerMsg,
					TriggerMsg->xMsgHeader.msgLength);
				msleep(100);
			} else
				rc = smscore_sendrequest_and_wait(coredev,
					TriggerMsg,
					TriggerMsg->xMsgHeader.msgLength,
					&coredev->trigger_done);
		} else {
			SMS_INIT_MSG(msg, MSG_SW_RELOAD_EXEC_REQ,
					sizeof(struct SmsMsgHdr_ST));
			smsendian_handle_tx_message((struct SmsMsgHdr_ST *)msg);
			rc = coredev->sendrequest_handler(coredev->context, msg,
					msg->msgLength);
		}
		msleep(500);
	}

	sms_debug("rc=%d, postload=%p ", rc, coredev->postload_handler);

	kfree(msg);

	return ((rc >= 0) && coredev->postload_handler) ?
			coredev->postload_handler(coredev->context) : rc;
}

/**
 * loads specified firmware into a buffer and calls device loadfirmware_handler
 *
 * @param coredev pointer to a coredev object returned by
 *                smscore_register_device
 * @param filename null-terminated string specifies firmware file name
 * @param loadfirmware_handler device handler that loads firmware
 *
 * @return 0 on success, <0 on error.
 */
static int smscore_load_firmware_from_file(struct smscore_device_t *coredev,
		char *filename, loadfirmware_t loadfirmware_handler) {
	int rc = -ENOENT;
	u8 *fw_buf;
	u32 fw_buf_size;

#ifdef REQUEST_FIRMWARE_SUPPORTED
	const struct firmware *fw;

	if (loadfirmware_handler == NULL && !(coredev->device_flags
			& SMS_DEVICE_FAMILY2))
		return -EINVAL;

	rc = request_firmware(&fw, filename, coredev->device);
	if (rc < 0) {
		sms_info("failed to open \"%s\"", filename);
		return rc;
	}
	sms_info("read FW %s, size=%zd", filename, fw->size);
	fw_buf = kmalloc(ALIGN(fw->size, SMS_ALLOC_ALIGNMENT),
				GFP_KERNEL | GFP_DMA);
	if (!fw_buf) {
		sms_info("failed to allocate firmware buffer");
		return -ENOMEM;
	}
	memcpy(fw_buf, fw->data, fw->size);
	fw_buf_size = fw->size;
#else
	if (!coredev->fw_buf) {
		sms_info("missing fw file buffer");
		return -EINVAL;
	}
	fw_buf = coredev->fw_buf;
	fw_buf_size = coredev->fw_buf_size;
#endif
	rc = (coredev->device_flags & SMS_DEVICE_FAMILY2) ?
		smscore_load_firmware_family2(coredev, fw_buf, fw_buf_size)
		: /*loadfirmware_handler(coredev->context, fw_buf,
		fw_buf_size);*/printk(" error - should not be here\n");
      /* add by jiabf for re download firmware ++ 20120111 */
      if(rc < 0) 
      {    
            sms_warn("error %d try again download firmware",rc);
            msleep(100);
            sms_redownload_firmware_reset(); // add by jiabf
            rc = smscore_load_firmware_family2(coredev, fw_buf, fw_buf_size);
            if(rc < 0)
            {
                sms_warn("error %d try again download firmware failed",rc);
            }
      }
       /* add by jiabf for re download firmware --  20120111 */
	kfree(fw_buf);

#ifdef REQUEST_FIRMWARE_SUPPORTED
	release_firmware(fw);
#else
	coredev->fw_buf = NULL;
	coredev->fw_buf_size = 0;
#endif
	return rc;
}

/**
 * notifies all clients registered with the device, notifies hotplugs,
 * frees all buffers and coredev object
 *
 * @param coredev pointer to a coredev object returned by
 *                smscore_register_device
 *
 * @return 0 on success, <0 on error.
 */
void smscore_unregister_device(struct smscore_device_t *coredev)
{
	struct smscore_buffer_t *cb;
	int num_buffers = 0;
	int retry = 0;

	kmutex_lock(&g_smscore_deviceslock);

	smscore_notify_clients(coredev);
	smscore_notify_callbacks(coredev, NULL, 0);

	/* at this point all buffers should be back
	 * onresponse must no longer be called */

	while (1) {
		while(!list_empty(&coredev->buffers))
		{
			cb = (struct smscore_buffer_t *) coredev->buffers.next;
			list_del(&cb->entry);
			kfree(cb);
			num_buffers++;
		}
		if (num_buffers == coredev->num_buffers ) 
			break;
		if (++retry > 10) {
			sms_info("exiting although "
					"not all buffers released.");
			break;
		}

		sms_info("waiting for %d buffer(s)",
				coredev->num_buffers - num_buffers);
		msleep(100);
	}

	sms_info("freed %d buffers", num_buffers);

	if (coredev->common_buffer)
		dma_free_coherent(NULL, coredev->common_buffer_size,
			coredev->common_buffer, coredev->common_buffer_phys);

	if (coredev->fw_buf != NULL)
		kfree(coredev->fw_buf);
        
	list_del(&coredev->entry);
	kfree(coredev);
        panic_core_dev = NULL ;
	kmutex_unlock(&g_smscore_deviceslock);

	sms_info("device %p destroyed", coredev);
}

static int smscore_detect_mode(struct smscore_device_t *coredev)
{
	void *buffer = kmalloc(sizeof(struct SmsMsgHdr_ST) + SMS_DMA_ALIGNMENT,
			GFP_KERNEL | GFP_DMA);
	struct SmsMsgHdr_ST *msg =
			(struct SmsMsgHdr_ST *) SMS_ALIGN_ADDRESS(buffer);
	int rc;

	if (!buffer)
		return -ENOMEM;

	SMS_INIT_MSG(msg, MSG_SMS_GET_VERSION_EX_REQ,
			sizeof(struct SmsMsgHdr_ST));

	smsendian_handle_tx_message((struct SmsMsgHdr_ST *)msg);
	rc = smscore_sendrequest_and_wait(coredev, msg, msg->msgLength,
			&coredev->version_ex_done);
	if (rc == -ETIME) {
		sms_err("MSG_SMS_GET_VERSION_EX_REQ failed first try");

		if (wait_for_completion_timeout(&coredev->resume_done,
				msecs_to_jiffies(5000))) {
			rc = smscore_sendrequest_and_wait(coredev, msg,
				msg->msgLength, &coredev->version_ex_done);
			if (rc < 0)
				sms_err("MSG_SMS_GET_VERSION_EX_REQ failed "
						"second try, rc %d", rc);
		} else
			rc = -ETIME;
	}

	kfree(buffer);

	return rc;
}

static char *smscore_fw_lkup[][SMS_NUM_OF_DEVICE_TYPES] = {
/*Stellar               NOVA A0         Nova B0         VEGA */
/*DVBT*/
{ "none", "dvb_nova_12mhz.inp", "dvb_nova_12mhz_b0.inp", "none" },
/*DVBH*/
{ "none", "dvb_nova_12mhz.inp", "dvb_nova_12mhz_b0.inp", "none" },
/*TDMB*/
{ "none", "tdmb_nova_12mhz.inp", "tdmb_nova_12mhz_b0.inp", "none" },
/*DABIP*/{ "none", "none", "none", "none" },
/*BDA*/
{ "none", "dvb_nova_12mhz.inp", "dvb_nova_12mhz_b0.inp", "none" },
/*ISDBT*/
{ "none", "isdbt_nova_12mhz.inp", "isdbt_nova_12mhz_b0.inp", "none" },
/*ISDBTBDA*/
{ "none", "isdbt_nova_12mhz.inp", "isdbt_nova_12mhz_b0.inp", "none" },
/*CMMB*/{ "none", "none", "none", "cmmb_vega_12mhz.inp" } };

static inline char *sms_get_fw_name(struct smscore_device_t *coredev, int mode,
		enum sms_device_type_st type) {
	char **fw = sms_get_board(smscore_get_board_id(coredev))->fw;
	return (fw && fw[mode]) ? fw[mode] : smscore_fw_lkup[mode][type];
}


int smscore_reset_device_drvs(struct smscore_device_t *coredev)

{
	int rc = 0;
    	sms_debug("currnet  device mode to %d", coredev->mode);
		coredev->mode = DEVICE_MODE_NONE;
		coredev->device_flags = 		    SMS_DEVICE_FAMILY2 | SMS_DEVICE_NOT_READY |
		    SMS_ROM_NO_RESPONSE;
		
   return rc;
}


/**
 * calls device handler to change mode of operation
 * NOTE: stellar/usb may disconnect when changing mode
 *
 * @param coredev pointer to a coredev object returned by
 *                smscore_register_device
 * @param mode requested mode of operation
 *
 * @return 0 on success, <0 on error.
 */
int smscore_set_device_mode(struct smscore_device_t *coredev, int mode)
{
	void *buffer;
	int rc = 0;
	enum sms_device_type_st type;


	sms_info("set device mode1 to %d", mode);
//	sms_debug("current device mode, device flags, modes_supported  to %d", coredev->mode, coredev->device_flags, coredev->modes_supported);

	sms_debug("set device mode to %d", mode);
	if (coredev->device_flags & SMS_DEVICE_FAMILY2) {
		if (mode < DEVICE_MODE_DVBT || mode > DEVICE_MODE_RAW_TUNER) {
			sms_err("invalid mode specified %d", mode);
			return -EINVAL;
		}

		smscore_registry_setmode(coredev->devpath, mode);

		if (!(coredev->device_flags & SMS_DEVICE_NOT_READY)) {
			rc = smscore_detect_mode(coredev);
			if (rc < 0) {
				sms_err("mode detect failed %d", rc);
				return rc;
			}
		}

		if (coredev->mode == mode) {
			sms_info("device mode %d already set", mode);
			return 0;
		}

		if (!(coredev->modes_supported & (1 << mode))) {
			char *fw_filename;

			type = smscore_registry_gettype(coredev->devpath);
			fw_filename = sms_get_fw_name(coredev, mode, type);

			if(NULL == fw_filename)
			{
				sms_err("wrong filename");
				return rc;
			}

			rc = smscore_load_firmware_from_file(coredev,
					fw_filename, NULL);
			if (rc < 0) {
				sms_warn("error %d loading firmware: %s, "
					"trying again with default firmware",
					rc, fw_filename);

				/* try again with the default firmware */
				fw_filename = smscore_fw_lkup[mode][type];
				rc = smscore_load_firmware_from_file(coredev,
						fw_filename, NULL);

				if (rc < 0) {
					sms_warn("error %d loading "
							"firmware: %s", rc,
							fw_filename);
					return rc;
				}
			}
			sms_info("firmware download success: %s", fw_filename);
		} else
			sms_info("mode %d supported by running "
					"firmware", mode);

		buffer = kmalloc(sizeof(struct SmsMsgData_ST) +
				SMS_DMA_ALIGNMENT, GFP_KERNEL | GFP_DMA);
		if (buffer) {
			struct SmsMsgData_ST *msg =
					(struct SmsMsgData_ST *)
					SMS_ALIGN_ADDRESS(buffer);

			SMS_INIT_MSG(&msg->xMsgHeader, MSG_SMS_INIT_DEVICE_REQ,
					sizeof(struct SmsMsgData_ST));
			msg->msgData[0] = mode;

			smsendian_handle_tx_message((struct SmsMsgHdr_ST *)msg);
			rc = smscore_sendrequest_and_wait(coredev, msg,
					msg->xMsgHeader.msgLength,
					&coredev->init_device_done);

			SMS_INIT_MSG(&msg->xMsgHeader, MSG_SMS_GET_VERSION_EX_REQ,
					sizeof(struct SmsMsgHdr_ST));

			smsendian_handle_tx_message((struct SmsMsgHdr_ST *)msg);
			rc = smscore_sendrequest_and_wait(coredev, msg,
					msg->xMsgHeader.msgLength,
					&coredev->version_ex_done);

			kfree(buffer);
		} else {
			sms_err("Could not allocate buffer for "
					"init device message.");
			rc = -ENOMEM;
		}

		// start to do loopback test
	//rc = AdrLoopbackTest(coredev);//add by wangtao
		//

	} else {
		if (mode < DEVICE_MODE_DVBT || mode > DEVICE_MODE_DVBT_BDA) {
			sms_err("invalid mode specified %d", mode);
			return -EINVAL;
		}




		smscore_registry_setmode(coredev->devpath, mode);

		if (coredev->detectmode_handler)
			coredev->detectmode_handler(coredev->context,
					&coredev->mode);

		if (coredev->mode != mode && coredev->setmode_handler)
			rc = coredev->setmode_handler(coredev->context, mode);
	}

	if (rc >= 0) {
		sms_err("device is ready");
		coredev->mode = mode;
		coredev->device_flags &= ~SMS_DEVICE_NOT_READY;
	}

	if (rc < 0)
		sms_err("return error code %d.", rc);
	return rc;
}

/**
 * calls device handler to get fw file name
 *
 * @param coredev pointer to a coredev object returned by
 *                smscore_register_device
 * @param filename pointer to user buffer to fill the file name
 *
 * @return 0 on success, <0 on error.
 */
int smscore_get_fw_filename(struct smscore_device_t *coredev, int mode,
		char *filename) {
	int rc = 0;
	enum sms_device_type_st type;
	char tmpname[200];

	type = smscore_registry_gettype(coredev->devpath);

#ifdef REQUEST_FIRMWARE_SUPPORTED
	/* driver not need file system services */
	tmpname[0] = '\0';
#else
	sprintf(tmpname, "%s/%s", DEFAULT_FW_FILE_PATH,
			smscore_fw_lkup[mode][type]);
#endif
	if (copy_to_user(filename, tmpname, strlen(tmpname) + 1)) {
		sms_err("Failed copy file path to user buffer\n");
		return -EFAULT;
	}
	return rc;
}

/**
 * calls device handler to keep fw buff for later use
 *
 * @param coredev pointer to a coredev object returned by
 *                smscore_register_device
 * @param ufwbuf  pointer to user fw buffer
 * @param size    size in bytes of buffer
 *
 * @return 0 on success, <0 on error.
 */
int smscore_send_fw_file(struct smscore_device_t *coredev, u8 *ufwbuf,
		int size) {
	int rc = 0;

	/* free old buffer */
	if (coredev->fw_buf != NULL) {
		kfree(coredev->fw_buf);
		coredev->fw_buf = NULL;
	}

	coredev->fw_buf = kmalloc(ALIGN(size, SMS_ALLOC_ALIGNMENT), GFP_KERNEL
			| GFP_DMA);
	if (!coredev->fw_buf) {
		sms_err("Failed allocate FW buffer memory\n");
		return -EFAULT;
	}

	if (copy_from_user(coredev->fw_buf, ufwbuf, size)) {
		sms_err("Failed copy FW from user buffer\n");
		kfree(coredev->fw_buf);
		return -EFAULT;
	}
	coredev->fw_buf_size = size;

	return rc;
}

/**
 * calls device handler to get current mode of operation
 *
 * @param coredev pointer to a coredev object returned by
 *                smscore_register_device
 *
 * @return current mode
 */
int smscore_get_device_mode(struct smscore_device_t *coredev)
{
	return coredev->mode;
}

/**
 * find client by response id & type within the clients list.
 * return client handle or NULL.
 *
 * @param coredev pointer to a coredev object returned by
 *                smscore_register_device
 * @param data_type client data type (SMS_DONT_CARE for all types)
 * @param id client id (SMS_DONT_CARE for all id)
 *
 */
static struct smscore_client_t *smscore_find_client(
		struct smscore_device_t *coredev, int data_type, int id) {
	struct smscore_client_t *client = NULL;
	struct list_head *next, *first;
	unsigned long flags;
	struct list_head *firstid, *nextid;

	spin_lock_irqsave(&coredev->clientslock, flags);
	first = &coredev->clients;
	for (next = first->next; (next != first) && !client;
			next = next->next) {
		firstid = &((struct smscore_client_t *) next)->idlist;
		for (nextid = firstid->next; nextid != firstid;
				nextid = nextid->next) {
			if ((((struct smscore_idlist_t *) nextid)->id == id)
					&& (((struct smscore_idlist_t *)
						 nextid)->data_type
						== data_type
						|| (((struct smscore_idlist_t *)
						nextid)->data_type == 0))) {
				client = (struct smscore_client_t *) next;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&coredev->clientslock, flags);
	return client;
}

/**
 * find client by response id/type, call clients onresponse handler
 * return buffer to pool on error
 *
 * @param coredev pointer to a coredev object returned by
 *                smscore_register_device
 * @param cb pointer to response buffer descriptor
 *
 */
void smscore_onresponse(struct smscore_device_t *coredev,
		struct smscore_buffer_t *cb) {
	struct SmsMsgHdr_ST *phdr = (struct SmsMsgHdr_ST *) ((u8 *) cb->p
			+ cb->offset);
	struct smscore_client_t *client = smscore_find_client(coredev,
			phdr->msgType, phdr->msgDstId);
	int rc = -EBUSY;

	static unsigned long last_sample_time; /* = 0; */
	static int data_total; /* = 0; */
	unsigned long time_now = jiffies_to_msecs(jiffies);

	if (!last_sample_time)
		last_sample_time = time_now;

	if (time_now - last_sample_time > 10000) {
		sms_debug("\ndata rate %d bytes/secs",
				(int)((data_total * 1000) /
						(time_now - last_sample_time)));

		last_sample_time = time_now;
		data_total = 0;
	}
        
  
	data_total += cb->size;
	/* If no client registered for type & id,
	 * check for control client where type is not registered */
      #if 1
      smsendian_handle_rx_message((struct SmsMsgData_ST *)phdr);
	
	//sms_info("lch fun=%s, phdr->msgType=%d \n", __func__, phdr->msgType);

	switch (phdr->msgType) 
       {
         case MSG_SMS_RF_TUNE_RES:{
                 struct SmsMsgData_ST_TEST *msg_data= (struct SmsMsgData_ST_TEST *)phdr;
		
                 printk("MSG_SMS_RF_TUNE_RES nstatus = 0x%x freq=%d",msg_data->msgData[0],msg_data->msgData[1]);
                 break;
         }
         case MSG_SMS_GET_STATISTICS_EX_RES:
         {
                 struct SmsMsgData_ST_TEST *msg_data= (struct SmsMsgData_ST_TEST *)phdr;
                 CMMB_SMS118X_STATISTICS *pStatistics = (CMMB_SMS118X_STATISTICS *)&msg_data->msgData[1];
                 printk("MSG_SMS_GET_STATISTICS_EX_RES BER=%d,SNR=%d,Fre=%d,InbandPwr=%d\r\n",pStatistics->BER,pStatistics->SNR>>16,pStatistics->Frequency,pStatistics->InBandPwr);
          #if 0
                printk( "StatisticsType=%d, FullSize=%d, IsRfLocked=%d, IsDemodLocked=%d, IsExternalLNAOn=%d\r\n", pStatistics->StatisticsType, pStatistics->FullSize, pStatistics->IsRfLocked, pStatistics->IsDemodLocked, pStatistics->IsExternalLNAOn);
                printk("SNR=0x%x, RSSI=%d, ErrorsCounter=%d, InBandPwr=%d, CarrierOffset=%d, BER=%d\r\n", pStatistics->SNR, pStatistics->RSSI, pStatistics->ErrorsCounter, pStatistics->InBandPwr, pStatistics->CarrierOffset, pStatistics->BER);
                printk( "Frequency=%d, IsBandwidth2Mhz=%d, ModemState=%d, NumActiveChannels=%d\r\n", pStatistics->Frequency, pStatistics->IsBandwidth2Mhz, pStatistics->ModemState, pStatistics->NumActiveChannels);
                printk( "ErrorsHistory=%d,%d,%d,%d,%d,%d,%d,%d\r\n", pStatistics->ErrorsHistory[0],pStatistics->ErrorsHistory[1],pStatistics->ErrorsHistory[2],pStatistics->ErrorsHistory[3],pStatistics->ErrorsHistory[4],pStatistics->ErrorsHistory[5],pStatistics->ErrorsHistory[6],pStatistics->ErrorsHistory[7]);
                printk( "ch0 Id=%d, RsNumTotalBytes=%d, PostLdpcBadBytes=%d, LdpcCycleCountAvg=%d, LdpcNonConvergedWordsCount=%d\r\n", pStatistics->ChannelsStatsArr[0].Id, pStatistics->ChannelsStatsArr[0].RsNumTotalBytes, pStatistics->ChannelsStatsArr[0].PostLdpcBadBytes, pStatistics->ChannelsStatsArr[0].LdpcCycleCountAvg, pStatistics->ChannelsStatsArr[0].LdpcNonConvergedWordsCount);
                printk("ch0 LdpcWordsCount=%d, RsNumTotalRows=%d, RsNumBadRows=%d, NumGoodFrameHeaders=%d, NumBadFrameHeaders=%d\r\n", pStatistics->ChannelsStatsArr[0].LdpcWordsCount, pStatistics->ChannelsStatsArr[0].RsNumTotalRows, pStatistics->ChannelsStatsArr[0].RsNumBadRows, pStatistics->ChannelsStatsArr[0].NumGoodFrameHeaders, pStatistics->ChannelsStatsArr[0].NumBadFrameHeaders);
                printk( "ch1 Id=%d, RsNumTotalBytes=%d, PostLdpcBadBytes=%d, LdpcCycleCountAvg=%d, LdpcNonConvergedWordsCount=%d\r\n", pStatistics->ChannelsStatsArr[1].Id, pStatistics->ChannelsStatsArr[1].RsNumTotalBytes, pStatistics->ChannelsStatsArr[1].PostLdpcBadBytes, pStatistics->ChannelsStatsArr[1].LdpcCycleCountAvg, pStatistics->ChannelsStatsArr[1].LdpcNonConvergedWordsCount);
                printk("ch1 LdpcWordsCount=%d, RsNumTotalRows=%d, RsNumBadRows=%d, NumGoodFrameHeaders=%d, NumBadFrameHeaders=%d\r\n", pStatistics->ChannelsStatsArr[1].LdpcWordsCount, pStatistics->ChannelsStatsArr[1].RsNumTotalRows, pStatistics->ChannelsStatsArr[1].RsNumBadRows, pStatistics->ChannelsStatsArr[1].NumGoodFrameHeaders, pStatistics->ChannelsStatsArr[1].NumBadFrameHeaders);
            #endif
                 
                 break;
		}
         
		#ifdef ZTE_CMMB_LOOP_TEST
		/*add by wangtao for cmmb 607loop 20111114 ++*/ 
		case MSG_SMS_DAB_CHANNEL:{			
			u32* buf;
			u32 i,len;
			struct SmsMsgData_LOOP_TEST *msg_data= (struct SmsMsgData_LOOP_TEST *)phdr;
			buf= &(msg_data->msgData[0]);
			len= (msg_data->xMsgHeader.msgLength-8)/4;
			
			
			printk("MSG_SMS_DAB_CHANNEL g_607Buf, %d\n", len);
		 		
			if(g_607num<255){
				for(i=0;i<len;i++)
			 		 g_607Buf[i+g_607len]=*(buf+i);				
				//printk("MSG_SMS_DAB_CHANNEL g_607Buf :0x%x, 0x%x, 0x%x, 0x%x \n",g_607Buf[0+1024*g_607num],g_607Buf[1+1024*g_607num],g_607Buf[2+1024*g_607num],g_607Buf[3+1024*g_607num]);	
				g_607num++;
				g_607len=len+g_607len;
				printk("MSG_SMS_DAB_CHANNEL g_607Buf, g_607len:%d\n", g_607len);			
			}
			else if(g_607num==255)
			{		
				printk("MSG_SMS_DAB_CHANNEL last g_607Buf:\n");
			//	for(i=0;i<261120;i++)
			//	{
			//		printk("0x%04x, ", g_607Buf[i]);
			//		if((i%10)==0)
			//	 		printk("\r\n");					
			//	}	
				g_607num++;		
			}
			break;
		}
		/*add by wangtao for cmmb 607loop 20111114 --*/
            #endif
	}
      #endif
    
	if (client)
		rc = client->onresponse_handler(client->context, cb);

	if (rc < 0) {
		smsendian_handle_rx_message((struct SmsMsgData_ST *)phdr);
		
		//sms_info("lch fun=%s, phdr->msgType=%d \n", __func__, phdr->msgType);

		switch (phdr->msgType) {
             #if 0  // add by jiabf for test
             case MSG_SMS_RF_TUNE_RES:{
                     struct SmsMsgData_ST_TEST *msg_data= (struct SmsMsgData_ST_TEST *)phdr;
                     printk("MSG_SMS_RF_TUNE_RES nstatus = 0x%x freq=%d",msg_data->msgData[0],msg_data->msgData[1]);
                     break;
             }
             case MSG_SMS_GET_STATISTICS_EX_RES:{
                     struct SmsMsgData_ST_TEST *msg_data= (struct SmsMsgData_ST_TEST *)phdr;
                     CMMB_SMS118X_STATISTICS *pStatistics = (CMMB_SMS118X_STATISTICS *)&msg_data->msgData[1];
                     printk("MSG_SMS_GET_STATISTICS_EX_RES BER=%d,SNR=%d,Fre=%d,InbandPwr=%d\r\n",pStatistics->BER,pStatistics->SNR>>16,pStatistics->Frequency,pStatistics->InBandPwr);
#if 0
                    printk( "StatisticsType=%d, FullSize=%d, IsRfLocked=%d, IsDemodLocked=%d, IsExternalLNAOn=%d\r\n", pStatistics->StatisticsType, pStatistics->FullSize, pStatistics->IsRfLocked, pStatistics->IsDemodLocked, pStatistics->IsExternalLNAOn);
                    printk("SNR=0x%x, RSSI=%d, ErrorsCounter=%d, InBandPwr=%d, CarrierOffset=%d, BER=%d\r\n", pStatistics->SNR, pStatistics->RSSI, pStatistics->ErrorsCounter, pStatistics->InBandPwr, pStatistics->CarrierOffset, pStatistics->BER);
                    printk( "Frequency=%d, IsBandwidth2Mhz=%d, ModemState=%d, NumActiveChannels=%d\r\n", pStatistics->Frequency, pStatistics->IsBandwidth2Mhz, pStatistics->ModemState, pStatistics->NumActiveChannels);
                    printk( "ErrorsHistory=%d,%d,%d,%d,%d,%d,%d,%d\r\n", pStatistics->ErrorsHistory[0],pStatistics->ErrorsHistory[1],pStatistics->ErrorsHistory[2],pStatistics->ErrorsHistory[3],pStatistics->ErrorsHistory[4],pStatistics->ErrorsHistory[5],pStatistics->ErrorsHistory[6],pStatistics->ErrorsHistory[7]);
                    printk( "ch0 Id=%d, RsNumTotalBytes=%d, PostLdpcBadBytes=%d, LdpcCycleCountAvg=%d, LdpcNonConvergedWordsCount=%d\r\n", pStatistics->ChannelsStatsArr[0].Id, pStatistics->ChannelsStatsArr[0].RsNumTotalBytes, pStatistics->ChannelsStatsArr[0].PostLdpcBadBytes, pStatistics->ChannelsStatsArr[0].LdpcCycleCountAvg, pStatistics->ChannelsStatsArr[0].LdpcNonConvergedWordsCount);
                    printk("ch0 LdpcWordsCount=%d, RsNumTotalRows=%d, RsNumBadRows=%d, NumGoodFrameHeaders=%d, NumBadFrameHeaders=%d\r\n", pStatistics->ChannelsStatsArr[0].LdpcWordsCount, pStatistics->ChannelsStatsArr[0].RsNumTotalRows, pStatistics->ChannelsStatsArr[0].RsNumBadRows, pStatistics->ChannelsStatsArr[0].NumGoodFrameHeaders, pStatistics->ChannelsStatsArr[0].NumBadFrameHeaders);
                    printk( "ch1 Id=%d, RsNumTotalBytes=%d, PostLdpcBadBytes=%d, LdpcCycleCountAvg=%d, LdpcNonConvergedWordsCount=%d\r\n", pStatistics->ChannelsStatsArr[1].Id, pStatistics->ChannelsStatsArr[1].RsNumTotalBytes, pStatistics->ChannelsStatsArr[1].PostLdpcBadBytes, pStatistics->ChannelsStatsArr[1].LdpcCycleCountAvg, pStatistics->ChannelsStatsArr[1].LdpcNonConvergedWordsCount);
                    printk("ch1 LdpcWordsCount=%d, RsNumTotalRows=%d, RsNumBadRows=%d, NumGoodFrameHeaders=%d, NumBadFrameHeaders=%d\r\n", pStatistics->ChannelsStatsArr[1].LdpcWordsCount, pStatistics->ChannelsStatsArr[1].RsNumTotalRows, pStatistics->ChannelsStatsArr[1].RsNumBadRows, pStatistics->ChannelsStatsArr[1].NumGoodFrameHeaders, pStatistics->ChannelsStatsArr[1].NumBadFrameHeaders);
#endif
                     
                     break;
             }
             #endif

			 
		case MSG_SMS_GET_VERSION_EX_RES: {
			struct SmsVersionRes_ST *ver =
					(struct SmsVersionRes_ST *) phdr;
			printk("MSG_SMS_GET_VERSION_EX_RES "
					"id %d prots 0x%x ver %d.%d, ver=%s\r\n",
					ver->FirmwareId,
					ver->SupportedProtocols,
					ver->RomVersionMajor,
					ver->RomVersionMinor, ver->TextLabel);

			coredev->mode = ver->FirmwareId == 255 ?
					DEVICE_MODE_NONE : ver->FirmwareId;
			//coredev->modes_supported = ver->SupportedProtocols;

			complete(&coredev->version_ex_done);
			break;
		}
		case MSG_SMS_INIT_DEVICE_RES:
			sms_debug("MSG_SMS_INIT_DEVICE_RES");
			complete(&coredev->init_device_done);
			break;
		case MSG_SW_RELOAD_START_RES:
			sms_debug("MSG_SW_RELOAD_START_RES");
			complete(&coredev->reload_start_done);
			break;
		case MSG_SMS_DATA_DOWNLOAD_RES:
			complete(&coredev->data_download_done);
			break;
		case MSG_SW_RELOAD_EXEC_RES:
			sms_debug("MSG_SW_RELOAD_EXEC_RES");
			break;
		case MSG_SMS_SWDOWNLOAD_TRIGGER_RES:
			sms_debug("MSG_SMS_SWDOWNLOAD_TRIGGER_RES");
			complete(&coredev->trigger_done);
			break;
		case MSG_SMS_SLEEP_RESUME_COMP_IND:
			complete(&coredev->resume_done);
			break;
		case MSG_SMS_GPIO_CONFIG_EX_RES:
			sms_debug("MSG_SMS_GPIO_CONFIG_EX_RES");
			complete(&coredev->gpio_configuration_done);
			break;
		case MSG_SMS_GPIO_SET_LEVEL_RES:
			sms_debug("MSG_SMS_GPIO_SET_LEVEL_RES");
			complete(&coredev->gpio_set_level_done);
			break;
		case MSG_SMS_GPIO_GET_LEVEL_RES:
		{
			u32 *msgdata = (u32 *) phdr;
			coredev->gpio_get_res = msgdata[1];
			sms_debug("MSG_SMS_GPIO_GET_LEVEL_RES gpio level %d",
					coredev->gpio_get_res);
			complete(&coredev->gpio_get_level_done);
			break;
		}

// loopback in the drv

		case MSG_SMS_LOOPBACK_RES:
		{
		//	u32 *msgdata = (u32 *) phdr;
			memcpy( g_LbResBuf, (u8 *)phdr, phdr->msgLength );
			sms_debug("MSG_SMS_LOOPBACK_RES \n");
			complete(&coredev->loopback_res_done);
			break;
		}

		default:
#if 0
			sms_info("no client (%p) or error (%d), "
					"type:%d dstid:%d", client, rc,
					phdr->msgType, phdr->msgDstId);
#endif
			break;
		}
		smscore_putbuffer(coredev, cb);
	}
}

/**
 * return pointer to next free buffer descriptor from core pool
 *
 * @param coredev pointer to a coredev object returned by
 *                smscore_register_device
 *
 * @return pointer to descriptor on success, NULL on error.
 */
struct smscore_buffer_t *smscore_getbuffer(struct smscore_device_t *coredev)
{
	struct smscore_buffer_t *cb = NULL;
	unsigned long flags;

	DEFINE_WAIT(wait);

	spin_lock_irqsave(&coredev->bufferslock, flags);

	/* This function must return a valid buffer, since the buffer list is
	 * finite, we check that there is an available buffer, if not, we wait
	 * until such buffer become available.
	 */

	prepare_to_wait(&coredev->buffer_mng_waitq, &wait, TASK_INTERRUPTIBLE);

	if (list_empty(&coredev->buffers))
	{
		//to avoid rx buffers hung
printk("eladr: smscore_getbuffer scheduled caus list is empty\n");
     		spin_unlock_irqrestore(&coredev->bufferslock, flags);
		schedule();
		spin_lock_irqsave(&coredev->bufferslock, flags);
	}

//printk("smscore_getbuffer call finish_wait\n");
	finish_wait(&coredev->buffer_mng_waitq, &wait);

// if list is still empty we will return null
	if (list_empty(&coredev->buffers))
	{
		//buffer is null
		printk("eladr: smscore_getbuffer fail to allocate buffer, returning null \n");		
	}
	else
	{
		cb = (struct smscore_buffer_t *) coredev->buffers.next;
		if(cb->entry.prev==LIST_POISON1 || cb->entry.next==LIST_POISON1 || cb->entry.prev==LIST_POISON2 || cb->entry.next==LIST_POISON2 )
		{
			printk("smscore_getbuffer list is no good\n");  
			spin_unlock_irqrestore(&coredev->bufferslock, flags); 
			return NULL;
		}

//printk("smscore_getbuffer buffer was allocated cb=0x%x\n", cb);
		list_del(&cb->entry);
	}

	spin_unlock_irqrestore(&coredev->bufferslock, flags);

	return cb;
}

/**
 * return buffer descriptor to a pool
 *
 * @param coredev pointer to a coredev object returned by
 *                smscore_register_device
 * @param cb pointer buffer descriptor
 *
 */
void smscore_putbuffer(struct smscore_device_t *coredev,
		struct smscore_buffer_t *cb) {
       wake_up_interruptible(&coredev->buffer_mng_waitq);
       list_add_locked(&cb->entry, &coredev->buffers, &coredev->bufferslock);
}

static int smscore_validate_client(struct smscore_device_t *coredev,
		struct smscore_client_t *client, int data_type, int id) {
	struct smscore_idlist_t *listentry;
	struct smscore_client_t *registered_client;

	if (!client) {
		sms_err("bad parameter.");
		return -EFAULT;
	}
	registered_client = smscore_find_client(coredev, data_type, id);
	if (registered_client == client)
		return 0;

	if (registered_client) {
		sms_err("The msg ID already registered to another client.");
		return -EEXIST;
	}
	listentry = kzalloc(sizeof(struct smscore_idlist_t), GFP_KERNEL);
	if (!listentry) {
		sms_err("Can't allocate memory for client id.");
		return -ENOMEM;
	}
	listentry->id = id;
	listentry->data_type = data_type;
	list_add_locked(&listentry->entry, &client->idlist,
			&coredev->clientslock);
	return 0;
}

/**
 * creates smsclient object, check that id is taken by another client
 *
 * @param coredev pointer to a coredev object from clients hotplug
 * @param initial_id all messages with this id would be sent to this client
 * @param data_type all messages of this type would be sent to this client
 * @param onresponse_handler client handler that is called to
 *                           process incoming messages
 * @param onremove_handler client handler that is called when device is removed
 * @param context client-specific context
 * @param client pointer to a value that receives created smsclient object
 *
 * @return 0 on success, <0 on error.
 */
int smscore_register_client(struct smscore_device_t *coredev,
		struct smsclient_params_t *params,
				struct smscore_client_t **client) {
	struct smscore_client_t *newclient;
	/* check that no other channel with same parameters exists */
	sms_info("entering....smscore_register_client \n");


	if (smscore_find_client(coredev, params->data_type,
				params->initial_id)) {
		sms_err("Client already exist.");
		return -EEXIST;
	}

	newclient = kzalloc(sizeof(struct smscore_client_t), GFP_KERNEL);
	if (!newclient) {
		sms_err("Failed to allocate memory for client.");
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&newclient->idlist);
	newclient->coredev = coredev;
	newclient->onresponse_handler = params->onresponse_handler;
	newclient->onremove_handler = params->onremove_handler;
	newclient->context = params->context;
	list_add_locked(&newclient->entry, &coredev->clients,
			&coredev->clientslock);
	smscore_validate_client(coredev, newclient, params->data_type,
			params->initial_id);
	*client = newclient;
	sms_debug("Register new client %p DT=%d ID=%d",
		params->context, params->data_type, params->initial_id);

	return 0;
}

/**
 * frees smsclient object and all subclients associated with it
 *
 * @param client pointer to smsclient object returned by
 *               smscore_register_client
 *
 */
void smscore_unregister_client(struct smscore_client_t *client)
{
	struct smscore_device_t *coredev = client->coredev;
	unsigned long flags;

	spin_lock_irqsave(&coredev->clientslock, flags);

	while (!list_empty(&client->idlist)) {
		struct smscore_idlist_t *identry =
				(struct smscore_idlist_t *) client->idlist.next;
		list_del(&identry->entry);
		kfree(identry);
	}

	sms_info("%p", client->context);

	list_del(&client->entry);
	kfree(client);

	spin_unlock_irqrestore(&coredev->clientslock, flags);
}

/**
 * verifies that source id is not taken by another client,
 * calls device handler to send requests to the device
 *
 * @param client pointer to smsclient object returned by
 *               smscore_register_client
 * @param buffer pointer to a request buffer
 * @param size size (in bytes) of request buffer
 *
 * @return 0 on success, <0 on error.
 */
int smsclient_sendrequest(struct smscore_client_t *client, void *buffer,
		size_t size) {
	struct smscore_device_t *coredev;
	struct SmsMsgHdr_ST *phdr = (struct SmsMsgHdr_ST *) buffer;
	int rc;

	if (client == NULL) {
		sms_err("Got NULL client");
		return -EINVAL;
	}

	coredev = client->coredev;

	/* check that no other channel with same id exists */
	if (coredev == NULL) {
		sms_err("Got NULL coredev");
		return -EINVAL;
	}

	rc = smscore_validate_client(client->coredev, client, 0,
			phdr->msgSrcId);
	if (rc < 0)
		return rc;

	return coredev->sendrequest_handler(coredev->context, buffer, size);
}

#ifdef SMS_HOSTLIB_SUBSYS
/**
 * return the size of large (common) buffer
 *
 * @param coredev pointer to a coredev object from clients hotplug
 *
 * @return size (in bytes) of the buffer
 */
int smscore_get_common_buffer_size(struct smscore_device_t *coredev)
{
	return coredev->common_buffer_size;
}
EXPORT_SYMBOL(smscore_get_common_buffer_size);
/**
 * maps common buffer (if supported by platform)
 *
 * @param coredev pointer to a coredev object from clients hotplug
 * @param vma pointer to vma struct from mmap handler
 *
 * @return 0 on success, <0 on error.
 */
int smscore_map_common_buffer(struct smscore_device_t *coredev,
		struct vm_area_struct *vma)
{
	unsigned long end = vma->vm_end,
	start = vma->vm_start,
	size = PAGE_ALIGN(coredev->common_buffer_size);

	if (!(vma->vm_flags & (VM_READ | VM_SHARED)) ||
			(vma->vm_flags & VM_WRITE)) {
		sms_err("invalid vm flags");
		return -EINVAL;
	}

	if ((end - start) != size) {
		sms_err("invalid size %d expected %d",
				(int)(end - start), (int)size);
		return -EINVAL;
	}

	if (remap_pfn_range(vma, start,
			coredev->common_buffer_phys >> PAGE_SHIFT,
			size, pgprot_noncached(vma->vm_page_prot))) {
		sms_err("remap_page_range failed");
		return -EAGAIN;
	}

	return 0;
}

EXPORT_SYMBOL(smscore_map_common_buffer);
#endif /* SMS_HOSTLIB_SUBSYS */

static int GetGpioPinParams(u32 PinNum, u32 *pTranslatedPinNum,
		u32 *pGroupNum, u32 *pGroupCfg) {

	*pGroupCfg = 1;

	if (PinNum >= 0 && PinNum <= 1)	{
		*pTranslatedPinNum = 0;
		*pGroupNum = 9;
		*pGroupCfg = 2;
	} else if (PinNum >= 2 && PinNum <= 6) {
		*pTranslatedPinNum = 2;
		*pGroupNum = 0;
		*pGroupCfg = 2;
	} else if (PinNum >= 7 && PinNum <= 11) {
		*pTranslatedPinNum = 7;
		*pGroupNum = 1;
	} else if (PinNum >= 12 && PinNum <= 15) {
		*pTranslatedPinNum = 12;
		*pGroupNum = 2;
		*pGroupCfg = 3;
	} else if (PinNum == 16) {
		*pTranslatedPinNum = 16;
		*pGroupNum = 23;
	} else if (PinNum >= 17 && PinNum <= 24) {
		*pTranslatedPinNum = 17;
		*pGroupNum = 3;
	} else if (PinNum == 25) {
		*pTranslatedPinNum = 25;
		*pGroupNum = 6;
	} else if (PinNum >= 26 && PinNum <= 28) {
		*pTranslatedPinNum = 26;
		*pGroupNum = 4;
	} else if (PinNum == 29) {
		*pTranslatedPinNum = 29;
		*pGroupNum = 5;
		*pGroupCfg = 2;
	} else if (PinNum == 30) {
		*pTranslatedPinNum = 30;
		*pGroupNum = 8;
	} else if (PinNum == 31) {
		*pTranslatedPinNum = 31;
		*pGroupNum = 17;
	} else
		return -1;

	*pGroupCfg <<= 24;

	return 0;
}

int smscore_gpio_configure(struct smscore_device_t *coredev, u8 PinNum,
		struct smscore_gpio_config *pGpioConfig) {

	u32 totalLen;
	u32 TranslatedPinNum;
	u32 GroupNum;
	u32 ElectricChar;
	u32 groupCfg;
	void *buffer;
	int rc;

	struct SetGpioMsg {
		struct SmsMsgHdr_ST xMsgHeader;
		u32 msgData[6];
	} *pMsg;


	if (PinNum > MAX_GPIO_PIN_NUMBER)
		return -EINVAL;

	if (pGpioConfig == NULL)
		return -EINVAL;

	totalLen = sizeof(struct SmsMsgHdr_ST) + (sizeof(u32) * 6);

	buffer = kmalloc(totalLen + SMS_DMA_ALIGNMENT,
			GFP_KERNEL | GFP_DMA);
	if (!buffer)
		return -ENOMEM;

	pMsg = (struct SetGpioMsg *) SMS_ALIGN_ADDRESS(buffer);

	pMsg->xMsgHeader.msgSrcId = DVBT_BDA_CONTROL_MSG_ID;
	pMsg->xMsgHeader.msgDstId = HIF_TASK;
	pMsg->xMsgHeader.msgFlags = 0;
	pMsg->xMsgHeader.msgLength = (u16) totalLen;
	pMsg->msgData[0] = PinNum;

	if (!(coredev->device_flags & SMS_DEVICE_FAMILY2)) {
		pMsg->xMsgHeader.msgType = MSG_SMS_GPIO_CONFIG_REQ;
		if (GetGpioPinParams(PinNum, &TranslatedPinNum, &GroupNum,
				&groupCfg) != 0)
			return -EINVAL;

		pMsg->msgData[1] = TranslatedPinNum;
		pMsg->msgData[2] = GroupNum;
		ElectricChar = (pGpioConfig->PullUpDown)
				| (pGpioConfig->InputCharacteristics << 2)
				| (pGpioConfig->OutputSlewRate << 3)
				| (pGpioConfig->OutputDriving << 4);
		pMsg->msgData[3] = ElectricChar;
		pMsg->msgData[4] = pGpioConfig->Direction;
		pMsg->msgData[5] = groupCfg;
	} else {
		pMsg->xMsgHeader.msgType = MSG_SMS_GPIO_CONFIG_EX_REQ;
		pMsg->msgData[1] = pGpioConfig->PullUpDown;
		pMsg->msgData[2] = pGpioConfig->OutputSlewRate;
		pMsg->msgData[3] = pGpioConfig->OutputDriving;
		pMsg->msgData[4] = pGpioConfig->Direction;
		pMsg->msgData[5] = 0;
	}

	smsendian_handle_tx_message((struct SmsMsgHdr_ST *)pMsg);
	rc = smscore_sendrequest_and_wait(coredev, pMsg, totalLen,
			&coredev->gpio_configuration_done);

	if (rc != 0) {
		if (rc == -ETIME)
			sms_err("smscore_gpio_configure timeout");
		else
			sms_err("smscore_gpio_configure error");
	}
	kfree(buffer);

	return rc;
}

int smscore_gpio_set_level(struct smscore_device_t *coredev, u8 PinNum,
		u8 NewLevel) {

	u32 totalLen;
	int rc;
	void *buffer;

	struct SetGpioMsg {
		struct SmsMsgHdr_ST xMsgHeader;
		u32 msgData[3]; /* keep it 3 ! */
	} *pMsg;

	if ((NewLevel > 1) || (PinNum > MAX_GPIO_PIN_NUMBER) ||
			(PinNum > MAX_GPIO_PIN_NUMBER))
		return -EINVAL;

	totalLen = sizeof(struct SmsMsgHdr_ST) +
			(3 * sizeof(u32)); /* keep it 3 ! */

	buffer = kmalloc(totalLen + SMS_DMA_ALIGNMENT,
			GFP_KERNEL | GFP_DMA);
	if (!buffer)
		return -ENOMEM;

	pMsg = (struct SetGpioMsg *) SMS_ALIGN_ADDRESS(buffer);

	pMsg->xMsgHeader.msgSrcId = DVBT_BDA_CONTROL_MSG_ID;
	pMsg->xMsgHeader.msgDstId = HIF_TASK;
	pMsg->xMsgHeader.msgFlags = 0;
	pMsg->xMsgHeader.msgType = MSG_SMS_GPIO_SET_LEVEL_REQ;
	pMsg->xMsgHeader.msgLength = (u16) totalLen;
	pMsg->msgData[0] = PinNum;
	pMsg->msgData[1] = NewLevel;

	/* Send message to SMS */
	smsendian_handle_tx_message((struct SmsMsgHdr_ST *)pMsg);
	rc = smscore_sendrequest_and_wait(coredev, pMsg, totalLen,
			&coredev->gpio_set_level_done);

	if (rc != 0) {
		if (rc == -ETIME)
			sms_err("smscore_gpio_set_level timeout");
		else
			sms_err("smscore_gpio_set_level error");
	}
	kfree(buffer);

	return rc;
}

int smscore_gpio_get_level(struct smscore_device_t *coredev, u8 PinNum,
		u8 *level) {

	u32 totalLen;
	int rc;
	void *buffer;

	struct SetGpioMsg {
		struct SmsMsgHdr_ST xMsgHeader;
		u32 msgData[2];
	} *pMsg;


	if (PinNum > MAX_GPIO_PIN_NUMBER)
		return -EINVAL;

	totalLen = sizeof(struct SmsMsgHdr_ST) + (2 * sizeof(u32));

	buffer = kmalloc(totalLen + SMS_DMA_ALIGNMENT,
			GFP_KERNEL | GFP_DMA);
	if (!buffer)
		return -ENOMEM;

	pMsg = (struct SetGpioMsg *) SMS_ALIGN_ADDRESS(buffer);

	pMsg->xMsgHeader.msgSrcId = DVBT_BDA_CONTROL_MSG_ID;
	pMsg->xMsgHeader.msgDstId = HIF_TASK;
	pMsg->xMsgHeader.msgFlags = 0;
	pMsg->xMsgHeader.msgType = MSG_SMS_GPIO_GET_LEVEL_REQ;
	pMsg->xMsgHeader.msgLength = (u16) totalLen;
	pMsg->msgData[0] = PinNum;
	pMsg->msgData[1] = 0;

	/* Send message to SMS */
	smsendian_handle_tx_message((struct SmsMsgHdr_ST *)pMsg);
	rc = smscore_sendrequest_and_wait(coredev, pMsg, totalLen,
			&coredev->gpio_get_level_done);

	if (rc != 0) {
		if (rc == -ETIME)
			sms_err("smscore_gpio_get_level timeout");
		else
			sms_err("smscore_gpio_get_level error");
	}
	kfree(buffer);

	/* Its a race between other gpio_get_level() and the copy of the single
	 * global 'coredev->gpio_get_res' to  the function's variable 'level'
	 */
	*level = coredev->gpio_get_res;

	return rc;
}

#if 1 // add by jiabf
void cmmb_init_device_test(void )
{
    struct {
	struct SmsMsgHdr_ST Msg;
    }Msg;                       
    Msg.Msg.msgSrcId = DVBT_BDA_CONTROL_MSG_ID;
    Msg.Msg.msgDstId = HIF_TASK;
    Msg.Msg.msgFlags=0;
    Msg.Msg.msgType = MSG_SMS_INIT_DEVICE_REQ;
    Msg.Msg.msgLength = sizeof(Msg);

    printk("[cmmbspi]cmmb_init_device_test\r\n") ;
   //panic_core_dev->sendrequest_handler(panic_core_dev->context,(void *)&Msg,sizeof(Msg));
   smscore_sendrequest_and_wait(panic_core_dev,(void *)&Msg,sizeof(Msg), &panic_core_dev->init_device_done);
}

void cmmb_get_device_infor_test(void )
{
    struct {
	struct SmsMsgHdr_ST Msg;
    }Msg;                       
    Msg.Msg.msgSrcId = DVBT_BDA_CONTROL_MSG_ID;
    Msg.Msg.msgDstId = HIF_TASK;
    Msg.Msg.msgFlags=0;
    Msg.Msg.msgType = MSG_SMS_GET_VERSION_EX_REQ;
    Msg.Msg.msgLength = sizeof(Msg);

    printk("[cmmbspi]cmmb_get_device_infor_test\r\n") ;
   //panic_core_dev->sendrequest_handler(panic_core_dev->context,(void *)&Msg,sizeof(Msg));
   smscore_sendrequest_and_wait(panic_core_dev,(void *)&Msg,sizeof(Msg), &panic_core_dev->version_ex_done);
}

void cmmb_select_service_test(void)
{
     struct {
	struct SmsMsgHdr_ST Msg;
	u32 Data[3];
    }Msg;                       
   Msg.Msg.msgSrcId = DVBT_BDA_CONTROL_MSG_ID;
   Msg.Msg.msgDstId = HIF_TASK;
   Msg.Msg.msgFlags=0;
   Msg.Msg.msgType = 762;
   Msg.Msg.msgLength = sizeof(Msg);
   Msg.Data[0]= 0xffffffff;
   Msg.Data[1]= 0xffffffff;
   Msg.Data[2]= 601;
   printk("[cmmbspi]cmmb_select_service_test\r\n");
   panic_core_dev->sendrequest_handler(panic_core_dev->context,(void *)&Msg,sizeof(Msg));
}

void cmmb_single_scan_test(u32 fre)
{
struct {
	struct SmsMsgHdr_ST Msg;
	u32 Data[3];
}Msg;                       
   Msg.Msg.msgSrcId = DVBT_BDA_CONTROL_MSG_ID;
   Msg.Msg.msgDstId = HIF_TASK;
   Msg.Msg.msgFlags=0;
   Msg.Msg.msgType = MSG_SMS_RF_TUNE_REQ;
   Msg.Msg.msgLength = sizeof(Msg);
   Msg.Data[0]= fre;
   Msg.Data[1]= BW_8_MHZ;
   Msg.Data[2]= 12000000;
   printk("[cmmbspi]cmmb_single_scan_test\r\n");
   panic_core_dev->sendrequest_handler(panic_core_dev->context,(void *)&Msg,sizeof(Msg));

}

void cmmb_start_control_test(void)
{
    struct {
    	struct SmsMsgHdr_ST Msg;
    	u32 Data[2];
    }Msg;                       
   Msg.Msg.msgSrcId = DVBT_BDA_CONTROL_MSG_ID;
   Msg.Msg.msgDstId = HIF_TASK;
   Msg.Msg.msgFlags=0;
   Msg.Msg.msgType = 772;
   Msg.Msg.msgLength = sizeof(Msg);
   Msg.Data[0]= 0xffffffff;
   Msg.Data[1]= 0xffffffff;
   printk("[cmmbspi]cmmb_start_control_test\r\n") ;
   panic_core_dev->sendrequest_handler(panic_core_dev->context,(void *)&Msg,sizeof(Msg));
}

void cmmb_get_statistics_test(void)
{
    struct {
    	struct SmsMsgHdr_ST Msg;
    }Msg;                       
   Msg.Msg.msgSrcId = DVBT_BDA_CONTROL_MSG_ID;
   Msg.Msg.msgDstId = HIF_TASK;
   Msg.Msg.msgFlags = 0;
   Msg.Msg.msgType = MSG_SMS_GET_STATISTICS_EX_REQ;
   Msg.Msg.msgLength = sizeof(Msg);

   printk("[cmmbspi]cmmb_get_statistics_test\n") ;
   panic_core_dev->sendrequest_handler(panic_core_dev->context,(void *)&Msg,sizeof(Msg));
}

extern void smsspi_poweron(void);
void cmmb_test(u32 fre)
{
    int i=60;
    smsspi_poweron();
    smscore_load_firmware_family2(panic_core_dev,(void*)siano_fwimage_3188,siano_fwsize_3188);
    mdelay(100);
    cmmb_init_device_test();
    //while(1)
    {
        cmmb_get_device_infor_test();
        mdelay(100);
   }
    cmmb_single_scan_test(fre);
    mdelay(2000);
    //cmmb_start_contro_test();
    mdelay(200);
    cmmb_select_service_test();
    while(i>0)
   {
    mdelay(1000);
    cmmb_get_statistics_test();
    i--;
   }
}
#endif

#ifdef	CONFIG_PROC_FS//added   for test
#include <linux/proc_fs.h>
#include<linux/fs.h>

static struct proc_dir_entry *pxaspi_proc_file;

static ssize_t pxaspi_proc_read(struct file *filp,
	char *buffer, size_t length, loff_t *offset)
{
	return 0;
}

static ssize_t pxaspi_proc_write(struct file *filp,
	const char *buff, size_t len, loff_t *off)
{
    int tmp = 0;
    if (len > 256)
    len = 256;

    tmp = simple_strtoul(buff, NULL, 10);    //10 ����
    printk(KERN_WARNING" [cmmbspi] enter pxaspi_proc_write tmp=%d\n",tmp);
    switch(tmp)
    {
        case 13:
        {
            
        }
        case 16:
        {
            cmmb_test(498000000);
            break;
        }    
        case 20:
        {
            cmmb_test(530000000);
            break;
        }    
        case 32:
        {
            cmmb_test(666000000);
            break;
        }
        case 43:
        {
            cmmb_test(754000000);
            break;
        }
        case 3:
        {
            break;
        }
        case 4:
        {
            break;
        }
        case 255:
        {
            break;
        }
        default:
            break;
    }
    return len;
}


static struct file_operations pxaspi_proc_ops = {
	.read = pxaspi_proc_read,
	.write = pxaspi_proc_write,
};

static void create_pxaspi_proc_file(void)
{
    printk(KERN_WARNING"[cmmbspi]enter create_pxaspi_proc_file!\n");

    pxaspi_proc_file = create_proc_entry("driver/cmmbtest", 0644, NULL);
    if (pxaspi_proc_file) {
        //pxaspi_proc_file->owner = THIS_MODULE;
        pxaspi_proc_file->proc_fops = &pxaspi_proc_ops;
    } 
    else
        printk(KERN_WARNING"[cmmbspi] create_pxaspi_proc_file failed!\n");
}
#endif

static int __init smscore_module_init(void)
{
	//printk(KERN_WARNING"helike enter smscore_module_init\n");
	#if 1
	int rc = 0;

	printk(KERN_WARNING"smsmdtv module init...\n");
	sms_info("entering... smscore_module_init....\n");
	INIT_LIST_HEAD(&g_smscore_notifyees);
	INIT_LIST_HEAD(&g_smscore_devices);
	kmutex_init(&g_smscore_deviceslock);

	INIT_LIST_HEAD(&g_smscore_registry);
	kmutex_init(&g_smscore_registrylock);

	/* Register sub system adapter objects */

#ifdef SMS_NET_SUBSYS
	/* NET Register */
	rc = smsnet_register();
	if (rc) {
		sms_err("Error registering Siano's network client.\n");
		goto smsnet_error;
	}
#endif

#ifdef SMS_HOSTLIB_SUBSYS
	/* Char interface Register */
	rc = smschar_register();
	printk(KERN_WARNING"helike smscore_module_init call smschar_register ret=%d\n",rc);

	if (rc) {
		sms_err("Error registering Siano's char device client.\n");
		goto smschar_error;
	}
#endif

#ifdef SMS_DVB3_SUBSYS
	/* DVB v.3 Register */
	rc = smsdvb_register();
	if (rc) {
		sms_err("Error registering DVB client.\n");
		goto smsdvb_error;
	}
#endif

	/* Register interfaces objects */

#ifdef SMS_USB_DRV
	/* USB Register */
	rc = smsusb_register();
	if (rc) {
		sms_err("Error registering USB bus driver.\n");
		goto sms_bus_drv_error;
	}
#endif

#ifdef SMS_SDIO_DRV
	/* SDIO Register */
	rc = smssdio_register();
	if (rc) {
		sms_err("Error registering SDIO bus driver.\n");
		goto sms_bus_drv_error;
	}
#endif

#ifdef SMS_SPI_PXA310_DRV
	/* Intel PXA310 SPI Register */
	rc = smsspi_register();
	if (rc) {
		sms_err("Error registering Intel PXA310 SPI bus driver.\n");
		goto sms_bus_drv_error;
	}
	printk(KERN_INFO "sys_chmod liu \n") ;
	
	sys_chmod("/dev/mdtvctrl", 776);
	sys_chmod("/dev/mdtv1", 776);
	sys_chmod("/dev/mdtv2", 776);
	sys_chmod("/dev/mdtv3", 776);
	sys_chmod("/dev/mdtv4", 776);
	sys_chmod("/dev/mdtv5", 776);
	sys_chmod("/dev/mdtv6", 776);


        //added  for test
       #ifdef	CONFIG_PROC_FS
       create_pxaspi_proc_file();
       //cmmb_test(498000000);
       #endif
       //smscore_load_firmware_family2(panic_core_dev,(void*)siano_fwimage_3188,siano_fwsize_3188);
        ///mdelay(100);
#endif

	return rc;

sms_bus_drv_error:
#ifdef SMS_DVB3_SUBSYS
	smsdvb_unregister();
smsdvb_error:
#endif

#ifdef SMS_HOSTLIB_SUBSYS
	smschar_unregister();
smschar_error:
#endif

#ifdef SMS_NET_SUBSYS
	smsnet_unregister();
smsnet_error:
#endif

	sms_err("rc %d", rc);
	printk(KERN_INFO "%s, rc %d\n", __func__, rc);

	return rc;

	#endif 

	return 0;
}

static void __exit smscore_module_exit(void)
{
#ifdef SMS_NET_SUBSYS
	/* Net Unregister */
	smsnet_unregister();
#endif

#ifdef SMS_HOSTLIB_SUBSYS
	/* Char interface Unregister */
	smschar_unregister();
#endif

#ifdef SMS_DVB3_SUBSYS
	/* DVB v.3 unregister */
	smsdvb_unregister();
#endif

	/* Unegister interfaces objects */
#ifdef SMS_USB_DRV
	/* USB unregister */
	smsusb_unregister();
#endif

#ifdef SMS_SDIO_DRV
	/* SDIO unegister */
	smssdio_unregister();
#endif
#ifdef SMS_SPI_PXA310_DRV
	/* Intel PXA310 SPI unegister */
	smsspi_unregister();
#endif

	kmutex_lock(&g_smscore_deviceslock);
	while (!list_empty(&g_smscore_notifyees)) {
		struct smscore_device_notifyee_t *notifyee =
		(struct smscore_device_notifyee_t *)
		g_smscore_notifyees.next;

		list_del(&notifyee->entry);
		kfree(notifyee);
	}
	kmutex_unlock(&g_smscore_deviceslock);

	kmutex_lock(&g_smscore_registrylock);
	while (!list_empty(&g_smscore_registry)) {
		struct smscore_registry_entry_t *entry =
		(struct smscore_registry_entry_t *)
		g_smscore_registry.next;

		list_del(&entry->entry);
		kfree(entry);
	}
	kmutex_unlock(&g_smscore_registrylock);

	sms_debug("");
}

// for loopback test
// for loopback

int  AdrLoopbackTest( struct smscore_device_t *coredev )
{
	char msgbuff[252];
	struct SmsMsgData_ST* pLoopbackMsg = (struct SmsMsgData_ST*)msgbuff;
	struct SmsMsgData_ST* pLoopbackRes = (struct SmsMsgData_ST*)g_LbResBuf;
	int i , j;
	int g_Loopback_failCounters= 0; 
	int Len = 252 - sizeof(struct SmsMsgData_ST);
	char* pPtr;
	int rc =0;

	pLoopbackMsg->xMsgHeader.msgType = MSG_SMS_LOOPBACK_REQ;
	pLoopbackMsg->xMsgHeader.msgSrcId = 151;
	pLoopbackMsg->xMsgHeader.msgDstId = 11;
	pLoopbackMsg->xMsgHeader.msgFlags = 0;
	pLoopbackMsg->xMsgHeader.msgLength = 252;
	
	sms_info("Loobpack test start.");
	sms_err("Loobpack test start.");

	for ( i = 0 ; i < 10000 ; i++ )
	{

		pPtr = (u8*) &pLoopbackMsg->msgData[1];
		for ( j = 0 ; j < Len ; j ++ )
		{
			pPtr[j] = i+j;
		}
		pLoopbackMsg->msgData[0] = i+1;
	
		smsendian_handle_tx_message((struct SmsMsgHdr_ST *)pLoopbackMsg);
			rc = smscore_sendrequest_and_wait(coredev, pLoopbackMsg,
					pLoopbackMsg->xMsgHeader.msgLength,
					&coredev->loopback_res_done);


		if (rc)
			return  rc; 

	
		pPtr = (u8*) &pLoopbackRes->msgData[1];

		for ( j = 0 ; j < Len ; j ++ )
		{
			if ( pPtr[j] != (u8)(j + i))
			{
					sms_err("Loopback data error at byte %u. Exp %u, Got %u", j, (u8)(j+i), pPtr[j] );
					g_Loopback_failCounters++;
					break;
			}
		} //for ( j = 0 ; j < Len ; j ++ )
	} //for ( i = 0 ; i < 100 ; i++ )
	sms_info( "Loobpack test end. RUN  times: %d; fail times : %d", i, g_Loopback_failCounters);
	sms_err( "Loobpack test end. RUN  times: %d; fail times : %d", i, g_Loopback_failCounters);
	
        return rc ;
}


module_init(smscore_module_init);
module_exit(smscore_module_exit);

MODULE_DESCRIPTION("Siano MDTV Core module");
MODULE_AUTHOR("Siano Mobile Silicon, Inc. (uris@siano-ms.com)");
MODULE_LICENSE("GPL");
