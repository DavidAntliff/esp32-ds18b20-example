#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "ds18b20.h"


#define GPIO_DS18B20_0 (GPIO_NUM_4)


void app_main()
{
	// Create a 1-Wire bus
	OneWireBus * owb = owb_malloc();
	owb_init(owb, GPIO_DS18B20_0);
    owb_use_crc(owb, true);              // enable CRC check for ROM code

    //owb_search();    // find all connected devices
    //OneWireBusROMCode rom_code = 0x1162e87ccee28;
    uint64_t rom_code = owb_read_rom(owb);
	printf("1-Wire ROM code 0x%08" PRIx64 "\n", rom_code);

	DS18B20_Info * ds18b20_info = ds18b20_malloc();
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

    //ds18b20_free(&ds18b20_info);
    	owb_free(&owb);

    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
