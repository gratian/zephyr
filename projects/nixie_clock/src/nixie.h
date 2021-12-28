/*
 * Copyright (c) 2021 Gratian Crisan.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef _NIXIE_H_
#define _NIXIE_H_

struct nixie_dev {
	const struct device* dev;
	const char* name;
	gpio_pin_t pin;
	gpio_flags_t flags;
};

int init_nixies(void);
int set_nixie(uint8_t position, uint8_t number);

#endif /* _NIXIE_H_ */
