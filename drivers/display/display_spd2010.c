/*
 * Copyright (c) 2025 Gratian Crisan
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#define DT_DRV_COMPAT solomon_spd2010

#include "display_spd2010.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(spd2010, CONFIG_DISPLAY_LOG_LEVEL);

#include <string.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>

struct spd2010_config {
	const struct device *spi_dev;
	struct gpio_dt_spec reset_gpio;
};

struct spd2010_data {
	struct spi_config spi_cfg;
	enum display_pixel_format pixel_format;
	enum display_orientation orientation;
	uint16_t width;
	uint16_t height;
	uint8_t bytes_per_pixel;
	uint8_t reg_madctr;
	uint8_t reg_setpixel;
};

struct spd2010_init_cmd {
	uint8_t cmd;
	const void* data;
	size_t len;
};

static const struct spd2010_init_cmd spd2010_init_cmds[] = {
	{0xFF, (uint8_t []){0x20, 0x10, 0x10}, 3},
	{0x0C, (uint8_t []){0x11}, 1},
	{0x10, (uint8_t []){0x02}, 1},
	{0x11, (uint8_t []){0x11}, 1},
	{0x15, (uint8_t []){0x42}, 1},
	{0x16, (uint8_t []){0x11}, 1},
	{0x1A, (uint8_t []){0x02}, 1},
	{0x1B, (uint8_t []){0x11}, 1},
	{0x61, (uint8_t []){0x80}, 1},
	{0x62, (uint8_t []){0x80}, 1},
	{0x54, (uint8_t []){0x44}, 1},
	{0x58, (uint8_t []){0x88}, 1},
	{0x5C, (uint8_t []){0xcc}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x10}, 3},
	{0x20, (uint8_t []){0x80}, 1},
	{0x21, (uint8_t []){0x81}, 1},
	{0x22, (uint8_t []){0x31}, 1},
	{0x23, (uint8_t []){0x20}, 1},
	{0x24, (uint8_t []){0x11}, 1},
	{0x25, (uint8_t []){0x11}, 1},
	{0x26, (uint8_t []){0x12}, 1},
	{0x27, (uint8_t []){0x12}, 1},
	{0x30, (uint8_t []){0x80}, 1},
	{0x31, (uint8_t []){0x81}, 1},
	{0x32, (uint8_t []){0x31}, 1},
	{0x33, (uint8_t []){0x20}, 1},
	{0x34, (uint8_t []){0x11}, 1},
	{0x35, (uint8_t []){0x11}, 1},
	{0x36, (uint8_t []){0x12}, 1},
	{0x37, (uint8_t []){0x12}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x10}, 3},
	{0x41, (uint8_t []){0x11}, 1},
	{0x42, (uint8_t []){0x22}, 1},
	{0x43, (uint8_t []){0x33}, 1},
	{0x49, (uint8_t []){0x11}, 1},
	{0x4A, (uint8_t []){0x22}, 1},
	{0x4B, (uint8_t []){0x33}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x15}, 3},
	{0x00, (uint8_t []){0x00}, 1},
	{0x01, (uint8_t []){0x00}, 1},
	{0x02, (uint8_t []){0x00}, 1},
	{0x03, (uint8_t []){0x00}, 1},
	{0x04, (uint8_t []){0x10}, 1},
	{0x05, (uint8_t []){0x0C}, 1},
	{0x06, (uint8_t []){0x23}, 1},
	{0x07, (uint8_t []){0x22}, 1},
	{0x08, (uint8_t []){0x21}, 1},
	{0x09, (uint8_t []){0x20}, 1},
	{0x0A, (uint8_t []){0x33}, 1},
	{0x0B, (uint8_t []){0x32}, 1},
	{0x0C, (uint8_t []){0x34}, 1},
	{0x0D, (uint8_t []){0x35}, 1},
	{0x0E, (uint8_t []){0x01}, 1},
	{0x0F, (uint8_t []){0x01}, 1},
	{0x20, (uint8_t []){0x00}, 1},
	{0x21, (uint8_t []){0x00}, 1},
	{0x22, (uint8_t []){0x00}, 1},
	{0x23, (uint8_t []){0x00}, 1},
	{0x24, (uint8_t []){0x0C}, 1},
	{0x25, (uint8_t []){0x10}, 1},
	{0x26, (uint8_t []){0x20}, 1},
	{0x27, (uint8_t []){0x21}, 1},
	{0x28, (uint8_t []){0x22}, 1},
	{0x29, (uint8_t []){0x23}, 1},
	{0x2A, (uint8_t []){0x33}, 1},
	{0x2B, (uint8_t []){0x32}, 1},
	{0x2C, (uint8_t []){0x34}, 1},
	{0x2D, (uint8_t []){0x35}, 1},
	{0x2E, (uint8_t []){0x01}, 1},
	{0x2F, (uint8_t []){0x01}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x16}, 3},
	{0x00, (uint8_t []){0x00}, 1},
	{0x01, (uint8_t []){0x00}, 1},
	{0x02, (uint8_t []){0x00}, 1},
	{0x03, (uint8_t []){0x00}, 1},
	{0x04, (uint8_t []){0x08}, 1},
	{0x05, (uint8_t []){0x04}, 1},
	{0x06, (uint8_t []){0x19}, 1},
	{0x07, (uint8_t []){0x18}, 1},
	{0x08, (uint8_t []){0x17}, 1},
	{0x09, (uint8_t []){0x16}, 1},
	{0x0A, (uint8_t []){0x33}, 1},
	{0x0B, (uint8_t []){0x32}, 1},
	{0x0C, (uint8_t []){0x34}, 1},
	{0x0D, (uint8_t []){0x35}, 1},
	{0x0E, (uint8_t []){0x01}, 1},
	{0x0F, (uint8_t []){0x01}, 1},
	{0x20, (uint8_t []){0x00}, 1},
	{0x21, (uint8_t []){0x00}, 1},
	{0x22, (uint8_t []){0x00}, 1},
	{0x23, (uint8_t []){0x00}, 1},
	{0x24, (uint8_t []){0x04}, 1},
	{0x25, (uint8_t []){0x08}, 1},
	{0x26, (uint8_t []){0x16}, 1},
	{0x27, (uint8_t []){0x17}, 1},
	{0x28, (uint8_t []){0x18}, 1},
	{0x29, (uint8_t []){0x19}, 1},
	{0x2A, (uint8_t []){0x33}, 1},
	{0x2B, (uint8_t []){0x32}, 1},
	{0x2C, (uint8_t []){0x34}, 1},
	{0x2D, (uint8_t []){0x35}, 1},
	{0x2E, (uint8_t []){0x01}, 1},
	{0x2F, (uint8_t []){0x01}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x12}, 3},
	{0x00, (uint8_t []){0x99}, 1},
	{0x2A, (uint8_t []){0x28}, 1},
	{0x2B, (uint8_t []){0x0f}, 1},
	{0x2C, (uint8_t []){0x16}, 1},
	{0x2D, (uint8_t []){0x28}, 1},
	{0x2E, (uint8_t []){0x0f}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0xA0}, 3},
	{0x08, (uint8_t []){0xdc}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x45}, 3},
	{0x01, (uint8_t []){0x9C}, 1},
	{0x03, (uint8_t []){0x9C}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x42}, 3},
	{0x05, (uint8_t []){0x2c}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x11}, 3},
	{0x50, (uint8_t []){0x01}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x00}, 3},
	{0x2A, (uint8_t []){0x00, 0x00, 0x01, 0x9B}, 4},
	{0x2B, (uint8_t []){0x00, 0x00, 0x01, 0x9B}, 4},
	{0xFF, (uint8_t []){0x20, 0x10, 0x40}, 3},
	{0x86, (uint8_t []){0x00}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x00}, 3},
	{0xFF, (uint8_t []){0x20, 0x10, 0x12}, 3},
	{0x0D, (uint8_t []){0x66}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x17}, 3},
	{0x39, (uint8_t []){0x3c}, 1},
	{0xff, (uint8_t []){0x20, 0x10, 0x31}, 3},
	{0x38, (uint8_t []){0x03}, 1},
	{0x39, (uint8_t []){0xf0}, 1},
	{0x36, (uint8_t []){0x03}, 1},
	{0x37, (uint8_t []){0xe8}, 1},
	{0x34, (uint8_t []){0x03}, 1},
	{0x35, (uint8_t []){0xCF}, 1},
	{0x32, (uint8_t []){0x03}, 1},
	{0x33, (uint8_t []){0xBA}, 1},
	{0x30, (uint8_t []){0x03}, 1},
	{0x31, (uint8_t []){0xA2}, 1},
	{0x2e, (uint8_t []){0x03}, 1},
	{0x2f, (uint8_t []){0x95}, 1},
	{0x2c, (uint8_t []){0x03}, 1},
	{0x2d, (uint8_t []){0x7e}, 1},
	{0x2a, (uint8_t []){0x03}, 1},
	{0x2b, (uint8_t []){0x62}, 1},
	{0x28, (uint8_t []){0x03}, 1},
	{0x29, (uint8_t []){0x44}, 1},
	{0x26, (uint8_t []){0x02}, 1},
	{0x27, (uint8_t []){0xfc}, 1},
	{0x24, (uint8_t []){0x02}, 1},
	{0x25, (uint8_t []){0xd0}, 1},
	{0x22, (uint8_t []){0x02}, 1},
	{0x23, (uint8_t []){0x98}, 1},
	{0x20, (uint8_t []){0x02}, 1},
	{0x21, (uint8_t []){0x6f}, 1},
	{0x1e, (uint8_t []){0x02}, 1},
	{0x1f, (uint8_t []){0x32}, 1},
	{0x1c, (uint8_t []){0x01}, 1},
	{0x1d, (uint8_t []){0xf6}, 1},
	{0x1a, (uint8_t []){0x01}, 1},
	{0x1b, (uint8_t []){0xb8}, 1},
	{0x18, (uint8_t []){0x01}, 1},
	{0x19, (uint8_t []){0x6E}, 1},
	{0x16, (uint8_t []){0x01}, 1},
	{0x17, (uint8_t []){0x41}, 1},
	{0x14, (uint8_t []){0x00}, 1},
	{0x15, (uint8_t []){0xfd}, 1},
	{0x12, (uint8_t []){0x00}, 1},
	{0x13, (uint8_t []){0xCf}, 1},
	{0x10, (uint8_t []){0x00}, 1},
	{0x11, (uint8_t []){0x98}, 1},
	{0x0e, (uint8_t []){0x00}, 1},
	{0x0f, (uint8_t []){0x89}, 1},
	{0x0c, (uint8_t []){0x00}, 1},
	{0x0d, (uint8_t []){0x79}, 1},
	{0x0a, (uint8_t []){0x00}, 1},
	{0x0b, (uint8_t []){0x67}, 1},
	{0x08, (uint8_t []){0x00}, 1},
	{0x09, (uint8_t []){0x55}, 1},
	{0x06, (uint8_t []){0x00}, 1},
	{0x07, (uint8_t []){0x3F}, 1},
	{0x04, (uint8_t []){0x00}, 1},
	{0x05, (uint8_t []){0x28}, 1},
	{0x02, (uint8_t []){0x00}, 1},
	{0x03, (uint8_t []){0x0E}, 1},
	{0xff, (uint8_t []){0x20, 0x10, 0x00}, 3},
	{0xff, (uint8_t []){0x20, 0x10, 0x32}, 3},
	{0x38, (uint8_t []){0x03}, 1},
	{0x39, (uint8_t []){0xf0}, 1},
	{0x36, (uint8_t []){0x03}, 1},
	{0x37, (uint8_t []){0xe8}, 1},
	{0x34, (uint8_t []){0x03}, 1},
	{0x35, (uint8_t []){0xCF}, 1},
	{0x32, (uint8_t []){0x03}, 1},
	{0x33, (uint8_t []){0xBA}, 1},
	{0x30, (uint8_t []){0x03}, 1},
	{0x31, (uint8_t []){0xA2}, 1},
	{0x2e, (uint8_t []){0x03}, 1},
	{0x2f, (uint8_t []){0x95}, 1},
	{0x2c, (uint8_t []){0x03}, 1},
	{0x2d, (uint8_t []){0x7e}, 1},
	{0x2a, (uint8_t []){0x03}, 1},
	{0x2b, (uint8_t []){0x62}, 1},
	{0x28, (uint8_t []){0x03}, 1},
	{0x29, (uint8_t []){0x44}, 1},
	{0x26, (uint8_t []){0x02}, 1},
	{0x27, (uint8_t []){0xfc}, 1},
	{0x24, (uint8_t []){0x02}, 1},
	{0x25, (uint8_t []){0xd0}, 1},
	{0x22, (uint8_t []){0x02}, 1},
	{0x23, (uint8_t []){0x98}, 1},
	{0x20, (uint8_t []){0x02}, 1},
	{0x21, (uint8_t []){0x6f}, 1},
	{0x1e, (uint8_t []){0x02}, 1},
	{0x1f, (uint8_t []){0x32}, 1},
	{0x1c, (uint8_t []){0x01}, 1},
	{0x1d, (uint8_t []){0xf6}, 1},
	{0x1a, (uint8_t []){0x01}, 1},
	{0x1b, (uint8_t []){0xb8}, 1},
	{0x18, (uint8_t []){0x01}, 1},
	{0x19, (uint8_t []){0x6E}, 1},
	{0x16, (uint8_t []){0x01}, 1},
	{0x17, (uint8_t []){0x41}, 1},
	{0x14, (uint8_t []){0x00}, 1},
	{0x15, (uint8_t []){0xfd}, 1},
	{0x12, (uint8_t []){0x00}, 1},
	{0x13, (uint8_t []){0xCf}, 1},
	{0x10, (uint8_t []){0x00}, 1},
	{0x11, (uint8_t []){0x98}, 1},
	{0x0e, (uint8_t []){0x00}, 1},
	{0x0f, (uint8_t []){0x89}, 1},
	{0x0c, (uint8_t []){0x00}, 1},
	{0x0d, (uint8_t []){0x79}, 1},
	{0x0a, (uint8_t []){0x00}, 1},
	{0x0b, (uint8_t []){0x67}, 1},
	{0x08, (uint8_t []){0x00}, 1},
	{0x09, (uint8_t []){0x55}, 1},
	{0x06, (uint8_t []){0x00}, 1},
	{0x07, (uint8_t []){0x3F}, 1},
	{0x04, (uint8_t []){0x00}, 1},
	{0x05, (uint8_t []){0x28}, 1},
	{0x02, (uint8_t []){0x00}, 1},
	{0x03, (uint8_t []){0x0E}, 1},
	{0xff, (uint8_t []){0x20, 0x10, 0x00}, 3},
	{0xFF, (uint8_t []){0x20, 0x10, 0x11}, 3},
	{0x60, (uint8_t []){0x01}, 1},
	{0x65, (uint8_t []){0x03}, 1},
	{0x66, (uint8_t []){0x38}, 1},
	{0x67, (uint8_t []){0x04}, 1},
	{0x68, (uint8_t []){0x34}, 1},
	{0x69, (uint8_t []){0x03}, 1},
	{0x61, (uint8_t []){0x03}, 1},
	{0x62, (uint8_t []){0x38}, 1},
	{0x63, (uint8_t []){0x04}, 1},
	{0x64, (uint8_t []){0x34}, 1},
	{0x0A, (uint8_t []){0x11}, 1},
	{0x0B, (uint8_t []){0x20}, 1},
	{0x0c, (uint8_t []){0x20}, 1},
	{0x55, (uint8_t []){0x06}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x42}, 3},
	{0x05, (uint8_t []){0x3D}, 1},
	{0x06, (uint8_t []){0x03}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x00}, 3},
	{0xFF, (uint8_t []){0x20, 0x10, 0x12}, 3},
	{0x1F, (uint8_t []){0xDC}, 1},
	{0xff, (uint8_t []){0x20, 0x10, 0x17}, 3},
	{0x11, (uint8_t []){0xAA}, 1},
	{0x16, (uint8_t []){0x12}, 1},
	{0x0B, (uint8_t []){0xC3}, 1},
	{0x10, (uint8_t []){0x0E}, 1},
	{0x14, (uint8_t []){0xAA}, 1},
	{0x18, (uint8_t []){0xA0}, 1},
	{0x1A, (uint8_t []){0x80}, 1},
	{0x1F, (uint8_t []){0x80}, 1},
	{0xff, (uint8_t []){0x20, 0x10, 0x11}, 3},
	{0x30, (uint8_t []){0xEE}, 1},
	{0xff, (uint8_t []){0x20, 0x10, 0x12}, 3},
	{0x15, (uint8_t []){0x0F}, 1},
	{0xff, (uint8_t []){0x20, 0x10, 0x2D}, 3},
	{0x01, (uint8_t []){0x3E}, 1},
	{0xff, (uint8_t []){0x20, 0x10, 0x40}, 3},
	{0x83, (uint8_t []){0xC4}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x12}, 3},
	{0x00, (uint8_t []){0xCC}, 1},
	{0x36, (uint8_t []){0xA0}, 1},
	{0x2A, (uint8_t []){0x2D}, 1},
	{0x2B, (uint8_t []){0x1e}, 1},
	{0x2C, (uint8_t []){0x26}, 1},
	{0x2D, (uint8_t []){0x2D}, 1},
	{0x2E, (uint8_t []){0x1e}, 1},
	{0x1F, (uint8_t []){0xE6}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0xA0}, 3},
	{0x08, (uint8_t []){0xE6}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x12}, 3},
	{0x10, (uint8_t []){0x0F}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x18}, 3},
	{0x01, (uint8_t []){0x01}, 1},
	{0x00, (uint8_t []){0x1E}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x43}, 3},
	{0x03, (uint8_t []){0x04}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x18}, 3},
	{0x3A, (uint8_t []){0x01}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x50}, 3},
	{0x05, (uint8_t []){0x08}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x00}, 3},
	{0xFF, (uint8_t []){0x20, 0x10, 0x50}, 3},
	{0x00, (uint8_t []){0xA6}, 1},
	{0x01, (uint8_t []){0xA6}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x00}, 3},
	{0xFF, (uint8_t []){0x20, 0x10, 0x50}, 3},
	{0x08, (uint8_t []){0x55}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x00}, 3},
	{0xFF, (uint8_t []){0x20, 0x10, 0x10}, 3},
	{0x0B, (uint8_t []){0x43}, 1},
	{0x0C, (uint8_t []){0x12}, 1},
	{0x10, (uint8_t []){0x01}, 1},
	{0x11, (uint8_t []){0x12}, 1},
	{0x15, (uint8_t []){0x00}, 1},
	{0x16, (uint8_t []){0x00}, 1},
	{0x1A, (uint8_t []){0x00}, 1},
	{0x1B, (uint8_t []){0x00}, 1},
	{0x61, (uint8_t []){0x00}, 1},
	{0x62, (uint8_t []){0x00}, 1},
	{0x51, (uint8_t []){0x11}, 1},
	{0x55, (uint8_t []){0x55}, 1},
	{0x58, (uint8_t []){0x00}, 1},
	{0x5C, (uint8_t []){0x00}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x10}, 3},
	{0x20, (uint8_t []){0x81}, 1},
	{0x21, (uint8_t []){0x82}, 1},
	{0x22, (uint8_t []){0x72}, 1},
	{0x30, (uint8_t []){0x00}, 1},
	{0x31, (uint8_t []){0x00}, 1},
	{0x32, (uint8_t []){0x00}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x10}, 3},
	{0x44, (uint8_t []){0x44}, 1},
	{0x45, (uint8_t []){0x55}, 1},
	{0x46, (uint8_t []){0x66}, 1},
	{0x47, (uint8_t []){0x77}, 1},
	{0x49, (uint8_t []){0x00}, 1},
	{0x4A, (uint8_t []){0x00}, 1},
	{0x4B, (uint8_t []){0x00}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x17}, 3},
	{0x37, (uint8_t []){0x00}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x15}, 3},
	{0x04, (uint8_t []){0x08}, 1},
	{0x05, (uint8_t []){0x04}, 1},
	{0x06, (uint8_t []){0x1C}, 1},
	{0x07, (uint8_t []){0x1A}, 1},
	{0x08, (uint8_t []){0x18}, 1},
	{0x09, (uint8_t []){0x16}, 1},
	{0x24, (uint8_t []){0x05}, 1},
	{0x25, (uint8_t []){0x09}, 1},
	{0x26, (uint8_t []){0x17}, 1},
	{0x27, (uint8_t []){0x19}, 1},
	{0x28, (uint8_t []){0x1B}, 1},
	{0x29, (uint8_t []){0x1D}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x16}, 3},
	{0x04, (uint8_t []){0x09}, 1},
	{0x05, (uint8_t []){0x05}, 1},
	{0x06, (uint8_t []){0x1D}, 1},
	{0x07, (uint8_t []){0x1B}, 1},
	{0x08, (uint8_t []){0x19}, 1},
	{0x09, (uint8_t []){0x17}, 1},
	{0x24, (uint8_t []){0x04}, 1},
	{0x25, (uint8_t []){0x08}, 1},
	{0x26, (uint8_t []){0x16}, 1},
	{0x27, (uint8_t []){0x18}, 1},
	{0x28, (uint8_t []){0x1A}, 1},
	{0x29, (uint8_t []){0x1C}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x18}, 3},
	{0x1F, (uint8_t []){0x02}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x11}, 3},
	{0x15, (uint8_t []){0x99}, 1},
	{0x16, (uint8_t []){0x99}, 1},
	{0x1C, (uint8_t []){0x88}, 1},
	{0x1D, (uint8_t []){0x88}, 1},
	{0x1E, (uint8_t []){0x88}, 1},
	{0x13, (uint8_t []){0xf0}, 1},
	{0x14, (uint8_t []){0x34}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x12}, 3},
	{0x12, (uint8_t []){0x89}, 1},
	{0x06, (uint8_t []){0x06}, 1},
	{0x18, (uint8_t []){0x00}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x11}, 3},
	{0x0A, (uint8_t []){0x00}, 1},
	{0x0B, (uint8_t []){0xF0}, 1},
	{0x0c, (uint8_t []){0xF0}, 1},
	{0x6A, (uint8_t []){0x10}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x00}, 3},
	{0xFF, (uint8_t []){0x20, 0x10, 0x11}, 3},
	{0x08, (uint8_t []){0x70}, 1},
	{0x09, (uint8_t []){0x00}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x00}, 3},
	{0x35, (uint8_t []){0x00}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x12}, 3},
	{0x21, (uint8_t []){0x70}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x2D}, 3},
	{0x02, (uint8_t []){0x00}, 1},
	{0xFF, (uint8_t []){0x20, 0x10, 0x00}, 3},
	{0x11, (uint8_t []){0x00}, 0},
	/* requires 120 ms delay at the end of init sequence */
};

