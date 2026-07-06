#include "CAN_task.hpp"

#include "CAN_packets.hpp"
#include "MSM_CAN.hpp"
#include "config/BoardConfig.hpp"
#include "data/SensorQueues.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace
{
    const char *TAG = "CAN_task";

    template <size_t Capacity>
    void send_frames(const CANPackets::FrameBatch<Capacity>& batch)
    {
        for (size_t index = 0; index < Capacity; index++)
        {
            (void)MSM_CAN::send_msg(batch.frames[index]);
        }
    }
}

void CAN_task(void *pv_parameters)
{
    (void)pv_parameters;

    const esp_err_t init_result = MSM_CAN::init(
        BoardConfig::CAN::RX_GPIO,
        BoardConfig::CAN::TX_GPIO);
    if (init_result != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "CAN initialization failed: %s",
            esp_err_to_name(init_result));
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(
        TAG,
        "CAN initialized RX=GPIO%d TX=GPIO%d",
        static_cast<int>(BoardConfig::CAN::RX_GPIO),
        static_cast<int>(BoardConfig::CAN::TX_GPIO));

    while (true)
    {
        SensorData::ADCSample adc_sample{};
        if (SensorQueues::take(adc_sample))
        {
            send_frames(CANPackets::encode(adc_sample));
        }

        SensorData::IMUSample imu_sample{};
        if (SensorQueues::take(imu_sample))
        {
            send_frames(CANPackets::encode(imu_sample));
        }

        SensorData::GPSSample gps_sample{};
        if (SensorQueues::take(gps_sample))
        {
            send_frames(CANPackets::encode(gps_sample));
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
