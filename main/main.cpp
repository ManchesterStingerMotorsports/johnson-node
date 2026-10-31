#include "data/ADCSample.hpp"
#include "tasks/ADC/ADC_task.hpp"
#include "tasks/CAN/CAN_task.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

extern "C" void app_main(void)
{
    QueueHandle_t adc_queue = xQueueCreate(16, sizeof(SensorData::ADCSample));
    ESP_ERROR_CHECK(adc_queue != nullptr ? ESP_OK : ESP_ERR_NO_MEM);

    const BaseType_t can_started = xTaskCreate(
        CAN_task, "CAN", 4'096, adc_queue, tskIDLE_PRIORITY + 2, nullptr);
    ESP_ERROR_CHECK(can_started == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    const BaseType_t adc_started = xTaskCreate(
        ADC_task, "ADC", 4'096, adc_queue, tskIDLE_PRIORITY + 1, nullptr);
    ESP_ERROR_CHECK(adc_started == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