static int spd2010_reset(const struct spd2010_config *cfg)
{
	int ret;

	if (!cfg->reset_gpio.port) {
		LOG_ERR("%s: error: no reset gpio defined", __FUNCTION__);
		return -ENODEV;
	}

	if (!gpio_is_ready_dt(&cfg->reset_gpio)) {
		LOG_ERR("%s: error: reset gpio not ready", __FUNCTION__);
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&cfg->reset_gpio, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		LOG_ERR("%s: error: configuring reset gpio", __FUNCTION__);
		return ret;
	}

	ret = gpio_pin_set_dt(&cfg->reset_gpio, 1);
	if (ret < 0) {
		LOG_ERR("%s: error: setting reset gpio high", __FUNCTION__);
		return ret;
	}
	k_msleep(10);

	ret = gpio_pin_set_dt(&cfg->reset_gpio, 0);
	if (ret < 0) {
		LOG_ERR("%s: error: setting reset gpio low", __FUNCTION__);
		return ret;
	}
	k_msleep(120);

	return ret;
}

static int spd2010_tx(const struct device *dev, uint8_t type, uint8_t cmd,
		const uint8_t *buffer, size_t len)
{
	const struct spd2010_config *cfg = dev->config;
	struct spd2010_data *data = dev->data;
	struct spi_config *spi_cfg = &data->spi_cfg;
	uint8_t cmd_buf[] = {type, 0x00, cmd, 0x00};
	struct spi_buf tx_buffers[2];
	struct spi_buf_set tx;
	int ret;

	tx_buffers[0].buf = cmd_buf;
	tx_buffers[0].len = sizeof(cmd_buf);
	tx_buffers[1].buf = (void *)buffer;
	tx_buffers[1].len = len;
	tx.buffers = tx_buffers;
	tx.count = (buffer && len > 0) ? 2 : 1;

	spi_cfg->operation |= SPI_LOCK_ON;
	ret = spi_write(cfg->spi_dev, spi_cfg, &tx);
	spi_release(cfg->spi_dev, spi_cfg);
	if (ret < 0)
		LOG_ERR("%s: error: cmd spi write: %d", __FUNCTION__, ret);

	return ret;
}

