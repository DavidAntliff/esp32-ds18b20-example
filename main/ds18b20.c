/*
 * MIT License
 *
 * Copyright (c) 2017 David Antliff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/**
 * @file ds18b20.c
 *
 * Resolution is cached in the DS18B20_Info object to avoid querying the hardware
 * every time a temperature conversion is required. However this can result in the
 * cached value becoming inconsistent with the hardware value, so care must be taken.
 *
 */

#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "ds18b20.h"
#include "owb.h"

static const char * TAG = "ds18b20";
static const int T_CONV = 750;   // maximum conversion time at 12-bit resolution in milliseconds

// Function commands
#define DS18B20_FUNCTION_TEMP_CONVERT       0x44
#define DS18B20_FUNCTION_SCRATCHPAD_WRITE   0x4E
#define DS18B20_FUNCTION_SCRATCHPAD_READ    0xBE
#define DS18B20_FUNCTION_SCRATCHPAD_COPY    0x48
#define DS18B20_FUNCTION_EEPROM_RECALL      0xB8
#define DS18B20_FUNCTION_POWER_SUPPLY_READ  0xB4


typedef struct
{
    uint8_t temperature[2];    // [0] is LSB, [1] is MSB
    uint8_t trigger_high;
    uint8_t trigger_low;
    uint8_t configuration;
    uint8_t reserved[3];
    uint8_t crc;
} Scratchpad;

