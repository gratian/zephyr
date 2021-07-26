/*
 * Copyright (c) 2021 Gratian Crisan
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef PWM_DAC_H
#define PWM_DAC_H

int pwm_dac_init();
int pwm_dac_out_ch(uint8_t ch, uint8_t val);
int pwm_dac_out(uint8_t *data, uint32_t cnt);

#endif /* PWM_DAC_H */
