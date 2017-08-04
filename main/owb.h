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

typedef struct _OneWireBus OneWireBus;

typedef union
{
    struct fields
    {
        uint8_t family[1];         // LSB - read/write first
        uint8_t serial_number[6];
        uint8_t crc[1];            // MSB
    } fields;

    uint8_t bytes[8];

} OneWireBus_ROMCode;

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

// TODO
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
 */
uint8_t owb_crc8_byte(uint8_t crc, uint8_t data);


// TODO
uint8_t owb_crc8_bytes(uint8_t crc, const uint8_t * data, size_t len);


// Search API
struct OneWireBus_SearchState
{
    OneWireBus_ROMCode rom_code;
    int last_discrepancy;
    int last_family_discrepancy;
    int last_device_flag;
};
typedef struct OneWireBus_SearchState OneWireBus_SearchState;

bool owb_search_first(const OneWireBus * bus, OneWireBus_SearchState * state);
bool owb_search_next(const OneWireBus * bus, OneWireBus_SearchState * state);

// TODO: maybe not useful?
uint64_t uint64_from_rom_code(OneWireBus_ROMCode rom_code);
OneWireBus_ROMCode rom_code_from_uint64(uint64_t rom_code);


#ifdef __cplusplus
}
#endif

#endif  // ONE_WIRE_BUS_H
