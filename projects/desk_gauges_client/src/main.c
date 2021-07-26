/*
 * Copyright (c) 2021 Gratian Crisan.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <device.h>
#include <drivers/uart.h>

#include <usb/usb_device.h>
#include <usb/class/usb_hid.h>

#include <logging/log.h>

#include "pwm_dac.h"

#define DEBUG 0

#if DEBUG
LOG_MODULE_REGISTER(cdc_acm_composite, LOG_LEVEL_INF);
#else
LOG_MODULE_REGISTER(cdc_acm_composite, LOG_LEVEL_WRN);
#endif

#define HID_DATA_SZ 31
uint8_t hid_data[HID_DATA_SZ + 1];
#define REPORT_ID_1 0x01

static struct k_work hid_data_received;

/*
 * Simple HID Report Descriptor
 * Report ID is present for completeness, although it can be omitted.
 * Output of "usbhid-dump -d 2fe3:0006 -e descriptor":
 *  05 01 09 00 A1 01 15 00    26 FF 00 85 01 75 08 95
 *  01 09 00 81 02 C0
 */
static const uint8_t hid_report_desc[] = {
	HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_UNDEFINED),
	HID_COLLECTION(HID_COLLECTION_APPLICATION),
	HID_LOGICAL_MIN8(0x00),
	HID_LOGICAL_MAX16(0xFF, 0x00),
	HID_REPORT_ID(REPORT_ID_1),
	HID_REPORT_SIZE(8),
	HID_REPORT_COUNT(1),
	HID_USAGE(HID_USAGE_GEN_DESKTOP_UNDEFINED),
	HID_INPUT(0x02),
	HID_OUTPUT(0x02),
	HID_END_COLLECTION,
};

#if DEBUG
static void int_in_ready_cb(const struct device *dev)
{
	LOG_INF("int_in_ready_cb\n");
}

static void int_out_ready_cb(const struct device *dev)
{
	LOG_INF("int_out_ready_cb\n");
	k_work_submit(&hid_data_received);
}

static void on_idle_cb(const struct device *dev, uint16_t report_id)
{
	LOG_INF("on_idle_cb\n");
}


static void protocol_cb(const struct device *dev, uint8_t protocol)
{
	LOG_INF("New protocol: %s", protocol == HID_PROTOCOL_BOOT ?
		"boot" : "report");
}

static const struct hid_ops ops = {
	.int_in_ready = int_in_ready_cb,
	.int_out_ready = int_out_ready_cb,
	.on_idle = on_idle_cb,
	.protocol_change = protocol_cb,
};
#else
static void int_out_ready_cb(const struct device *dev)
{
	k_work_submit(&hid_data_received);
}

static const struct hid_ops ops = {
	.int_out_ready = int_out_ready_cb,
};
#endif

static const struct device* hid0_dev;
static const struct device* dev0;

static void hid_data_read(struct k_work *work)
{
	int ret;
	uint32_t len = 0;

	ret = hid_int_ep_read(hid0_dev, hid_data, HID_DATA_SZ, &len);
	if (!ret)
		pwm_dac_out(hid_data, len);
}

void main(void)
{
	uint32_t dtr = 0U;
	int ret;

	k_work_init(&hid_data_received, hid_data_read);

	hid0_dev = device_get_binding("HID_0");
	if (hid0_dev == NULL) {
		LOG_ERR("Cannot get USB HID 0 Device");
		return;
	}

	usb_hid_register_device(hid0_dev, hid_report_desc, sizeof(hid_report_desc),
				&ops);

	if (usb_hid_set_proto_code(hid0_dev, HID_BOOT_IFACE_CODE_NONE)) {
		LOG_WRN("Failed to set Protocol Code");
	}

	if (usb_hid_init(hid0_dev)) {
		LOG_ERR("Failed usb_hid_init");
	}

	dev0 = device_get_binding("CDC_ACM_0");
	if (!dev0) {
		LOG_WRN("CDC_ACM_0 device not found");
		return;
	}

	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return;
	}

	LOG_INF("Init PWM channels");
	pwm_dac_init();


	LOG_INF("Wait for DTR");

	while (1) {
		uart_line_ctrl_get(dev0, UART_LINE_CTRL_DTR, &dtr);
		if (dtr) {
			break;
		}

		k_sleep(K_MSEC(100));
	}

	printk("*** Desk Gauges Client ***\n");
	while (1) {
		k_sleep(K_MSEC(1000));
	}
}
