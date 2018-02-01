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

#include "owb.h"
#include "owb_rmt.h"
#include "ds18b20.h"


#define GPIO_DS18B20_0       (CONFIG_ONE_WIRE_GPIO)
#define MAX_DEVICES          (8)
#define DS18B20_RESOLUTION   (DS18B20_RESOLUTION_12_BIT)
#define SAMPLE_PERIOD        (1000)   // milliseconds

void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);

    // Stable readings require a brief period before communication
    vTaskDelay(2000.0 / portTICK_PERIOD_MS);

    // Create a 1-Wire bus
//#ifdef USE_STATIC
//    OneWireBus owb_static;        // static allocation
//    OneWireBus * owb = &owb_static;
//#else
//    OneWireBus * owb = owb_malloc();     // heap allocation
//#endif
    OneWireBus * owb;
    owb_rmt_driver_info rmt_driver_info;
    owb = owb_rmt_initialize(&rmt_driver_info, GPIO_DS18B20_0, RMT_CHANNEL_1, RMT_CHANNEL_0);

//    owb_init(owb, GPIO_DS18B20_0);
    owb_use_crc(owb, true);              // enable CRC check for ROM code

    // Find all connected devices
    printf("Find devices:\n");
    OneWireBus_ROMCode device_rom_codes[MAX_DEVICES] = {0};
    int num_devices = 0;
    OneWireBus_SearchState search_state = {0};
    bool found = false;
    owb_search_first(owb, &search_state, &found);
    while (found)
    {
        char rom_code_s[17];
        owb_string_from_rom_code(search_state.rom_code, rom_code_s, sizeof(rom_code_s));
        printf("  %d : %s\n", num_devices, rom_code_s);
        device_rom_codes[num_devices] = search_state.rom_code;
        ++num_devices;
        owb_search_next(owb, &search_state, &found);
    }

    printf("Found %d devices\n", num_devices);

    //uint64_t rom_code = 0x0001162e87ccee28;  // pink
    //uint64_t rom_code = 0xf402162c6149ee28;  // green
    //uint64_t rom_code = 0x1502162ca5b2ee28;  // orange
    //uint64_t rom_code = owb_read_rom(owb);

    // Known ROM code (LSB first):
    OneWireBus_ROMCode known_device = {
        .fields.family = { 0x28 },
        .fields.serial_number = { 0xee, 0xcc, 0x87, 0x2e, 0x16, 0x01 },
        .fields.crc = { 0x00 },
    };
    char rom_code_s[17];
    owb_string_from_rom_code(known_device, rom_code_s, sizeof(rom_code_s));
    bool is_present = false;
    owb_verify_rom(owb, known_device, &is_present);
    printf("Device %s is %s\n", rom_code_s, is_present ? "present" : "not present");

    // Create a DS18B20 device on the 1-Wire bus
#ifdef USE_STATIC
    DS18B20_Info devices_static[MAX_DEVICES] = {0};
    DS18B20_Info * devices[MAX_DEVICES] = {0};
    for (int i = 0; i < MAX_DEVICES; ++i)
    {
        devices[i] = &(devices_static[i]);
    }
#else
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
        if (num_devices == 1)
        {
            printf("Single device optimisations enabled\n");
            ds18b20_init_solo(ds18b20_info, owb);          // only one device on bus
        }
        else
        {
            ds18b20_init(ds18b20_info, owb, device_rom_codes[i]); // associate with bus and device
        }
        ds18b20_use_crc(ds18b20_info, true);           // enable CRC check for temperature readings
        ds18b20_set_resolution(ds18b20_info, DS18B20_RESOLUTION);
    }

//    // Read temperatures from all sensors sequentially
//    while (1)
//    {
//        printf("\nTemperature readings (degrees C):\n");
//        for (int i = 0; i < num_devices; ++i)
//        {
//            float temp = ds18b20_get_temp(devices[i]);
//            printf("  %d: %.3f\n", i, temp);
//        }
//        vTaskDelay(1000 / portTICK_PERIOD_MS);
//    }

    // Read temperatures more efficiently by starting conversions on all devices at the same time
    int errors_count[MAX_DEVICES] = {0};
    int sample_count = 0;
    if (num_devices > 0)
    {
        TickType_t last_wake_time = xTaskGetTickCount();

        while (1)
        {
            last_wake_time = xTaskGetTickCount();

            ds18b20_convert_all(owb);

            // In this application all devices use the same resolution,
            // so use the first device to determine the delay
            ds18b20_wait_for_conversion(devices[0]);

            // Read the results immediately after conversion otherwise it may fail
            // (using printf before reading may take too long)
            float readings[MAX_DEVICES] = { 0 };
            DS18B20_ERROR errors[MAX_DEVICES] = { 0 };

            for (int i = 0; i < num_devices; ++i)
            {
                errors[i] = ds18b20_read_temp(devices[i], &readings[i]);
            }

            // Print results in a separate loop, after all have been read
            printf("\nTemperature readings (degrees C): sample %d\n", ++sample_count);
            for (int i = 0; i < num_devices; ++i)
            {
                if (errors[i] != DS18B20_OK)
                {
                    ++errors_count[i];
                }

                printf("  %d: %.1f    %d errors\n", i, readings[i], errors_count[i]);
            }

            vTaskDelayUntil(&last_wake_time, SAMPLE_PERIOD / portTICK_PERIOD_MS);
        }
    }

#ifndef USE_STATIC
    // clean up dynamically allocated data
    for (int i = 0; i < num_devices; ++i)
    {
        ds18b20_free(&devices[i]);
    }
//    owb_free(&owb);
#endif

    printf("Restarting now.\n");
    fflush(stdout);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();
}
