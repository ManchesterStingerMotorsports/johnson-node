#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tasks/CAN/CAN_task.hpp"
#include "tasks/IMU_GPS/IMU_GPS_task.hpp"

static const char *SYS_TAG = "JohnsonNode";
static constexpr gpio_num_t LED_GPIO = GPIO_NUM_18;

extern "C" void app_main(void)
{
    ESP_LOGI(SYS_TAG, "Hello World, creating tasks...");
    xTaskCreate(CAN_task, "CAN_task", 4096, nullptr, tskIDLE_PRIORITY + 1, nullptr);
    xTaskCreate(IMU_GPS_task, "IMU_GPS_task", 4096, nullptr, tskIDLE_PRIORITY + 1, nullptr);
    ESP_LOGI(SYS_TAG, "Tasks created.");

    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    int led_level = 0;

    while (true)
    {
        ESP_LOGI(SYS_TAG, "Blink!");

        led_level = !led_level;
        gpio_set_level(LED_GPIO, led_level);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
