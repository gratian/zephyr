/*
 * Copyright (c) 2021 Gratian Crisan
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <device.h>
#include <drivers/pwm.h>

#include <logging/log.h>

#include "pwm_dac.h"

/* Digital #0 */
#define PWM_IO0_NODE	DT_ALIAS(pwm_io0)

#if DT_NODE_HAS_STATUS(PWM_IO0_NODE, okay)
#define PWM0_CTLR	DT_PWMS_CTLR(PWM_IO0_NODE)
#define PWM0_CHANNEL	DT_PWMS_CHANNEL(PWM_IO0_NODE)
#define PWM0_FLAGS	DT_PWMS_FLAGS(PWM_IO0_NODE)
#else
#error "Unsupported board: pwm-io0 devicetree alias is not defined"
#define PWM0_CTLR	DT_INVALID_NODE
#define PWM0_CHANNEL	0
#define PWM0_FLAGS	0
#endif

/* Digital #2 */
#define PWM_IO1_NODE	DT_ALIAS(pwm_io1)

#if DT_NODE_HAS_STATUS(PWM_IO1_NODE, okay)
#define PWM1_CTLR	DT_PWMS_CTLR(PWM_IO1_NODE)
#define PWM1_CHANNEL	DT_PWMS_CHANNEL(PWM_IO1_NODE)
#define PWM1_FLAGS	DT_PWMS_FLAGS(PWM_IO1_NODE)
#else
#error "Unsupported board: pwm-io1 devicetree alias is not defined"
#define PWM1_CTLR	DT_INVALID_NODE
#define PWM1_CHANNEL	0
#define PWM1_FLAGS	0
#endif

/* Digital #4 */
#define PWM_IO2_NODE	DT_ALIAS(pwm_io2)

#if DT_NODE_HAS_STATUS(PWM_IO2_NODE, okay)
#define PWM2_CTLR	DT_PWMS_CTLR(PWM_IO2_NODE)
#define PWM2_CHANNEL	DT_PWMS_CHANNEL(PWM_IO2_NODE)
#define PWM2_FLAGS	DT_PWMS_FLAGS(PWM_IO2_NODE)
#else
#error "Unsupported board: pwm-io2 devicetree alias is not defined"
#define PWM2_CTLR	DT_INVALID_NODE
#define PWM2_CHANNEL	0
#define PWM2_FLAGS	0
#endif

/* Digital #3 */
#define PWM_IO3_NODE	DT_ALIAS(pwm_io3)

#if DT_NODE_HAS_STATUS(PWM_IO3_NODE, okay)
#define PWM3_CTLR	DT_PWMS_CTLR(PWM_IO3_NODE)
#define PWM3_CHANNEL	DT_PWMS_CHANNEL(PWM_IO3_NODE)
#define PWM3_FLAGS	DT_PWMS_FLAGS(PWM_IO3_NODE)
#else
#error "Unsupported board: pwm-io3 devicetree alias is not defined"
#define PWM3_CTLR	DT_INVALID_NODE
#define PWM3_CHANNEL	0
#define PWM3_FLAGS	0
#endif


#define PWM_DAC_PERIOD	120

struct pwm_dac_channel {
	const struct device* dev;
	int (*out)(uint8_t);
};

static int pwm_dac_out_ch0(uint8_t val);
static int pwm_dac_out_ch1(uint8_t val);
static int pwm_dac_out_ch2(uint8_t val);
static int pwm_dac_out_ch3(uint8_t val);

static const struct pwm_dac_channel dac_ch[] = {
	{.dev = DEVICE_DT_GET(PWM0_CTLR), .out = pwm_dac_out_ch0 },
	{.dev = DEVICE_DT_GET(PWM1_CTLR), .out = pwm_dac_out_ch1 },
	{.dev = DEVICE_DT_GET(PWM2_CTLR), .out = pwm_dac_out_ch2 },
	{.dev = DEVICE_DT_GET(PWM3_CTLR), .out = pwm_dac_out_ch3 },
};

static int pwm_dac_out_ch0(uint8_t val)
{
	if (val > PWM_DAC_PERIOD)
		val = PWM_DAC_PERIOD;

	return pwm_pin_set_cycles(dac_ch[0].dev, PWM0_CHANNEL,
				PWM_DAC_PERIOD, val, PWM0_FLAGS);
}

static int pwm_dac_out_ch1(uint8_t val)
{
	if (val > PWM_DAC_PERIOD)
		val = PWM_DAC_PERIOD;

	return pwm_pin_set_cycles(dac_ch[1].dev, PWM1_CHANNEL,
				PWM_DAC_PERIOD, val, PWM1_FLAGS);
}

static int pwm_dac_out_ch2(uint8_t val)
{
	if (val > PWM_DAC_PERIOD)
		val = PWM_DAC_PERIOD;

	return pwm_pin_set_cycles(dac_ch[2].dev, PWM2_CHANNEL,
				PWM_DAC_PERIOD, val, PWM2_FLAGS);
}

static int pwm_dac_out_ch3(uint8_t val)
{
	if (val > PWM_DAC_PERIOD)
		val = PWM_DAC_PERIOD;

	return pwm_pin_set_cycles(dac_ch[3].dev, PWM3_CHANNEL,
				PWM_DAC_PERIOD, val, PWM3_FLAGS);
}

int pwm_dac_out_ch(uint8_t ch, uint8_t val)
{
	int ret;

	if (ch >= sizeof(dac_ch)/sizeof(struct pwm_dac_channel))
		return -EINVAL;

	ret = dac_ch[ch].out(val);

	return ret;
}

int pwm_dac_out(uint8_t *data, uint32_t cnt)
{
	uint32_t i;
	int ret;

	for (i = 0, ret = 0; i < cnt && i < sizeof(dac_ch)/sizeof(struct pwm_dac_channel); i++)
		ret += pwm_dac_out_ch(i, data[i]);

	return ret;
}

/* work-around for pwm glitch on first update */
#define PWM_INIT_RETRIES 2

int pwm_dac_init()
{
	uint8_t i, j;
	int ret;

	for (j = 0; j < PWM_INIT_RETRIES; j++) {
		for (i = 0, ret = 0; i < sizeof(dac_ch)/sizeof(struct pwm_dac_channel); i++)
			ret += pwm_dac_out_ch(i, 0);
		k_sleep(K_MSEC(100));
	}

	return ret;
}
