//Example code

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

static const char* SYS_TAG = "SYSTEM"

extern "C" void app_main(void)
{
    ESP_LOGI(SYS_TAG, "Hello world!\n");
}
