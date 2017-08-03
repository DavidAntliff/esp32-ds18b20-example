#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>  // for PRIu64

#include "ds18b20.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define TAG "ds18b20"

// ROM commands
#define DS18B20_ROM_SEARCH        0xF0
#define DS18B20_ROM_READ          0x33
#define DS18B20_ROM_MATCH         0x55
#define DS18B20_ROM_SKIP          0xCC
#define DS18B20_ROM_SEARCH_ALARM  0xEC

// Function commands
#define DS18B20_FUNCTION_TEMP_CONVERT       0x44
#define DS18B20_FUNCTION_SCRATCHPAD_WRITE   0x4E
#define DS18B20_FUNCTION_SCRATCHPAD_READ    0xBE
#define DS18B20_FUNCTION_SCRATCHPAD_COPY    0x48
#define DS18B20_FUNCTION_EEPROM_RECALL      0xB8
#define DS18B20_FUNCTION_POWER_SUPPLY_READ  0xB4


struct _DS18B20_Timing
{
	int A, B, C, D, E, F, G, H, I, J;
};

// 1-Wire timing delays (standard) in ticks (quarter-microseconds).
static const struct _DS18B20_Timing _DS18B20_StandardTiming = {
	6 * 4,
	64 * 4,
	60 * 4,
	10 * 4,
	9 * 4,
	55 * 4,
	0,        // G
	480 * 4,  // H
	70 * 4,   // I
	410 * 4,  // J
};

struct _DS18B20_Info
{
	bool init;
	int gpio;
	const struct _DS18B20_Timing * timing;
};

static void _tick_delay(int ticks)
{
	// Each tick is 0.25 microseconds.
	float time_us = ticks / 4.0;
	ets_delay_us(time_us);
}

/**
 * @brief Generate a 1-Wire reset.
 * @param[in] ds18b20_info Initialised device info instance.
 * @return true if device is present, otherwise false.
 */
static bool _reset(DS18B20_Info * ds18b20_info)
{
	bool present = false;
	if (ds18b20_info != NULL)
	{
		if (ds18b20_info->init)
		{
			gpio_set_direction(ds18b20_info->gpio, GPIO_MODE_OUTPUT);

			_tick_delay(ds18b20_info->timing->G);
			gpio_set_level(ds18b20_info->gpio, 0);  // Drive DQ low
			_tick_delay(ds18b20_info->timing->H);
			gpio_set_level(ds18b20_info->gpio, 1);  // Release the bus
			_tick_delay(ds18b20_info->timing->I);

			gpio_set_direction(ds18b20_info->gpio, GPIO_MODE_INPUT);
			int level1 = gpio_get_level(ds18b20_info->gpio);
			_tick_delay(ds18b20_info->timing->J);   // Complete the reset sequence recovery
			int level2 = gpio_get_level(ds18b20_info->gpio);

			present = level1 == 0 && level2 == 1;   // Sample for presence pulse from slave
			ESP_LOGD(TAG, "reset: level1 0x%x, level2 0x%x, present %d", level1, level2, present);
		}
		else
		{
			ESP_LOGE(TAG, "ds18b20_info is not initialised");
		}
	}
	else
	{
		ESP_LOGE(TAG, "ds18b20_info is NULL");
	}
	return present;
}

/**
 * @brief Send a 1-Wire write bit, with recovery time.
 * @param[in] ds18b20_info Initialised device info instance.
 * @param[in] bit The value to send.
 */
static void _write_bit(DS18B20_Info * ds18b20_info, int bit)
{
	if (ds18b20_info != NULL)
	{
		if (ds18b20_info->init)
		{
			int delay1 = bit ? ds18b20_info->timing->A : ds18b20_info->timing->C;
			int delay2 = bit ? ds18b20_info->timing->B : ds18b20_info->timing->D;
			gpio_set_direction(ds18b20_info->gpio, GPIO_MODE_OUTPUT);
			gpio_set_level(ds18b20_info->gpio, 0);  // Drive DQ low
			_tick_delay(delay1);
			gpio_set_level(ds18b20_info->gpio, 1);  // Release the bus
			_tick_delay(delay2);
		}
		else
		{
			ESP_LOGE(TAG, "ds18b20_info is not initialised");
		}
	}
	else
	{
		ESP_LOGE(TAG, "ds18b20_info is NULL");
	}
}

