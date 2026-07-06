#include "driver/gpio.h"
#include "config/BoardConfig.hpp"
#include "data/SensorQueues.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tasks/ADC/ADC_task.hpp"
#include "tasks/CAN/CAN_task.hpp"
#include "tasks/GPS/GPS_task.hpp"
#include "tasks/IMU/IMU_task.hpp"

namespace
{
    const char *TAG = "JohnsonNode";

    struct TaskDefinition
    {
        TaskFunction_t function;
        const char *name;
        uint32_t stack_size;
        UBaseType_t priority;
    };

    bool create_task(const TaskDefinition& task)
    {
        const BaseType_t result = xTaskCreate(
            task.function,
            task.name,
            task.stack_size,
            nullptr,
            task.priority,
            nullptr);

        if (result != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to create %s", task.name);
            return false;
        }

        ESP_LOGI(TAG, "Created %s", task.name);
        return true;
    }
}

extern "C" void app_main(void)
{
    ESP_ERROR_CHECK(SensorQueues::init());

    const TaskDefinition tasks[] = {
        {CAN_task, "CAN_task", 5'120, tskIDLE_PRIORITY + 3},
        {ADC_task, "ADC_task", 4'096, tskIDLE_PRIORITY + 2},
        {IMU_task, "IMU_task", 4'096, tskIDLE_PRIORITY + 2},
        {GPS_task, "GPS_task", 4'096, tskIDLE_PRIORITY + 2},
    };

    for (const TaskDefinition& task : tasks)
    {
        (void)create_task(task);
    }

    ESP_ERROR_CHECK(
        gpio_set_direction(BoardConfig::StatusLed::GPIO, GPIO_MODE_OUTPUT));

    bool led_is_on = false;
    while (true)
    {
        led_is_on = !led_is_on;
        ESP_ERROR_CHECK(
            gpio_set_level(BoardConfig::StatusLed::GPIO, led_is_on ? 1 : 0));
        vTaskDelay(
            pdMS_TO_TICKS(BoardConfig::StatusLed::BLINK_PERIOD_MS));
    }
}