static int spd2010_tx_color(const struct device *dev, uint8_t cmd,
			const uint8_t *buffer, size_t len)
{
	/*
	 * Use single-lane write (0x02) instead of quad-lane (0x32).
	 * The Zephyr ESP32 SPI driver routes all bytes through the DATA phase
	 * with no separate CMD/ADDR phase, so SPI_LINES_QUAD sends the 0x32
	 * opcode itself in 4-lane mode. The SPD2010 expects the opcode in
	 * 1-lane mode and ignores transactions with a 4-lane opcode.
	 */
	return spd2010_tx(dev, SPD2010_QSPI_ONE_LANE_WRITE, cmd, buffer, len);
}

static int spd2010_tx_param(const struct device *dev, uint8_t cmd,
			const uint8_t *buffer, size_t len)
{
	return spd2010_tx(dev, SPD2010_QSPI_ONE_LANE_WRITE, cmd, buffer, len);
}

static __maybe_unused int spd2010_rx_param(const struct device *dev, uint8_t cmd,
			uint8_t *buffer, size_t len)
{
	const struct spd2010_config *cfg = dev->config;
	struct spd2010_data *data = dev->data;
	struct spi_config *spi_cfg = &data->spi_cfg;
	uint8_t cmd_buf[] = {SPD2010_QSPI_ONE_LANE_READ, 0x00, cmd, 0x00, 0x00};
	const struct spi_buf tx_buf[1] = {
		{.buf = cmd_buf, .len = ARRAY_SIZE(cmd_buf)}
	};
	const struct spi_buf rx_buf[1] = {{.buf = buffer, .len = len}};
	const struct spi_buf_set tx = {.buffers = tx_buf, .count = 1};
	const struct spi_buf_set rx = {.buffers = rx_buf, .count = 1};
	int ret;

	spi_cfg->operation |= SPI_LOCK_ON;
	ret = spi_write(cfg->spi_dev, spi_cfg, &tx);
	if (ret < 0) {
		LOG_ERR("%s: error: cmd spi write: %d", __FUNCTION__, ret);
		goto out;
	}

	ret = spi_read(cfg->spi_dev, spi_cfg, &rx);
	if (ret < 0)
		LOG_ERR("%s: error: cmd spi read: %d", __FUNCTION__, ret);

out:
	spi_release(cfg->spi_dev, spi_cfg);
	return ret;
}

