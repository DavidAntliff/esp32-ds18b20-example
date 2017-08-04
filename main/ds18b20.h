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

#ifndef DS18B20_H
#define DS18B20_H

#include "owb.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct _DS18B20_Info DS18B20_Info;

/**
 * @brief Construct a new device info instance.
 *        New instance should be initialised before calling other functions.
 * @return Pointer to new device info instance, or NULL if it cannot be created.
 */
DS18B20_Info * ds18b20_malloc(void);

/**
 * @brief Delete an existing device info instance.
 * @param[in] ds18b20_info Pointer to device info instance.
 * @param[in,out] ds18b20_info Pointer to device info instance that will be freed and set to NULL.
 */
void ds18b20_free(DS18B20_Info ** ds18b20_info);

/**
 * @brief Initialise a device info instance with the specified GPIO.
 * @param[in] ds18b20_info Pointer to device info instance.
 * @param[in] bus Pointer to initialised 1-Wire bus instance.
 * @param[in] rom_code Device-specific ROM code to identify a device on the bus.
 */
void ds18b20_init(DS18B20_Info * ds18b20_info, OneWireBus * bus, OneWireBus_ROMCode rom_code);

/**
 * @brief Enable or disable use of CRC checks on device communications.
 * @param[in] ds18b20_info Pointer to device info instance.
 * @param[in] use_crc True to enable CRC checks, false to disable.
 */
void ds18b20_use_crc(DS18B20_Info * ds18b20_info, bool use_crc);

/**
 * @brief Read 64-bit ROM code from device - only works when there is a single device on the bus.
 * @param[in] ds18b20_info Pointer to device info instance.
 * @return The 64-bit value read from the device's ROM.
 */
uint64_t ds18b20_read_rom(DS18B20_Info * ds18b20_info);

/**
 * @brief Get current temperature from device.
 * @param[in] ds18b20_info Pointer to device info instance. Must be initialised first.
 * @return The current temperature returned by the device, in degrees Celsius.
 */
float ds18b20_get_temp(DS18B20_Info * ds18b20_info);


#ifdef __cplusplus
}
#endif

#endif  // DS18B20_H
