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
 * @file owb.h
 * @brief Interface definitions for the 1-Wire bus component.
 *
 * This component provides structures and functions that are useful for communicating
 * with devices connected to a Maxim Integrated 1-Wire® bus via a single GPIO.
 *
 * Currently only externally powered devices are supported. Parasitic power is not supported.
 */

#ifndef ONE_WIRE_BUS_H
#define ONE_WIRE_BUS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


// ROM commands
#define OWB_ROM_SEARCH        0xF0
#define OWB_ROM_READ          0x33
#define OWB_ROM_MATCH         0x55
#define OWB_ROM_SKIP          0xCC
#define OWB_ROM_SEARCH_ALARM  0xEC

/**
 * @brief Structure containing 1-Wire bus information relevant to a single instance.
 */
typedef struct
{
    bool init;                                  ///< True if struct has been initialised, otherwise false.
    int gpio;                                   ///< Value of GPIO connected to 1-Wire bus
    const struct _OneWireBus_Timing * timing;   ///< Pointer to timing information
    bool use_crc;                               ///< True if CRC checks are to be used when retrieving information from a device on the bus
} OneWireBus;

/**
 * @brief Represents a 1-Wire ROM Code. This is a sequence of eight bytes, where
 *        the first byte is the family number, then the following 6 bytes form the
 *        serial number. The final byte is the CRC8 check byte.
 */
typedef union
{
    /// Provides access via field names
    struct fields
    {
        uint8_t family[1];         ///< family identifier (1 byte, LSB - read/write first)
        uint8_t serial_number[6];  ///< serial number (6 bytes)
        uint8_t crc[1];            ///< CRC check byte (1 byte, MSB - read/write last)
    } fields;                      ///< Provides access via field names

    uint8_t bytes[8];              ///< Provides raw byte access

} OneWireBus_ROMCode;

/**
 * @brief Represents the state of a device search on the 1-Wire bus.
 *
 *        Pass a pointer to this structure to owb_search_first() and
 *        owb_search_next() to iterate through detected devices on the bus.
 */
typedef struct
{
    OneWireBus_ROMCode rom_code;
    int last_discrepancy;
    int last_family_discrepancy;
    int last_device_flag;
} OneWireBus_SearchState;

/**
 * @brief Construct a new 1-Wire bus instance.
 *        New instance should be initialised before calling other functions.
 * @return Pointer to new bus instance, or NULL if it cannot be created.
 */
OneWireBus * owb_malloc(void);

/**
 * @brief Delete an existing device info instance.
 * @param[in] bus Pointer to bus instance.
 * @param[in,out] ds18b20_info Pointer to device info instance that will be freed and set to NULL.
 */
void owb_free(OneWireBus ** bus);

/**
 * @brief Initialise a 1-Wire bus instance with the specified GPIO.
 * @param[in] bus Pointer to bus instance.
 * @param[in] gpio GPIO number to associate with device.
 */
void owb_init(OneWireBus * bus, int gpio);

/**
 * @brief Enable or disable use of CRC checks on device communications.
 * @param[in] bus Pointer to initialised bus instance.
 * @param[in] use_crc True to enable CRC checks, false to disable.
 */
void owb_use_crc(OneWireBus * bus, bool use_crc);

/**
 * @brief Read ROM code from device - only works when there is a single device on the bus.
 * @param[in] bus Pointer to initialised bus instance.
 * @return The value read from the device's ROM.
 */
OneWireBus_ROMCode owb_read_rom(const OneWireBus * bus);

/**
 * @brief Verify the device specified by ROM code is present.
 * @param[in] bus Pointer to initialised bus instance.
 * @param[in] rom_code ROM code to verify.
 * @return true if device is present, false if not present.
 */
bool owb_verify_rom(const OneWireBus * bus, OneWireBus_ROMCode rom_code);

/**
 * @brief Reset the 1-Wire bus.
 * @param[in] bus Pointer to initialised bus instance.
 * @return True if at least one device is present on the bus.
 */
