#include "SensorQueues.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace
{
    constexpr UBaseType_t ADC_QUEUE_LENGTH = 16;
    constexpr UBaseType_t IMU_QUEUE_LENGTH = 16;
    constexpr UBaseType_t GPS_QUEUE_LENGTH = 4;

    QueueHandle_t adc_queue = nullptr;
    QueueHandle_t imu_queue = nullptr;
    QueueHandle_t gps_queue = nullptr;

    template <typename Sample>
    void publish_to(QueueHandle_t queue, const Sample& sample)
    {
        if (queue != nullptr)
        {
            (void)xQueueSend(queue, &sample, 0);
        }
    }

    template <typename Sample>
    bool take_from(QueueHandle_t queue, Sample& sample)
    {
        return queue != nullptr && xQueueReceive(queue, &sample, 0) == pdTRUE;
    }
}

namespace SensorQueues
{
    esp_err_t init()
    {
        if (adc_queue != nullptr || imu_queue != nullptr || gps_queue != nullptr)
        {
            return ESP_ERR_INVALID_STATE;
        }

        adc_queue = xQueueCreate(ADC_QUEUE_LENGTH, sizeof(SensorData::ADCSample));
        imu_queue = xQueueCreate(IMU_QUEUE_LENGTH, sizeof(SensorData::IMUSample));
        gps_queue = xQueueCreate(GPS_QUEUE_LENGTH, sizeof(SensorData::GPSSample));

        if (adc_queue == nullptr || imu_queue == nullptr || gps_queue == nullptr)
        {
            if (adc_queue != nullptr)
            {
                vQueueDelete(adc_queue);
                adc_queue = nullptr;
            }
            if (imu_queue != nullptr)
            {
                vQueueDelete(imu_queue);
                imu_queue = nullptr;
            }
            if (gps_queue != nullptr)
            {
                vQueueDelete(gps_queue);
                gps_queue = nullptr;
            }
            return ESP_ERR_NO_MEM;
        }

        return ESP_OK;
    }

    void publish(const SensorData::ADCSample& sample)
    {
        publish_to(adc_queue, sample);
    }

    void publish(const SensorData::IMUSample& sample)
    {
        publish_to(imu_queue, sample);
    }

    void publish(const SensorData::GPSSample& sample)
    {
        publish_to(gps_queue, sample);
    }

    bool take(SensorData::ADCSample& sample)
    {
        return take_from(adc_queue, sample);
    }

    bool take(SensorData::IMUSample& sample)
    {
        return take_from(imu_queue, sample);
    }

    bool take(SensorData::GPSSample& sample)
    {
        return take_from(gps_queue, sample);
    }
}
