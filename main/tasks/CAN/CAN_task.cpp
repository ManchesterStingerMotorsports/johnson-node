#include "CAN_task.hpp"
#include "MSM_CAN.hpp"
#include "config/BoardConfig.hpp"
#include "data/ADCSample.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace
{
    constexpr const char *TAG = "CAN";
    constexpr uint16_t PRESSURE_CAN_ID = 0x500;
}

void CAN_task(void *parameter)
{
    const auto adc_queue = static_cast<QueueHandle_t>(parameter);
    ESP_ERROR_CHECK(MSM_CAN::init(BoardConfig::CAN::RX_GPIO,
                                 BoardConfig::CAN::TX_GPIO));
    while (true)
    {
        SensorData::ADCSample sample{};
        // Waiting for a sample blocks this task and lets other tasks run.
        if (xQueueReceive(adc_queue, &sample, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        // 4. CAN: create a frame, pack the count, then send it.
        MSM_CAN::TxFrame frame{};
        frame.id = PRESSURE_CAN_ID;
        MSM_CAN::pack_u16(frame.data, 0, sample.raw[0]);

        const esp_err_t result = MSM_CAN::send_msg(frame);

        if (result != ESP_OK)
        {
            ESP_LOGW(TAG, "CAN operation failed: %s", esp_err_to_name(result));
        }
    }
}
