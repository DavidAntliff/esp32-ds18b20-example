// Expose structure for stack-based (static) allocation.

#ifndef ONE_WIRE_BUS_STATIC_H
#define ONE_WIRE_BUS_STATIC_H

#include <stdbool.h>

#include "owb.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _OneWireBus
{
	bool init;
	int gpio;
	const struct _OneWireBus_Timing * timing;
	bool use_crc;
};

typedef struct _OneWireBus OneWireBus_Static;

#ifdef __cplusplus
}
#endif

#endif  // ONE_WIRE_BUS_STATIC_H