static int spd2010_write(const struct device *dev,
			const uint16_t x, const uint16_t y,
			const struct display_buffer_descriptor *desc,
			const void *buf)
{
	int ret;
	uint16_t x_end = x + desc->width - 1;
	uint16_t y_end = y + desc->height - 1;

	ret = spd2010_tx_param(dev, SPD2010_CMD_SETCOL, (uint8_t[]){
				(x >> 8) & 0xFF,
				x & 0xFF,
				(x_end >> 8) & 0xFF,
				x_end & 0xFF,
			}, 4);
	if (ret < 0) {
		LOG_ERR("%s: error: setcol: %d", __FUNCTION__, ret);
		return ret;
	}

	ret = spd2010_tx_param(dev, SPD2010_CMD_SETPAGE, (uint8_t[]){
				(y >> 8) & 0xFF,
				y & 0xFF,
				(y_end >> 8) & 0xFF,
				y_end & 0xFF,
			}, 4);
	if (ret < 0) {
		LOG_ERR("%s: error: setpage: %d", __FUNCTION__, ret);
		return ret;
	}

	ret = spd2010_tx_color(dev, SPD2010_CMD_WRMEMST, buf, desc->buf_size);
	if (ret < 0) {
		LOG_ERR("%s: error: wrmemst: %d", __FUNCTION__, ret);
		return ret;
	}

	return 0;
}

