// Expose structure for stack-based (static) allocation.

#ifndef DS18B20_STATIC_H
#define DS18B20_STATIC_H

#include <stdbool.h>
#include <stdint.h>

#include "ds18b20.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _DS18B20_Info
{
    bool init;
    bool use_crc;
    OneWireBus * bus;
    uint64_t rom_code;
};

typedef struct _DS18B20_Info DS18B20_Info_Static;

#ifdef __cplusplus
}
#endif

#endif  // DS18B20_STATIC_H
