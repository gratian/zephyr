/*
 * Copyright (c) 2026 Gratian Crisan
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT solomon_spd2010_touch

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/input/input.h>
#include <zephyr/input/input_touch.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <string.h>

LOG_MODULE_REGISTER(spd2010_touch, CONFIG_INPUT_LOG_LEVEL);

#define SPD2010_REG_STATUS_LENGTH    0x2000U
#define SPD2010_REG_HDP_DATA         0x0003U
#define SPD2010_REG_HDP_STATUS       0xFC02U
#define SPD2010_REG_CMD_CLEAR_INT    0x0200U
#define SPD2010_REG_CMD_CPU_START    0x0400U
#define SPD2010_REG_CMD_TOUCH_START  0x4600U
#define SPD2010_REG_CMD_POINT_MODE   0x5000U

#define SPD2010_HDP_HEADER_SIZE      4U
#define SPD2010_HDP_POINT_SIZE       6U
#define SPD2010_MAX_PACKET_BYTES     (SPD2010_HDP_HEADER_SIZE + (10U * SPD2010_HDP_POINT_SIZE))
#define SPD2010_CMD_DELAY_US         200U
#define SPD2010_RESET_ASSERT_MS      50U
#define SPD2010_RESET_RELEASE_MS     50U
#define SPD2010_HDP_DONE_MAX_RETRY   8U

struct spd2010_point {
	uint8_t id;
	uint16_t x;
	uint16_t y;
	uint8_t weight;
};

struct spd2010_status {
	bool pt_exist;
	bool gesture;
	bool aux;
	bool cpu_run;
	bool tic_in_cpu;
	bool tic_in_bios;
	uint16_t read_len;
};

struct spd2010_hdp_status {
	uint8_t status;
	uint16_t next_packet_len;
};

struct spd2010_config {
	struct input_touchscreen_common_config common;
	struct i2c_dt_spec i2c;
	struct gpio_dt_spec rst_gpio;
#ifdef CONFIG_INPUT_SPD2010_INTERRUPT
	struct gpio_dt_spec irq_gpio;
#endif
};

struct spd2010_data {
	const struct device *dev;
	struct k_work work;
#ifdef CONFIG_INPUT_SPD2010_INTERRUPT
	struct gpio_callback irq_gpio_cb;
#else
	struct k_timer timer;
#endif
	struct spd2010_point prev_points[CONFIG_INPUT_SPD2010_MAX_TOUCH_POINTS];
	uint8_t prev_count;
};

INPUT_TOUCH_STRUCT_CHECK(struct spd2010_config);

static int spd2010_i2c_write(const struct spd2010_config *cfg, uint16_t reg,
			     const uint8_t *buf, size_t len)
{
	uint8_t write_buf[2U + 4U];

	if (len > 4U) {
		return -EINVAL;
	}

	write_buf[0] = (uint8_t)(reg >> 8);
	write_buf[1] = (uint8_t)(reg & 0xFFU);
	if (len > 0U) {
		memcpy(&write_buf[2], buf, len);
	}

	return i2c_write_dt(&cfg->i2c, write_buf, len + 2U);
}

static int spd2010_i2c_write_read(const struct spd2010_config *cfg, uint16_t reg,
				  uint8_t *buf, size_t len)
{
	uint8_t reg_buf[2];

	reg_buf[0] = (uint8_t)(reg >> 8);
	reg_buf[1] = (uint8_t)(reg & 0xFFU);

	return i2c_write_read_dt(&cfg->i2c, reg_buf, sizeof(reg_buf), buf, len);
}

static int spd2010_write_cmd(const struct spd2010_config *cfg, uint16_t reg, uint16_t value)
{
	uint8_t payload[2] = {
		(uint8_t)(value & 0xFFU),
		(uint8_t)(value >> 8),
	};
	int ret;

	ret = spd2010_i2c_write(cfg, reg, payload, sizeof(payload));
	if (ret < 0) {
		return ret;
	}

	k_busy_wait(SPD2010_CMD_DELAY_US);
	return 0;
}

static int spd2010_read_status(const struct spd2010_config *cfg, struct spd2010_status *status)
{
	uint8_t raw[4];
	int ret;

	ret = spd2010_i2c_write_read(cfg, SPD2010_REG_STATUS_LENGTH, raw, sizeof(raw));
	if (ret < 0) {
		return ret;
	}

	status->pt_exist = (raw[0] & BIT(0)) != 0U;
	status->gesture = (raw[0] & BIT(1)) != 0U;
	status->aux = (raw[0] & BIT(3)) != 0U;
	status->cpu_run = (raw[1] & BIT(3)) != 0U;
	status->tic_in_cpu = (raw[1] & BIT(5)) != 0U;
	status->tic_in_bios = (raw[1] & BIT(6)) != 0U;
	status->read_len = ((uint16_t)raw[3] << 8) | raw[2];

	k_busy_wait(SPD2010_CMD_DELAY_US);
	return 0;
}

static int spd2010_read_hdp_status(const struct spd2010_config *cfg, struct spd2010_hdp_status *status)
{
	uint8_t raw[8];
	int ret;

	ret = spd2010_i2c_write_read(cfg, SPD2010_REG_HDP_STATUS, raw, sizeof(raw));
	if (ret < 0) {
		return ret;
	}

	status->status = raw[5];
	status->next_packet_len = ((uint16_t)raw[3] << 8) | raw[2];

	return 0;
}

static int spd2010_read_hdp_remain(const struct spd2010_config *cfg, uint16_t len)
{
	uint8_t raw[SPD2010_MAX_PACKET_BYTES];

	if (len == 0U) {
		return -EINVAL;
	}

	len = MIN(len, (uint16_t)sizeof(raw));

	return spd2010_i2c_write_read(cfg, SPD2010_REG_HDP_DATA, raw, len);
}

static int spd2010_read_hdp(const struct spd2010_config *cfg, const struct spd2010_status *status,
			    struct spd2010_point *points, uint8_t *num_points)
{
	uint8_t raw[SPD2010_MAX_PACKET_BYTES];
	size_t i;
	size_t max_parse_points;
	size_t parsed_points;
	int ret;
	uint8_t packet_id;
	uint16_t read_len = status->read_len;

	*num_points = 0U;

	if (read_len < (SPD2010_HDP_HEADER_SIZE + 1U)) {
		return 0;
	}

	if (read_len > sizeof(raw)) {
		read_len = sizeof(raw);
	}

	ret = spd2010_i2c_write_read(cfg, SPD2010_REG_HDP_DATA, raw, read_len);
	if (ret < 0) {
		return ret;
	}

	packet_id = raw[4];
	if ((packet_id > 0x0AU) || !status->pt_exist) {
		return 0;
	}

	parsed_points = (read_len - SPD2010_HDP_HEADER_SIZE) / SPD2010_HDP_POINT_SIZE;
	max_parse_points = MIN(parsed_points, (size_t)CONFIG_INPUT_SPD2010_MAX_TOUCH_POINTS);

	for (i = 0U; i < max_parse_points; i++) {
		size_t base = SPD2010_HDP_HEADER_SIZE + (i * SPD2010_HDP_POINT_SIZE);
		struct spd2010_point pt;

		pt.id = raw[base + 0U];
		pt.x = ((uint16_t)(raw[base + 3U] & 0xF0U) << 4) | raw[base + 1U];
		pt.y = ((uint16_t)(raw[base + 3U] & 0x0FU) << 8) | raw[base + 2U];
		pt.weight = raw[base + 4U];

		if (pt.weight == 0U) {
			continue;
		}

		points[*num_points] = pt;
		(*num_points)++;
	}

	return 0;
}

static void spd2010_report_points(const struct device *dev, struct spd2010_data *data,
				  const struct spd2010_point *points, uint8_t count)
{
	uint8_t i;
	uint8_t j;

	for (i = 0U; i < count; i++) {
		uint8_t slot = points[i].id % CONFIG_INPUT_SPD2010_MAX_TOUCH_POINTS;

		if (CONFIG_INPUT_SPD2010_MAX_TOUCH_POINTS > 1) {
			input_report_abs(dev, INPUT_ABS_MT_SLOT, slot, true, K_FOREVER);
		}

		input_touchscreen_report_pos(dev, points[i].x, points[i].y, K_FOREVER);
		input_report_key(dev, INPUT_BTN_TOUCH, 1, true, K_FOREVER);
	}

	for (i = 0U; i < data->prev_count; i++) {
		for (j = 0U; j < count; j++) {
			if (data->prev_points[i].id == points[j].id) {
				break;
			}
		}

		if (j != count) {
			continue;
		}

		if (CONFIG_INPUT_SPD2010_MAX_TOUCH_POINTS > 1) {
			uint8_t slot = data->prev_points[i].id % CONFIG_INPUT_SPD2010_MAX_TOUCH_POINTS;

			input_report_abs(dev, INPUT_ABS_MT_SLOT, slot, true, K_FOREVER);
		}
		input_touchscreen_report_pos(dev, data->prev_points[i].x, data->prev_points[i].y,
					     K_FOREVER);
		input_report_key(dev, INPUT_BTN_TOUCH, 0, true, K_FOREVER);
	}

	memcpy(data->prev_points, points, sizeof(data->prev_points));
	data->prev_count = count;
}

static int spd2010_process(const struct device *dev)
{
	const struct spd2010_config *cfg = dev->config;
	struct spd2010_data *data = dev->data;
	struct spd2010_status status = {0};
	struct spd2010_hdp_status hdp_status;
	struct spd2010_point points[CONFIG_INPUT_SPD2010_MAX_TOUCH_POINTS] = {0};
	uint8_t num_points = 0U;
	uint8_t retries = SPD2010_HDP_DONE_MAX_RETRY;
	int ret;

	ret = spd2010_read_status(cfg, &status);
	if (ret < 0) {
		return ret;
	}

	if (status.tic_in_bios) {
		ret = spd2010_write_cmd(cfg, SPD2010_REG_CMD_CLEAR_INT, 0x0001U);
		if (ret < 0) {
			return ret;
		}
		return spd2010_write_cmd(cfg, SPD2010_REG_CMD_CPU_START, 0x0001U);
	}

	if (status.tic_in_cpu) {
		ret = spd2010_write_cmd(cfg, SPD2010_REG_CMD_POINT_MODE, 0x0000U);
		if (ret < 0) {
			return ret;
		}
		ret = spd2010_write_cmd(cfg, SPD2010_REG_CMD_TOUCH_START, 0x0000U);
		if (ret < 0) {
			return ret;
		}
		return spd2010_write_cmd(cfg, SPD2010_REG_CMD_CLEAR_INT, 0x0001U);
	}

	if (status.cpu_run && (status.read_len == 0U)) {
		return spd2010_write_cmd(cfg, SPD2010_REG_CMD_CLEAR_INT, 0x0001U);
	}

	if (status.pt_exist || status.gesture) {
		ret = spd2010_read_hdp(cfg, &status, points, &num_points);
		if (ret < 0) {
			return ret;
		}

		while (retries-- > 0U) {
			ret = spd2010_read_hdp_status(cfg, &hdp_status);
			if (ret < 0) {
				return ret;
			}

			if (hdp_status.status == 0x82U) {
				ret = spd2010_write_cmd(cfg, SPD2010_REG_CMD_CLEAR_INT, 0x0001U);
				if (ret < 0) {
					return ret;
				}
				break;
			}

			if ((hdp_status.status != 0x00U) || (hdp_status.next_packet_len == 0U)) {
				break;
			}

			ret = spd2010_read_hdp_remain(cfg, hdp_status.next_packet_len);
			if (ret < 0) {
				return ret;
			}
		}

		spd2010_report_points(dev, data, points, num_points);
		return 0;
	}

	if (status.cpu_run && status.aux) {
		return spd2010_write_cmd(cfg, SPD2010_REG_CMD_CLEAR_INT, 0x0001U);
	}

	if (data->prev_count > 0U) {
		spd2010_report_points(dev, data, points, 0U);
	}

	return 0;
}

static void spd2010_work_handler(struct k_work *work)
{
	struct spd2010_data *data = CONTAINER_OF(work, struct spd2010_data, work);
	int ret;

	ret = spd2010_process(data->dev);
	if (ret < 0) {
		LOG_DBG("touch process failed: %d", ret);
	}
}

#ifdef CONFIG_INPUT_SPD2010_INTERRUPT
static void spd2010_submit_work_once(struct spd2010_data *data)
{
	/* Avoid ISR-driven workqueue flooding when IRQ line chatters or stays asserted. */
	if (k_work_busy_get(&data->work) == 0U) {
		k_work_submit(&data->work);
	}
}