static void spd2010_get_capabilities(const struct device *dev,
				struct display_capabilities *caps)
{
	struct spd2010_data *data = dev->data;

	memset(caps, 0, sizeof(struct display_capabilities));

	caps->x_resolution = data->width;
	caps->y_resolution = data->height;
	caps->supported_pixel_formats =
		PIXEL_FORMAT_RGB_565 |
		PIXEL_FORMAT_RGB_565X |
		PIXEL_FORMAT_RGB_888 |
		PIXEL_FORMAT_BGR_888;
	caps->current_pixel_format = data->pixel_format;
	caps->current_orientation = data->orientation;
}

static int spd2010_set_pixel_format(const struct device *dev,
				const enum display_pixel_format pixel_format)
{
	struct spd2010_data *data = dev->data;
	uint8_t bytes_per_pixel;
	int ret;

	if (pixel_format == PIXEL_FORMAT_RGB_565 ||
		pixel_format == PIXEL_FORMAT_RGB_565X) {
		bytes_per_pixel = 2U;
		data->reg_setpixel &= ~SPD2010_SETPIXEL_MASK;
		data->reg_setpixel |= SPD2010_SETPIXEL_65K;
	} else if (pixel_format == PIXEL_FORMAT_RGB_888 ||
		pixel_format == PIXEL_FORMAT_BGR_888) {
		bytes_per_pixel = 3U;
		data->reg_setpixel &= ~SPD2010_SETPIXEL_MASK;
		data->reg_setpixel |= SPD2010_SETPIXEL_16M;
	} else {
		LOG_ERR("Unsupported pixel format");
		return -ENOTSUP;
	}

	if (pixel_format == PIXEL_FORMAT_RGB_565X ||
		pixel_format == PIXEL_FORMAT_BGR_888) {
		data->reg_madctr |= SPD2010_MADCTR_BGR;
	} else {
		data->reg_madctr &= ~SPD2010_MADCTR_BGR;
	}

	data->pixel_format = pixel_format;
	data->bytes_per_pixel = bytes_per_pixel;

	ret = spd2010_tx_param(dev, SPD2010_CMD_SETPIXEL,
			&data->reg_setpixel, 1);
	if (ret < 0) {
		LOG_ERR("%s: error: setpixel register write: %d", __FUNCTION__, ret);
		return ret;
	}

	ret = spd2010_tx_param(dev, SPD2010_CMD_MADCTR, &data->reg_madctr, 1);
	if (ret < 0)
		LOG_ERR("%s: error: madctr register write: %d", __FUNCTION__, ret);

	return ret;
}

