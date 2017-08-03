#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "ds18b20.h"


#define GPIO_DS18B20_0 (GPIO_NUM_4)


void app_main()
{
	DS18B20_Info * ds18b20_info = ds18b20_new();
	ds18b20_init(ds18b20_info, GPIO_DS18B20_0);

	uint64_t rom_code = ds18b20_read_rom(ds18b20_info);
	printf("ROM code = 0x%08" PRIx64 "\n", rom_code);

	while (1)
	{
		float temp = ds18b20_get_temp(ds18b20_info);
		printf("Temp = %.1f degrees C\n", temp);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
	}

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Restarting now.\n");
    fflush(stdout);
    esp_restart();
}
