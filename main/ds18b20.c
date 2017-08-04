#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>  // for PRIu64

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "sdkconfig.h"

#include "ds18b20.h"
#include "owb.h"

static const char * TAG = "ds18b20";


// Function commands
#define DS18B20_FUNCTION_TEMP_CONVERT       0x44
#define DS18B20_FUNCTION_SCRATCHPAD_WRITE   0x4E
#define DS18B20_FUNCTION_SCRATCHPAD_READ    0xBE
#define DS18B20_FUNCTION_SCRATCHPAD_COPY    0x48
#define DS18B20_FUNCTION_EEPROM_RECALL      0xB8
#define DS18B20_FUNCTION_POWER_SUPPLY_READ  0xB4


struct _DS18B20_Info
{
    bool init;
    bool use_crc;
    OneWireBus * bus;
    OneWireBus_ROMCode rom_code;
};

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
        ds18b20_info->init = true;
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
            vTaskDelay(750 / portTICK_PERIOD_MS);

            // reset
            owb_reset(bus);
            //owb_write_byte(bus, OWB_ROM_SKIP);
            owb_write_byte(bus, OWB_ROM_MATCH);
            owb_write_rom_code(bus, ds18b20_info->rom_code);
            owb_write_byte(bus, DS18B20_FUNCTION_SCRATCHPAD_READ);

            uint8_t temp_LSB = 0;
            uint8_t temp_MSB = 0;
            if (ds18b20_info->use_crc)
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
            temp = (float)(((temp_MSB << 8) + temp_LSB) >> 4);
        }
        else
        {
            ESP_LOGE(TAG, "ds18b20 device not responding");
        }
    }

    return temp;
}