static int spd2010_set_orientation(const struct device *dev,
				const enum display_orientation orientation)
{
	struct spd2010_data *data = dev->data;
	int ret;

	switch (orientation) {
	case DISPLAY_ORIENTATION_NORMAL:
		data->reg_madctr &=
			~(SPD2010_MADCTR_H_FLIP | SPD2010_MADCTR_V_FLIP);
		break;
	case DISPLAY_ORIENTATION_ROTATED_90:
		data->reg_madctr |= SPD2010_MADCTR_H_FLIP;
		data->reg_madctr &= ~SPD2010_MADCTR_V_FLIP;
		break;
	case DISPLAY_ORIENTATION_ROTATED_180:
		data->reg_madctr &= ~SPD2010_MADCTR_H_FLIP;
		data->reg_madctr |= SPD2010_MADCTR_V_FLIP;
		break;
	case DISPLAY_ORIENTATION_ROTATED_270:
		data->reg_madctr |= SPD2010_MADCTR_H_FLIP;
		data->reg_madctr |= SPD2010_MADCTR_V_FLIP;
		break;
	default:
		LOG_ERR("Unsupported orientation format");
		return -ENOTSUP;
	}

	ret = spd2010_tx_param(dev, SPD2010_CMD_MADCTR, &data->reg_madctr, 1);
	if (ret < 0)
		LOG_ERR("%s: error: madctr register write: %d", __FUNCTION__, ret);

	data->orientation = orientation;

	return 0;
}

