#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

//#define USE_STATIC 1

#ifdef USE_STATIC
#  include "owb_static.h"
#  include "ds18b20_static.h"
#else
#  include "owb.h"
#  include "ds18b20.h"
#endif


#define GPIO_DS18B20_0 (GPIO_NUM_4)


void app_main()
{
	// Create a 1-Wire bus
#ifdef USE_STATIC
	OneWireBus_Static owb_static;        // static allocation
	OneWireBus * owb = &owb_static;
#else
	OneWireBus * owb = owb_malloc();     // heap allocation
#endif

	owb_init(owb, GPIO_DS18B20_0);
    owb_use_crc(owb, true);              // enable CRC check for ROM code

    //owb_search();    // find all connected devices
    //OneWireBusROMCode rom_code = 0x1162e87ccee28;
    uint64_t rom_code = owb_read_rom(owb);
	printf("1-Wire ROM code 0x%08" PRIx64 "\n", rom_code);

	// Create a DS18B20 device on the 1-Wire bus
#ifdef USE_STATIC
	DS18B20_Info_Static ds18b20_info_static;       // static allocation
	DS18B20_Info * ds18b20_info = &ds18b20_info_static;
#else
	DS18B20_Info * ds18b20_info = ds18b20_malloc();  // heap allocation
#endif

	ds18b20_init(ds18b20_info, owb, rom_code);     // associate with bus and device
	//ds18b20_init_solo(ds18b20_info, owb);          // only one device on bus
	ds18b20_use_crc(ds18b20_info, true);           // enable CRC check for temperature readings

	while (1)
	{
		float temp = ds18b20_get_temp(ds18b20_info);
		printf("Temp %.1f degrees C\n", temp);
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