bool owb_reset(const OneWireBus * bus);

/**
 * @brief Write a single byte to the 1-Wire bus.
 * @param[in] bus Pointer to initialised bus instance.
 * @param[in] data Byte value to write to bus.
 */
void owb_write_byte(const OneWireBus * bus, uint8_t data);

/**
 * @brief Read a single byte from the 1-Wire bus.
 * @param[in] bus Pointer to initialised bus instance.
 * @return The byte value read from the bus.
 */
uint8_t owb_read_byte(const OneWireBus * bus);

/**
 * @brief Read a number of bytes from the 1-Wire bus.
 * @param[in] bus Pointer to initialised bus instance.
 * @param[in, out] buffer Pointer to buffer to receive read data.
 * @param[in] len Number of bytes to read, must not exceed length of receive buffer.
 * @return Pointer to receive buffer.
 */
uint8_t * owb_read_bytes(const OneWireBus * bus, uint8_t * buffer, size_t len);

/**
 * @brief Write a number of bytes to the 1-Wire bus.
 * @param[in] bus Pointer to initialised bus instance.
 * @param[in] buffer Pointer to buffer to write data from.
 * @param[in] len Number of bytes to write.
 * @return Pointer to write buffer.
 */
const uint8_t * owb_write_bytes(const OneWireBus * bus, const uint8_t * buffer, size_t len);

/**
 * @brief Write a ROM code to the 1-Wire bus ensuring LSB is sent first.
 * @param[in] bus Pointer to initialised bus instance.
 * @param[in] rom_code ROM code to write to bus.
 */
void owb_write_rom_code(const OneWireBus * bus, OneWireBus_ROMCode rom_code);

/**
 * @brief 1-Wire 8-bit CRC lookup.
 * @param[in] crc Starting CRC value. Pass in prior CRC to accumulate.
 * @param[in] data Byte to feed into CRC.
 * @return Resultant CRC value.
 *         Should be zero if last byte was the CRC byte and the CRC matches.
 */
uint8_t owb_crc8_byte(uint8_t crc, uint8_t data);

/**
 * @brief 1-Wire 8-bit CRC lookup with accumulation over a block of bytes.
 * @param[in] crc Starting CRC value. Pass in prior CRC to accumulate.
 * @param[in] data Array of bytes to feed into CRC.
 * @param[in] len Length of data array in bytes.
 * @return Resultant CRC value.
 *         Should be zero if last byte was the CRC byte and the CRC matches.
 */
uint8_t owb_crc8_bytes(uint8_t crc, const uint8_t * data, size_t len);

/**
 * @brief Locates the first device on the 1-Wire bus, if present.
 * @param[in] bus Pointer to initialised bus instance.
 * @param[in,out] state Pointer to an existing search state structure.
 * @return True if a device is found, false if no devices are found.
 *         If a device is found, the ROM Code can be obtained from the state.
 */
bool owb_search_first(const OneWireBus * bus, OneWireBus_SearchState * state);

/**
 * @brief Locates the next device on the 1-Wire bus, if present, starting from
 *        the provided state. Further calls will yield additional devices, if present.
 * @param[in] bus Pointer to initialised bus instance.
 * @param[in,out] state Pointer to an existing search state structure.
 * @return True if a device is found, false if no devices are found.
 *         If a device is found, the ROM Code can be obtained from the state.
 */
bool owb_search_next(const OneWireBus * bus, OneWireBus_SearchState * state);

/**
 * @brief Create a string representation of a ROM code.
 * @param[in] rom_code The ROM code to convert to string representation.
 * @param[out] buffer The destination for the string representation. It will be null terminated.
 * @param[in] len The length of the buffer in bytes. 64-bit ROM codes require 16 characters
 *                to represent as a string, plus a null terminator, for 17 bytes.
 */
char * owb_string_from_rom_code(OneWireBus_ROMCode rom_code, char * buffer, size_t len);


#ifdef __cplusplus
}
#endif

#endif  // ONE_WIRE_BUS_H