static bool _is_init(const DS18B20_Info * ds18b20_info)
{
    bool ok = false;
    if (ds18b20_info != NULL)
    {
        if (ds18b20_info->init)
        {
            // OK
            ok = true;
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
    return ok;
}

static bool _check_resolution(DS18B20_RESOLUTION resolution)
{
    return (resolution >= DS18B20_RESOLUTION_9_BIT) && (resolution <= DS18B20_RESOLUTION_12_BIT);
}

static float _decode_temp(uint8_t lsb, uint8_t msb, DS18B20_RESOLUTION resolution)
{
    float result = 0.0f;
    if (_check_resolution(resolution))
    {
        // masks to remove undefined bits from result
        static const uint8_t lsb_mask[4] = { ~0x03, ~0x02, ~0x01, ~0x00 };
        uint8_t lsb_masked = lsb_mask[resolution - DS18B20_RESOLUTION_9_BIT] & lsb;
        int16_t raw = (msb << 8) | lsb_masked;
        result = raw / 16.0f;
    }
    else
    {
        ESP_LOGE(TAG, "Unsupported resolution %d", resolution);
    }
    return result;
}

static size_t _min(size_t x, size_t y)
{
    return x > y ? y : x;
}

static Scratchpad _read_scratchpad(const DS18B20_Info * ds18b20_info, size_t count)
{
    count = _min(sizeof(Scratchpad), count);   // avoid overflow
    Scratchpad scratchpad = {0};
    ESP_LOGD(TAG, "scratchpad read %d bytes: ", count);
    owb_reset(ds18b20_info->bus);
    owb_write_byte(ds18b20_info->bus, OWB_ROM_MATCH);
    owb_write_rom_code(ds18b20_info->bus, ds18b20_info->rom_code);
    owb_write_byte(ds18b20_info->bus, DS18B20_FUNCTION_SCRATCHPAD_READ);
    owb_read_bytes(ds18b20_info->bus, (uint8_t *)&scratchpad, count);
    esp_log_buffer_hex(TAG, &scratchpad, count);
    return scratchpad;
}

static bool _write_scratchpad(const DS18B20_Info * ds18b20_info, const Scratchpad * scratchpad, bool verify)
{
    bool result = false;
    // Only bytes 2, 3 and 4 (trigger and configuration) can be written.
    // All three bytes MUST be written before the next reset to avoid corruption.
    if (_is_init(ds18b20_info))
    {
        owb_reset(ds18b20_info->bus);
        owb_write_byte(ds18b20_info->bus, OWB_ROM_MATCH);
        owb_write_rom_code(ds18b20_info->bus, ds18b20_info->rom_code);
        owb_write_byte(ds18b20_info->bus, DS18B20_FUNCTION_SCRATCHPAD_WRITE);
        owb_write_bytes(ds18b20_info->bus, (uint8_t *)&scratchpad->trigger_high, 3);
        ESP_LOGD(TAG, "scratchpad write 3 bytes:");
        esp_log_buffer_hex(TAG, &scratchpad->trigger_high, 3);
        result = true;

        if (verify)
        {
            Scratchpad read = _read_scratchpad(ds18b20_info, offsetof(Scratchpad, configuration) + 1);
            if (memcmp(&scratchpad->trigger_high, &read.trigger_high, 3) != 0)
            {
                ESP_LOGE(TAG, "scratchpad verify failed: "
                        "wrote {0x%02x, 0x%02x, 0x%02x}, "
                        "read {0x%02x, 0x%02x, 0x%02x}",
                        scratchpad->trigger_high, scratchpad->trigger_low, scratchpad->configuration,
                        read.trigger_high, read.trigger_low, read.configuration);
                result = false;
            }
        }
    }
    return result;
}

DS18B20_Info * ds18b20_malloc(void)
{
    DS18B20_Info * ds18b20_info = malloc(sizeof(*ds18b20_info));
    if (ds18b20_info != NULL)
    {
        memset(ds18b20_info, 0, sizeof(*ds18b20_info));
        ESP_LOGD(TAG, "malloc %p", ds18b20_info);
    }
    else
    {
        ESP_LOGE(TAG, "malloc failed");
    }

    return ds18b20_info;
}

void ds18b20_free(DS18B20_Info ** ds18b20_info)
{
    if (ds18b20_info != NULL && (*ds18b20_info != NULL))
    {
        ESP_LOGD(TAG, "free %p", *ds18b20_info);
        free(*ds18b20_info);
        *ds18b20_info = NULL;
    }
}

void ds18b20_init(DS18B20_Info * ds18b20_info, OneWireBus * bus, OneWireBus_ROMCode rom_code)
{
    if (ds18b20_info != NULL)
    {
        ds18b20_info->bus = bus;
        ds18b20_info->rom_code = rom_code;
        ds18b20_info->use_crc = false;
        ds18b20_info->resolution = DS18B20_RESOLUTION_INVALID;
        ds18b20_info->init = true;

        // read current resolution from device as it may not be power-on or factory default
        ds18b20_info->resolution = ds18b20_read_resolution(ds18b20_info);
    }
    else
    {
        ESP_LOGE(TAG, "ds18b20_info is NULL");
    }
}

void ds18b20_use_crc(DS18B20_Info * ds18b20_info, bool use_crc)
{
    if (_is_init(ds18b20_info))
    {
        ds18b20_info->use_crc = use_crc;
        ESP_LOGD(TAG, "use_crc %d", ds18b20_info->use_crc);
    }
}

bool ds18b20_set_resolution(DS18B20_Info * ds18b20_info, DS18B20_RESOLUTION resolution)
{
    bool result = false;
    if (_is_init(ds18b20_info))
    {
        if (_check_resolution(ds18b20_info->resolution))
        {
            // read scratchpad up to and including configuration register
            Scratchpad scratchpad = _read_scratchpad(ds18b20_info,
                    offsetof(Scratchpad, configuration) - offsetof(Scratchpad, temperature) + 1);

            // modify configuration register to set resolution
            uint8_t value = (((resolution - 1) & 0x03) << 5) | 0x1f;
            scratchpad.configuration = value;
            ESP_LOGD(TAG, "configuration value 0x%02x", value);

            // write bytes 2, 3 and 4 of scratchpad
            result = _write_scratchpad(ds18b20_info, &scratchpad, /* verify */ true);
            if (result)
            {
                ds18b20_info->resolution = resolution;
                ESP_LOGI(TAG, "Resolution set to %d bits", (int)resolution);
            }
            else
            {
                // Resolution change failed - update the info resolution with the value read from configuration
                ds18b20_info->resolution = ds18b20_read_resolution(ds18b20_info);
                ESP_LOGW(TAG, "Resolution consistency lost - refreshed from device: %d", ds18b20_info->resolution);
            }
        }
        else
        {
            ESP_LOGE(TAG, "Unsupported resolution %d", resolution);
        }
    }
    return result;
}

DS18B20_RESOLUTION ds18b20_read_resolution(DS18B20_Info * ds18b20_info)
{
    DS18B20_RESOLUTION resolution = DS18B20_RESOLUTION_INVALID;
    if (_is_init(ds18b20_info))
    {
        // read scratchpad up to and including configuration register
        Scratchpad scratchpad = _read_scratchpad(ds18b20_info,
                offsetof(Scratchpad, configuration) - offsetof(Scratchpad, temperature) + 1);

        resolution = ((scratchpad.configuration >> 5) & 0x03) + DS18B20_RESOLUTION_9_BIT;
        if (!_check_resolution(resolution))
        {
            ESP_LOGE(TAG, "invalid resolution read from device: 0x%02x", scratchpad.configuration);
            resolution = DS18B20_RESOLUTION_INVALID;
        }
        else
        {
            ESP_LOGI(TAG, "Resolution read as %d", resolution);
        }
    }
    return resolution;
}

static void _wait_for_conversion(DS18B20_RESOLUTION resolution)
{
    if (_check_resolution(resolution))
    {
        int divisor = 1 << (DS18B20_RESOLUTION_12_BIT - resolution);
        ESP_LOGD(TAG, "divisor %d", divisor);
        float max_conversion_time = (float)T_CONV / (float)divisor;
        int ticks = ceil(max_conversion_time / portTICK_PERIOD_MS);
        ESP_LOGD(TAG, "wait for conversion: %.3f ms, %d ticks", max_conversion_time, ticks);

        // wait at least this maximum conversion time
        vTaskDelay(ticks);
    }
}

float ds18b20_get_temp(DS18B20_Info * ds18b20_info)
{
    float temp = 0.0f;
    if (_is_init(ds18b20_info))
    {
        OneWireBus * bus = ds18b20_info->bus;
        if (owb_reset(bus))
        {
            //owb_write_byte(bus, OWB_ROM_SKIP);
            owb_write_byte(bus, OWB_ROM_MATCH);
            owb_write_rom_code(bus, ds18b20_info->rom_code);
            owb_write_byte(bus, DS18B20_FUNCTION_TEMP_CONVERT);

            _wait_for_conversion(ds18b20_info->resolution);

            // reset
            owb_reset(bus);
            //owb_write_byte(bus, OWB_ROM_SKIP);
            owb_write_byte(bus, OWB_ROM_MATCH);
            owb_write_rom_code(bus, ds18b20_info->rom_code);
            owb_write_byte(bus, DS18B20_FUNCTION_SCRATCHPAD_READ);

            uint8_t temp_LSB = 0;
            uint8_t temp_MSB = 0;
            if (!ds18b20_info->use_crc)
            {
                // Without CRC:
                temp_LSB = owb_read_byte(bus);
                temp_MSB = owb_read_byte(bus);
                owb_reset(bus);  // terminate early
            }
            else
            {
                // with CRC:
                uint8_t buffer[9];
                owb_read_bytes(bus, buffer, 9);

                temp_LSB = buffer[0];
                temp_MSB = buffer[1];

                if (owb_crc8_bytes(0, buffer, 9) != 0)
                {
                    ESP_LOGE(TAG, "CRC failed");
                    temp_LSB = temp_MSB = 0;
                }
            }

            ESP_LOGD(TAG, "temp_LSB 0x%02x, temp_MSB 0x%02x", temp_LSB, temp_MSB);
            temp = _decode_temp(temp_LSB, temp_MSB, ds18b20_info->resolution);
        }
        else
        {
            ESP_LOGE(TAG, "ds18b20 device not responding");
        }
    }

    return temp;
}