static int spd2010_display_blanking_off(const struct device *dev)
{
	return spd2010_tx_param(dev, SPD2010_CMD_DISPON, NULL, 0);
}

static int spd2010_display_blanking_on(const struct device *dev)
{
	return spd2010_tx_param(dev, SPD2010_CMD_DISPOFF, NULL, 0);
}

static void spd2010_update_saved_regs(const struct device *dev,
				const struct spd2010_init_cmd *c)
{
	struct spd2010_data *data = dev->data;
	static bool is_user_set = true;
	uint8_t *param = (uint8_t*)(c->data);

	if (is_user_set && (c->len > 0)) {
		switch (c->cmd) {
		case SPD2010_CMD_MADCTR:
			data->reg_madctr = param[0];
			break;
		case SPD2010_CMD_SETPIXEL:
			data->reg_setpixel = param[0];
			break;
		case SPD2010_CMD_SET:
			if (c->len > 2)
				is_user_set = (param[2] == SPD2010_CMD_SET_USER);
			break;
		}
	}
}

static int spd2010_init_lcd(const struct device *dev)
{
	size_t init_cmds_sz = sizeof(spd2010_init_cmds) /
		sizeof(struct spd2010_init_cmd);
	struct spd2010_data *data = dev->data;
	int ret;
	size_t i;

	ret = spd2010_tx_param(dev, SPD2010_CMD_SWRESET, NULL, 0);
	if (ret < 0)
		return ret;
	/* SPD2010 requires >=5ms after SWRESET before next command */
	k_msleep(5);

	ret = spd2010_tx_param(dev, SPD2010_CMD_SET,
			(uint8_t[]){SPD2010_CMD_SET_BYTE0,
				       SPD2010_CMD_SET_BYTE1,
				       SPD2010_CMD_SET_USER}, 3);
	if (ret < 0)
		return ret;

	data->reg_madctr = 0;
	ret = spd2010_tx_param(dev, SPD2010_CMD_MADCTR,
			(uint8_t[]){0}, 1);
	if (ret < 0)
		return ret;

	data->reg_setpixel = SPD2010_SETPIXEL_65K;
	ret = spd2010_tx_param(dev, SPD2010_CMD_SETPIXEL,
			(uint8_t[]){0x55}, 1);
	if (ret < 0)
		return ret;

	for (i = 0; i < init_cmds_sz; i++) {
		ret = spd2010_tx_param(dev, spd2010_init_cmds[i].cmd,
				spd2010_init_cmds[i].data,
				spd2010_init_cmds[i].len);
		if (ret < 0) {
			LOG_ERR("%s: error: init cmd[%zu]=0x%02x: %d",
				__FUNCTION__, i, spd2010_init_cmds[i].cmd, ret);
			return ret;
		}
		spd2010_update_saved_regs(dev, &spd2010_init_cmds[i]);
	}
	k_msleep(120);

	ret = spd2010_tx_param(dev, SPD2010_CMD_DISPON, NULL, 0);
	if (ret < 0)
		return ret;

	return ret;
}

