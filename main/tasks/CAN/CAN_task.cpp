#include "CAN_task.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "CAN_task";

void CAN_task(void *pv_parameters)
{
    (void)pv_parameters;

    ESP_LOGI(TAG, "Started");

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
