/*
 * Copyright (c) 2021 Gratian Crisan.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <device.h>
#include <drivers/uart.h>
#include <drivers/gpio.h>

#include <usb/usb_device.h>
#include <usb/class/usb_hid.h>

#include <logging/log.h>

#include "nixie.h"

#define DEBUG 1
#define PWR_LIMITER 0

#define LED0_NODE DT_ALIAS(led0)
#define LED	DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN	DT_GPIO_PIN(LED0_NODE, gpios)
#define FLAGS	DT_GPIO_FLAGS(LED0_NODE, gpios)

#if PWR_LIMITER
#define PWR_5V_NODE	DT_NODELABEL(pwr_5v)
#define PWR_LABEL	DT_GPIO_LABEL(PWR_5V_NODE, gpios)
#define PWR_PIN		DT_GPIO_PIN(PWR_5V_NODE, gpios)
#define PWR_FLAGS	DT_GPIO_FLAGS(PWR_5V_NODE, gpios)
#endif

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
 * Simple HID Report Descriptor (report id can be omitted).
 * Test with: $ sudo usbhid-dump -d 2886:0020
 * 003:042:002:DESCRIPTOR         1639617998.110796
 *  05 01 09 00 A1 01 15 00 26 FF 00 85 01 75 08 95
 *  01 09 00 81 02 91 02 C0
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

static void hid_data_read(struct k_work *work)
{
	int ret;
	uint32_t len = 0;
	uint32_t i;

	ret = hid_int_ep_read(hid0_dev, hid_data, HID_DATA_SZ, &len);
	printk("USB HID rx data: ");
	for (i = 0; i < len; i++)
		printk("0x%x ", hid_data[i]);
	printk("\n");
}

void main(void)
{
	const struct device* acm_dev;
	const struct device* led_dev;
#if PWR_LIMITER
	const struct device* pwr_dev;
#endif
	uint8_t nixie_number = 0;
	uint32_t dtr = 0U;
	int ret;
	int i;

	printk("Board: %s\n", CONFIG_BOARD);

	k_work_init(&hid_data_received, hid_data_read);

	hid0_dev = device_get_binding("HID_0");
	if (hid0_dev == NULL) {
		LOG_ERR("Cannot get USB HID 0 Device");
		return;
	}

	usb_hid_register_device(hid0_dev, hid_report_desc,
				sizeof(hid_report_desc), &ops);

	if (usb_hid_init(hid0_dev)) {
		LOG_ERR("Failed usb_hid_init");
	}

	acm_dev = device_get_binding("CDC_ACM_0");
	if (!acm_dev) {
		LOG_WRN("CDC_ACM_0 device not found");
		return;
	}

	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB");
		return;
	}

	LOG_INF("Wait for DTR");
	while (1) {
		uart_line_ctrl_get(acm_dev, UART_LINE_CTRL_DTR, &dtr);
		if (dtr) {
			break;
		}

		k_sleep(K_MSEC(100));
	}

	printk("*** NIXIE clock client ***\n");

	/* on-board LED */
	led_dev = device_get_binding(LED);
	if (led_dev == NULL)
		printk("error: failed to find LED device\n");
	else
		printk("ok: found LED device\n");

	ret = gpio_pin_configure(led_dev, PIN,
				GPIO_OUTPUT_ACTIVE | GPIO_ACTIVE_LOW | FLAGS);
	if (ret < 0)
		printk("error: failed to set led gpio flags\n");

#if PWR_LIMITER
	/* 5V current limiter */
	pwr_dev = device_get_binding(PWR_LABEL);
	if (pwr_dev == NULL)
		printk("error: failed to find 5V power control gpio device\n");
	else
		printk("ok: found 5V power control gpio device\n");

	ret = gpio_pin_configure(pwr_dev, PWR_PIN, GPIO_OUTPUT_ACTIVE | PWR_FLAGS);
	if (ret < 0)
		printk("error: failed to set 5v power gpio flags\n");
	gpio_pin_set(pwr_dev, PWR_PIN, 1);
#endif

	ret = init_nixies();
	if (ret < 0)
		printk("error: failed to init NIXIEs\n");

	gpio_pin_set(led_dev, PIN, 1);

	nixie_number = 0;
	while (1) {
		printk("%d\n", nixie_number);
		for (i = 0; i < 6; i++)
			set_nixie(i, nixie_number);

		nixie_number = (nixie_number + 1) % 10;

		k_sleep(K_MSEC(200));
	}
}