static int spd2010_init(const struct device *dev)
{
	const struct spd2010_config *cfg = dev->config;
	int ret;

	if (!device_is_ready(cfg->spi_dev)) {
		LOG_ERR("%s: error: spi device not ready", __FUNCTION__);
		return -ENODEV;
	}

	ret = spd2010_reset(cfg);
	if (ret < 0)
		return ret;

	ret = spd2010_init_lcd(dev);

	return ret;
}

static DEVICE_API(display, spd2010_api) = {
	.blanking_on = spd2010_display_blanking_on,
	.blanking_off = spd2010_display_blanking_off,
	.write = spd2010_write,
	.get_capabilities = spd2010_get_capabilities,
	.set_pixel_format = spd2010_set_pixel_format,
	.set_orientation = spd2010_set_orientation,
};

#define SPD2010_DEFINE(n)						\
	static const struct spd2010_config config##n = {		\
		.spi_dev = DEVICE_DT_GET(DT_INST_PARENT(n)),		\
		.reset_gpio = GPIO_DT_SPEC_INST_GET_OR(n, reset_gpios, {0}), \
	};								\
									\
	static struct spd2010_data data##n = {				\
		.spi_cfg = SPI_CONFIG_DT_INST(n,			\
					SPI_OP_MODE_MASTER |		\
					SPI_TRANSFER_MSB |		\
					SPI_WORD_SET(8) |		\
					SPI_HOLD_ON_CS |		\
					SPI_LINES_SINGLE),		\
		.width = DT_INST_PROP(n, width),			\
		.height = DT_INST_PROP(n, height),			\
		.pixel_format = DT_INST_PROP(n, pixel_format),		\
	};								\
									\
	DEVICE_DT_INST_DEFINE(n,					\
			&spd2010_init,					\
			NULL,						\
			&data##n,					\
			&config##n,					\
			POST_KERNEL,					\
			CONFIG_DISPLAY_INIT_PRIORITY,			\
			&spd2010_api);

DT_INST_FOREACH_STATUS_OKAY(SPD2010_DEFINE)