/**
 * @brief Read a bit from the 1-Wire bus and return the value, with recovery time.
 * @param[in] ds18b20_info Initialised device info instance.
 */
static int _read_bit(DS18B20_Info * ds18b20_info)
{
	int result = 0;
	if (ds18b20_info != NULL)
	{
		if (ds18b20_info->init)
		{
			gpio_set_direction(ds18b20_info->gpio, GPIO_MODE_OUTPUT);
			gpio_set_level(ds18b20_info->gpio, 0);  // Drive DQ low
			_tick_delay(ds18b20_info->timing->A);
			gpio_set_level(ds18b20_info->gpio, 1);  // Release the bus
			_tick_delay(ds18b20_info->timing->E);

			gpio_set_direction(ds18b20_info->gpio, GPIO_MODE_INPUT);
			int level = gpio_get_level(ds18b20_info->gpio);
			_tick_delay(ds18b20_info->timing->F);   // Complete the timeslot and 10us recovery
			result = level & 0x01;
		}
		else
		{
			ESP_LOGE(TAG, "ds18b20_info is not initialised");
		}
	}
	else
	{
		ESP_LOGE(TAG, "ds18b20_info is NULL");
	}
	return result;
}

/**
 * @brief Write 1-Wire data byte.
 * @param[in] ds18b20_info Initialised device info instance.
 * @param[in] data Value to write.
 */
static void _write_byte(DS18B20_Info * ds18b20_info, uint8_t data)
{
	if (ds18b20_info != NULL)
	{
		if (ds18b20_info->init)
		{
			ESP_LOGD(TAG, "write 0x%02x", data);
			for (int i = 0; i < 8; ++i)
			{
				_write_bit(ds18b20_info, data & 0x01);
				data >>= 1;
			}
		}
		else
		{
			ESP_LOGE(TAG, "ds18b20_info is not initialised");
		}
	}
	else
	{
		ESP_LOGE(TAG, "ds18b20_info is NULL");
	}
}

/**
 * @brief Read 1-Wire data byte from device.
 * @param[in] ds18b20_info Initialised device info instance.
 * @return Byte value read from device.
 */
static uint8_t _read_byte(DS18B20_Info * ds18b20_info)
{
	uint8_t result = 0;

	if (ds18b20_info != NULL)
	{
		if (ds18b20_info->init)
		{
			for (int i = 0; i < 8; ++i)
			{
				result >>= 1;
				if (_read_bit(ds18b20_info))
				{
					result |= 0x80;
				}
			}
			ESP_LOGD(TAG, "read 0x%02x", result);
		}
		else
		{
			ESP_LOGE(TAG, "ds18b20_info is not initialised");
		}
	}
	else
	{
		ESP_LOGE(TAG, "ds18b20_info is NULL");
	}
	return result;
}

static uint8_t  * _read_block(DS18B20_Info * ds18b20_info, uint8_t * buffer, unsigned int len)
{
	for (int i = 0; i < len; ++i)
	{
		*buffer++ = _read_byte(ds18b20_info);
	}
	return buffer;
}

void ds18b20_init(DS18B20_Info * ds18b20_info, int gpio)
{
	if (ds18b20_info != NULL)
	{
		gpio_pad_select_gpio(gpio);
		ds18b20_info->gpio = gpio;
		ds18b20_info->timing = &_DS18B20_StandardTiming;
		ds18b20_info->init = true;
	}
	else
	{
		ESP_LOGE(TAG, "ds18b20_info is NULL");
	}
}

