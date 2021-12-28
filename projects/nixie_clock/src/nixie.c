/*
 * Copyright (c) 2021 Gratian Crisan.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <device.h>
#include <drivers/gpio.h>

#include "nixie.h"

#define NIXIE_CNT	6
#define NIXIE_BCD	4

struct nixie_dev nixies[NIXIE_CNT][NIXIE_BCD] = {
	{
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie1_a), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie1_a), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie1_a), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie1_b), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie1_b), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie1_b), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie1_c), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie1_c), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie1_c), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie1_d), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie1_d), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie1_d), gpios)
		}
	},
	{
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie2_a), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie2_a), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie2_a), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie2_b), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie2_b), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie2_b), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie2_c), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie2_c), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie2_c), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie2_d), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie2_d), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie2_d), gpios)
		}
	},
	{
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie3_a), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie3_a), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie3_a), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie3_b), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie3_b), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie3_b), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie3_c), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie3_c), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie3_c), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie3_d), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie3_d), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie3_d), gpios)
		}
	},
	{
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie4_a), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie4_a), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie4_a), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie4_b), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie4_b), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie4_b), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie4_c), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie4_c), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie4_c), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie4_d), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie4_d), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie4_d), gpios)
		}
	},
	{
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie5_a), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie5_a), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie5_a), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie5_b), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie5_b), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie5_b), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie5_c), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie5_c), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie5_c), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie5_d), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie5_d), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie5_d), gpios)
		}
	},
	{
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie6_a), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie6_a), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie6_a), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie6_b), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie6_b), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie6_b), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie6_c), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie6_c), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie6_c), gpios)
		},
		{
			.dev = NULL,
			.name = DT_GPIO_LABEL(DT_NODELABEL(nixie6_d), gpios),
			.pin = DT_GPIO_PIN(DT_NODELABEL(nixie6_d), gpios),
			.flags = DT_GPIO_FLAGS(DT_NODELABEL(nixie6_d), gpios)
		}
	}
};

int init_nixies()
{
	uint8_t i, j;
	int ret = -1;

	for (i = 0; i < NIXIE_CNT; i++) {
		for (j = 0; j < NIXIE_BCD; j++) {
			nixies[i][j].dev = device_get_binding(nixies[i][j].name);

			if (nixies[i][j].dev == NULL) {
				printk("error: nixie %d: failed to get binding for %s\n",
					i, nixies[i][j].name);
				ret = -1;
				goto out;
			} else {
				printk("ok: nixie %d: found %s binding\n",
					i, nixies[i][j].name);
				ret = gpio_pin_configure(nixies[i][j].dev,
							nixies[i][j].pin,
							GPIO_OUTPUT_ACTIVE |
							GPIO_PULL_UP |
							nixies[i][j].flags);
				if (ret < 0) {
					printk("error: nixie %d: failed to configure %s pin\n",
						i, nixies[i][j].name);
					goto out;
				}
			}
		}
	}
out:
	return ret;
}

int set_nixie(uint8_t position, uint8_t number)
{
	uint8_t i;

	if (position >= NIXIE_CNT)
		return -1;

	for (i = 0; i < NIXIE_BCD; i++) {
		gpio_pin_set(nixies[position][i].dev,
			nixies[position][i].pin,
			(number >> i) & 0x01);
	}

	return 0;
}
