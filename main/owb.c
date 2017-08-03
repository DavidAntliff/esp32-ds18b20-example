#include <stddef.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "sdkconfig.h"
#include "driver/gpio.h"

#include "owb.h"

#define TAG "owb"


struct _OneWireBus_Timing
{
	int A, B, C, D, E, F, G, H, I, J;
};

// 1-Wire timing delays (standard) in ticks (quarter-microseconds).
static const struct _OneWireBus_Timing _StandardTiming = {
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

struct _OneWireBus
{
	bool init;
	int gpio;
	const struct _OneWireBus_Timing * timing;
	bool use_crc;
};

static void _tick_delay(int ticks)
{
	// Each tick is 0.25 microseconds.
	float time_us = ticks / 4.0;
	ets_delay_us(time_us);
}

static bool _is_init(const OneWireBus * bus)
{
	bool ok = false;
	if (bus != NULL)
	{
		if (bus->init)
		{
			// OK
			ok = true;
		}
		else
		{
			ESP_LOGE(TAG, "bus is not initialised");
		}
	}
	else
	{
		ESP_LOGE(TAG, "bus is NULL");
	}
	return ok;
}


/**
 * @brief Generate a 1-Wire reset.
 * @param[in] bus Initialised bus instance.
 * @return true if device is present, otherwise false.
 */
static bool _reset(const OneWireBus * bus)
{
	bool present = false;
	if (_is_init(bus))
	{
		gpio_set_direction(bus->gpio, GPIO_MODE_OUTPUT);

		_tick_delay(bus->timing->G);
		gpio_set_level(bus->gpio, 0);  // Drive DQ low
		_tick_delay(bus->timing->H);
		gpio_set_level(bus->gpio, 1);  // Release the bus
		_tick_delay(bus->timing->I);

		gpio_set_direction(bus->gpio, GPIO_MODE_INPUT);
		int level1 = gpio_get_level(bus->gpio);
		_tick_delay(bus->timing->J);   // Complete the reset sequence recovery
		int level2 = gpio_get_level(bus->gpio);

		present = (level1 == 0) && (level2 == 1);   // Sample for presence pulse from slave
		ESP_LOGD(TAG, "reset: level1 0x%x, level2 0x%x, present %d", level1, level2, present);
	}
	return present;
}

/**
 * @brief Send a 1-Wire write bit, with recovery time.
 * @param[in] bus Initialised bus instance.
 * @param[in] bit The value to send.
 */
static void _write_bit(const OneWireBus * bus, int bit)
{
	if (_is_init(bus))
	{
		int delay1 = bit ? bus->timing->A : bus->timing->C;
		int delay2 = bit ? bus->timing->B : bus->timing->D;
		gpio_set_direction(bus->gpio, GPIO_MODE_OUTPUT);
		gpio_set_level(bus->gpio, 0);  // Drive DQ low
		_tick_delay(delay1);
		gpio_set_level(bus->gpio, 1);  // Release the bus
		_tick_delay(delay2);
	}
}

/**
 * @brief Read a bit from the 1-Wire bus and return the value, with recovery time.
 * @param[in] bus Initialised bus instance.
 */
static int _read_bit(const OneWireBus * bus)
{
	int result = 0;
	if (_is_init(bus))
	{
		gpio_set_direction(bus->gpio, GPIO_MODE_OUTPUT);
		gpio_set_level(bus->gpio, 0);  // Drive DQ low
		_tick_delay(bus->timing->A);
		gpio_set_level(bus->gpio, 1);  // Release the bus
		_tick_delay(bus->timing->E);

		gpio_set_direction(bus->gpio, GPIO_MODE_INPUT);
		int level = gpio_get_level(bus->gpio);
		_tick_delay(bus->timing->F);   // Complete the timeslot and 10us recovery
		result = level & 0x01;
	}
	return result;
}

/**
 * @brief Write 1-Wire data byte.
 * @param[in] bus Initialised bus instance.
 * @param[in] data Value to write.
 */
static void _write_byte(const OneWireBus * bus, uint8_t data)
{
	if (_is_init(bus))
	{
		ESP_LOGD(TAG, "write 0x%02x", data);
		for (int i = 0; i < 8; ++i)
		{
			_write_bit(bus, data & 0x01);
			data >>= 1;
		}
	}
}

/**
 * @brief Read 1-Wire data byte from  bus.
 * @param[in] bus Initialised bus instance.
 * @return Byte value read from bus.
 */
static uint8_t _read_byte(const OneWireBus * bus)
{
	uint8_t result = 0;
	if (_is_init(bus))
	{
		for (int i = 0; i < 8; ++i)
		{
			result >>= 1;
			if (_read_bit(bus))
			{
				result |= 0x80;
			}
		}
		ESP_LOGD(TAG, "read 0x%02x", result);
	}
	return result;
}

/**
 * @param Read a block of bytes from 1-Wire bus.
 * @param[in] bus Initialised bus instance.
 * @param[in,out] buffer Pointer to buffer to receive read data.
 * @param[in] len Number of bytes to read, must not exceed length of receive buffer.
 * @return Pointer to receive buffer.
 */
static uint8_t * _read_block(const OneWireBus * bus, uint8_t * buffer, unsigned int len)
{
	for (int i = 0; i < len; ++i)
	{
		*buffer++ = _read_byte(bus);
	}
	return buffer;
}

/**
 * @param Write a block of bytes from 1-Wire bus.
 * @param[in] bus Initialised bus instance.
 * @param[in] buffer Pointer to buffer to write data from.
 * @param[in] len Number of bytes to write.
 * @return Pointer to write buffer.
 */
static uint8_t * _write_block(const OneWireBus * bus, const uint8_t * buffer, unsigned int len)
{
	for (int i = 0; i < len; ++i)
	{
		_write_byte(bus, buffer[i]);
	}
	return buffer;
}

/**
 * @brief 1-Wire 8-bit CRC lookup.
 * @param[in] crc Starting CRC value. Pass in prior CRC to accumulate.
 * @param[in] data Byte to feed into CRC.
 * @return Resultant CRC value.
 */
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


// Public API

OneWireBus * owb_malloc()
{
	OneWireBus * bus = malloc(sizeof(*bus));
	if (bus != NULL)
	{
		memset(bus, 0, sizeof(*bus));
	}
	else
	{
		ESP_LOGE(TAG, "malloc failed");
	}
	return bus;
}

void owb_free(OneWireBus ** bus)
{
	if (bus != NULL && (*bus != NULL))
	{
		free(bus);
		*bus = NULL;
	}
}

void owb_init(OneWireBus * bus, int gpio)
{
	if (bus != NULL)
	{
		bus->gpio = gpio;
		bus->timing = &_StandardTiming;
		bus->init = true;

		// platform specific:
		gpio_pad_select_gpio(bus->gpio);
	}
	else
	{
		ESP_LOGE(TAG, "bus is NULL");
	}
}

void owb_use_crc(OneWireBus * bus, bool use_crc)
{
	if (_is_init(bus))
	{
		bus->use_crc = use_crc;
		ESP_LOGD(TAG, "use_crc %d", bus->use_crc);
	}
}

int owb_rom_search(OneWireBus * bus)
{
	// TODO
	return 0;
}

uint64_t owb_read_rom(const OneWireBus * bus)
{
	uint64_t rom_code = 0;
	if (_is_init(bus))
	{
		if (_reset(bus))
		{
			uint8_t buffer[8] = { 0 };
			_write_byte(bus, OWB_ROM_READ);
			_read_block(bus, buffer, 8);

			// device provides LSB first
			for (int i = 7; i >= 0; --i)
			{
				// watch out for integer promotion
				rom_code |= ((uint64_t)buffer[i] << (8 * i));
			}

			if (bus->use_crc)
			{
				// check CRC
				uint8_t crc = 0;
				for (int i = 0; i < 8; ++i)
				{
					crc = _calc_crc(crc, buffer[i]);
				}
				ESP_LOGD(TAG, "crc 0x%02x", crc);

				if (crc != 0)
				{
					ESP_LOGE(TAG, "CRC failed");
					rom_code = 0;
				}

				ESP_LOGD(TAG, "rom_code 0x%08" PRIx64, rom_code);
			}
		}
		else
		{
			ESP_LOGE(TAG, "ds18b20 device not responding");
		}
	}
	return rom_code;
}

bool owb_reset(const OneWireBus * bus)
{
	return _reset(bus);
}

void owb_write_byte(const OneWireBus * bus, uint8_t data)
{
	_write_byte(bus, data);
}

uint8_t owb_read_byte(const OneWireBus * bus)
{
	return _read_byte(bus);
}

uint8_t * owb_read_bytes(const OneWireBus * bus, uint8_t * buffer, unsigned int len)
{
	return _read_block(bus, buffer, len);
}

uint8_t * owb_write_bytes(const OneWireBus * bus, const uint8_t * buffer, unsigned int len)
{
	return _write_block(bus, buffer, len);
}

void owb_write_rom_code(const OneWireBus * bus, uint64_t rom_code)
{
	uint8_t buffer[sizeof(uint64_t)] = {0};
	for (int i = 0; i < sizeof(buffer); ++i)
	{
		// LSB first
		buffer[i] = rom_code & 0xFF;
		rom_code >>= 8;
	}
	_write_block(bus, buffer, sizeof(buffer));
}

uint8_t owb_crc8(uint8_t crc, uint8_t data)
{
	return _calc_crc(crc, data);
}