static void spd2010_isr_handler(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
	struct spd2010_data *data = CONTAINER_OF(cb, struct spd2010_data, irq_gpio_cb);

	ARG_UNUSED(port);
	ARG_UNUSED(pins);
	spd2010_submit_work_once(data);
}
#else
static void spd2010_timer_handler(struct k_timer *timer)
{
	struct spd2010_data *data = CONTAINER_OF(timer, struct spd2010_data, timer);

	k_work_submit(&data->work);
}
#endif

static int spd2010_reset(const struct spd2010_config *cfg)
{
	int ret;

	if (cfg->rst_gpio.port == NULL) {
		return 0;
	}

	if (!gpio_is_ready_dt(&cfg->rst_gpio)) {
		LOG_ERR_DEVICE_NOT_READY(cfg->rst_gpio.port);
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&cfg->rst_gpio, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Could not configure reset GPIO");
		return ret;
	}

	k_sleep(K_MSEC(SPD2010_RESET_ASSERT_MS));
	ret = gpio_pin_set_dt(&cfg->rst_gpio, 0);
	if (ret < 0) {
		return ret;
	}
	k_sleep(K_MSEC(SPD2010_RESET_RELEASE_MS));

	return 0;
}

static int spd2010_init(const struct device *dev)
{
	const struct spd2010_config *cfg = dev->config;
	struct spd2010_data *data = dev->data;
	int ret;

	if (!i2c_is_ready_dt(&cfg->i2c)) {
		LOG_ERR_DEVICE_NOT_READY(cfg->i2c.bus);
		return -ENODEV;
	}

	data->dev = dev;
	k_work_init(&data->work, spd2010_work_handler);

	ret = spd2010_reset(cfg);
	if (ret < 0) {
		return ret;
	}

#ifdef CONFIG_INPUT_SPD2010_INTERRUPT
	if (!gpio_is_ready_dt(&cfg->irq_gpio)) {
		LOG_ERR_DEVICE_NOT_READY(cfg->irq_gpio.port);
		return -ENODEV;
	}

	ret = gpio_pin_configure_dt(&cfg->irq_gpio, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("Could not configure interrupt GPIO pin");
		return ret;
	}

	/*
	 * SPD2010 IRQ is active-low on this platform. Keep a pull-up enabled to
	 * stabilize the line and avoid a floating interrupt input.
	 */
	ret = gpio_pin_configure_dt(&cfg->irq_gpio, GPIO_INPUT | GPIO_PULL_UP);
	if (ret < 0) {
		LOG_ERR("Could not configure interrupt GPIO pull-up");
		return ret;
	}

	/*
	 * Use edge-to-active interrupts to avoid repeated triggering while the line
	 * remains asserted. Any pending assertion at init is handled below.
	 */
	ret = gpio_pin_interrupt_configure_dt(&cfg->irq_gpio, GPIO_INT_EDGE_TO_ACTIVE);
	if (ret < 0) {
		LOG_ERR("Could not configure interrupt GPIO");
		return ret;
	}

	gpio_init_callback(&data->irq_gpio_cb, spd2010_isr_handler, BIT(cfg->irq_gpio.pin));
	ret = gpio_add_callback(cfg->irq_gpio.port, &data->irq_gpio_cb);
	if (ret < 0) {
		LOG_ERR("Could not add GPIO callback");
		return ret;
	}

	/* Service any pending touch IRQ that was active before callback setup. */
	if (gpio_pin_get_dt(&cfg->irq_gpio) > 0) {
		spd2010_submit_work_once(data);
	}
#else
	k_timer_init(&data->timer, spd2010_timer_handler, NULL);
	k_timer_start(&data->timer, K_MSEC(CONFIG_INPUT_SPD2010_INIT_DELAY_MS),
		      K_MSEC(CONFIG_INPUT_SPD2010_PERIOD_MS));
#endif

	ret = spd2010_process(dev);
	if (ret < 0) {
		LOG_WRN("Initial SPD2010 poll failed: %d", ret);
	}

	return 0;
}

#ifdef CONFIG_INPUT_SPD2010_INTERRUPT
#define SPD2010_IRQ_INIT(n) .irq_gpio = GPIO_DT_SPEC_INST_GET_OR(n, irq_gpios, {0}),
#else
#define SPD2010_IRQ_INIT(n)
#endif

#define SPD2010_RST_INIT(n) GPIO_DT_SPEC_INST_GET_OR(n, rst_gpios, {0})

#define SPD2010_DEFINE(n)                                                                           \
	static const struct spd2010_config spd2010_config_##n = {                                    \
		.common = INPUT_TOUCH_DT_INST_COMMON_CONFIG_INIT(n),                                   \
		.i2c = I2C_DT_SPEC_INST_GET(n),                                                       \
		.rst_gpio = SPD2010_RST_INIT(n),                                                      \
		SPD2010_IRQ_INIT(n)};                                                                  \
                                                                                                   \
	static struct spd2010_data spd2010_data_##n;                                                   \
                                                                                                   \
	DEVICE_DT_INST_DEFINE(n, spd2010_init, NULL, &spd2010_data_##n, &spd2010_config_##n,          \
			      POST_KERNEL, CONFIG_INPUT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(SPD2010_DEFINE)