DS18B20_Info * ds18b20_new(void)
{
	DS18B20_Info * ds18b20 = malloc(sizeof(*ds18b20));
	if (ds18b20 != NULL)
	{
		memset(ds18b20, 0, sizeof(*ds18b20));
	}
	else
	{
		ESP_LOGE(TAG, "malloc failed");
	}

	return ds18b20;
}

static uint8_t _calc_crc(uint8_t crc, uint8_t data)
{
	// https://www.maximintegrated.com/en/app-notes/index.mvp/id/27
	static const uint8_t table[256] = {
		0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
		157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
		35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
		190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
		70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
		219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
		101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
		248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
		140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
		17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
		175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
		50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
		202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
		87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
		233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
		116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53
	};

	return table[crc ^ data];
}

uint64_t ds18b20_read_rom(DS18B20_Info * ds18b20_info)
{
	uint64_t rom_code = 0;
	if (ds18b20_info != NULL)
	{
		if (ds18b20_info->init)
		{
			if (_reset(ds18b20_info))
			{
				uint8_t buffer[8] = { 0 };
				_write_byte(ds18b20_info, DS18B20_ROM_READ);
				_read_block(ds18b20_info, buffer, 8);

				// device provides LSB first
				for (int i = 7; i >= 0; --i)
				{
					// watch out for integer promotion
					rom_code |= ((uint64_t)buffer[i] << (8 * i));
				}
				ESP_LOGD(TAG, "rom_code 0x%08" PRIx64, rom_code);

				// check CRC
				uint8_t crc = 0;
				for (int i = 0; i < 8; ++i)
				{
					crc = _calc_crc(crc, buffer[i]);
					ESP_LOGD(TAG, "crc 0x%02x", crc);
				}

			}
			else
			{
				ESP_LOGE(TAG, "ds18b20 device not responding");
			}
		}
		else
		{
			ESP_LOGE(TAG, "ds18b20_info is not initialised");
		}
	}
	else
	{
		ESP_LOGE(TAG, "ds18b20_info is NULL");
	}
	return rom_code;
}

float ds18b20_get_temp(DS18B20_Info * ds18b20_info)
{
	float temp = 0.0f;
	if (ds18b20_info != NULL)
	{
		if (ds18b20_info->init)
		{
			if (_reset(ds18b20_info))
			{
				_write_byte(ds18b20_info, DS18B20_ROM_SKIP);
				_write_byte(ds18b20_info, DS18B20_FUNCTION_TEMP_CONVERT);
				vTaskDelay(750 / portTICK_PERIOD_MS);

				// reset
				_reset(ds18b20_info);
				_write_byte(ds18b20_info, DS18B20_ROM_SKIP);
				_write_byte(ds18b20_info, DS18B20_FUNCTION_SCRATCHPAD_READ);

				// Without CRC:
				//uint8_t temp1 = _read_byte(ds18b20_info);
				//uint8_t temp2 = _read_byte(ds18b20_info);
				//_reset(ds18b20_info);  // terminate early

				// with CRC:
				uint8_t buffer[9];
				_read_block(ds18b20_info, buffer, 9);
				uint8_t crc = 0;
				for (int i = 0; i < 9; ++i)
				{
					crc = _calc_crc(crc, buffer[i]);
					ESP_LOGD(TAG, "crc 0x%02x", crc);
				}
				uint8_t temp1 = buffer[0];
				uint8_t temp2 = buffer[1];

				ESP_LOGD(TAG, "temp1 0x%02x, temp2 0x%02x", temp1, temp2);
				temp = (float)(((temp2 << 8) + temp1) >> 4);
			}
			else
			{
				ESP_LOGE(TAG, "ds18b20 device not responding");
			}
		}
		else
		{
			ESP_LOGE(TAG, "ds18b20_info is not initialised");
		}
	}
	else
	{
		ESP_LOGE(TAG, "ds18b20_info is NULL");
	}

	return temp;
}
