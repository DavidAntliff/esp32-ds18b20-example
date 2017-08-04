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

#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

// Uncomment to enable static (stack-based) allocation of instances and avoid malloc/free.
//#define USE_STATIC 1

#ifdef USE_STATIC
#  include "owb_static.h"
#  include "ds18b20_static.h"
#else
#  include "owb.h"
#  include "ds18b20.h"
#endif


#define GPIO_DS18B20_0 (GPIO_NUM_4)
#define MAX_DEVICES (8)


void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);

    // Create a 1-Wire bus
#ifdef USE_STATIC
    OneWireBus_Static owb_static;        // static allocation
    OneWireBus * owb = &owb_static;
#else
    OneWireBus * owb = owb_malloc();     // heap allocation
#endif

    owb_init(owb, GPIO_DS18B20_0);
    owb_use_crc(owb, true);              // enable CRC check for ROM code

    // find all connected devices
    printf("Find devices:\n");
    OneWireBus_ROMCode device_rom_codes[MAX_DEVICES] = {0};
    int num_devices = 0;
    OneWireBus_SearchState search_state = {0};
    bool found = owb_search_first(owb, &search_state);
    while (found)
    {
        char rom_code_s[17];
        owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
        printf("  %d : %s\n", num_devices, rom_code_s);
        device_rom_codes[num_devices] = search_state.rom_code;
        ++num_devices;
        found = owb_search_next(owb, &search_state);
    }

    // known ROM codes (LSB first):
    OneWireBus_ROMCode known_device = { { {0x28}, {0xee, 0xcc, 0x87, 0x2e, 0x16, 0x01}, {0x00} } };
    char rom_code_s[17];
    owb_string_from_rom_code(known_device, rom_code_s, sizeof(rom_code_s));
    printf("Device %s is %s\n", rom_code_s, owb_verify_rom(owb, known_device) ? "present" : "not present");

    //uint64_t rom_code = 0x0001162e87ccee28;  // pink
    //uint64_t rom_code = 0xf402162c6149ee28;  // green
    //uint64_t rom_code = 0x1502162ca5b2ee28;  // orange
    //uint64_t rom_code = owb_read_rom(owb);
    //printf("1-Wire ROM code 0x%08" PRIx64 "\n", rom_code);

    // Create a DS18B20 device on the 1-Wire bus
#ifdef USE_STATIC
    DS18B20_Info_Static ds18b20_info_static;       // static allocation
    DS18B20_Info * ds18b20_info = &ds18b20_info_static;
    DS18B20_Info devices_static[MAX_DEVICES] = {0};
    DS18B20_Info * devices[MAX_DEVICES] = {0};
    for (int i = 0; i < MAX_DEVICES; ++i)
    {
        devices[i] = &(devices_static[i]);
    }
#else
    DS18B20_Info * ds18b20_info = ds18b20_malloc();  // heap allocation
    DS18B20_Info * devices[MAX_DEVICES] = {0};
#endif

    for (int i = 0; i < num_devices; ++i)
    {
#ifdef USE_STATIC
        DS18B20_Info * ds18b20_info = devices[i];
#else
        DS18B20_Info * ds18b20_info = ds18b20_malloc();  // heap allocation
        devices[i] = ds18b20_info;
#endif
//        uint64_t rom_code = 0;
//        for (int j = 7; j >= 0; --j)
//        {
//            rom_code |= ((uint64_t)device_rom_codes[i][j] << (8 * j));
//        }
//        printf("1-Wire ROM code 0x%08" PRIx64 "\n", rom_code);

        //ds18b20_init(ds18b20_info, owb, rom_code);     // associate with bus and device
        ds18b20_init(ds18b20_info, owb, device_rom_codes[i]); // associate with bus and device
        //ds18b20_init_solo(ds18b20_info, owb);          // only one device on bus
        ds18b20_use_crc(ds18b20_info, true);           // enable CRC check for temperature readings
    }

    while (1)
    {
        printf("\nTemperature readings (degrees C):\n");
        for (int i = 0; i < num_devices; ++i)
        {
            float temp = ds18b20_get_temp(devices[i]);
            printf("  %d: %.2f\n", i, temp);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    ds18b20_free(&ds18b20_info);
    owb_free(&owb);

    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
