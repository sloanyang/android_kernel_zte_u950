/*
 * ZTE USB debug Driver for Android
 *
 * Copyright (C) 2012 ZTE, Inc.
 * Author: Li Xingyuan <li.xingyuan@zte.com.cn>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/sysrq.h>

static void dbg_reqcomplete(struct usb_ep *ep, struct usb_request *req)
{
	int length = req->actual;
	char key;
	
	if (req->status != 0) {
		pr_err("%s: err %d\n", __func__, req->status);
		return;
	}

	if (length >= 1) {
		key = ((char *)req->buf)[0];
		pr_info("%s: %02x\n", __func__, key);
		handle_sysrq(key);
	}
}

static int dbg_ctrlrequest(struct usb_composite_dev *cdev,
				const struct usb_ctrlrequest *ctrl)
{
	int	value = -EOPNOTSUPP;
	u8 b_requestType = ctrl->bRequestType;
	u8 b_request = ctrl->bRequest;
	u16	w_index = le16_to_cpu(ctrl->wIndex);
	u16	w_value = le16_to_cpu(ctrl->wValue);
	u16	w_length = le16_to_cpu(ctrl->wLength);

	/*pr_info("%s: %02x %02x %04x %04x %04x\n", __func__, 
		b_requestType, b_request, w_index, w_value, w_length);*/

	if (b_requestType == (USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE)
			&& w_value == 0xffff && w_index == 0xffff) {
		if (b_request == 0x00) {
			pr_info("Got debug request!\n");
			cdev->req->complete = dbg_reqcomplete;
			value = w_length;
		}
	}
	
	if (value >= 0) {
		cdev->req->zero = 0;
		cdev->req->length = value;
		value = usb_ep_queue(cdev->gadget->ep0, cdev->req, GFP_ATOMIC);
		if (value < 0)
			pr_err("%s: setup response queue error %d\n",
				__func__, value);
	}

	return value;
}
