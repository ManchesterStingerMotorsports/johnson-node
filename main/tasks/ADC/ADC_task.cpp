#include "ADC_task.hpp"
#include "ADC124S051.hpp"
#include "config/BoardConfig.hpp"
#include "data/ADCSample.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace
{
    constexpr const char *TAG = "ADC";
    constexpr auto PRESSURE_CHANNEL = ADC124S051::Channel::IN1;
    constexpr uint32_t SAMPLE_PERIOD_MS = 100;

    esp_err_t initialise_spi()
    {
        spi_bus_config_t config{};
        config.mosi_io_num = BoardConfig::ADC::MOSI_GPIO;
        config.miso_io_num = BoardConfig::ADC::MISO_GPIO;
        config.sclk_io_num = BoardConfig::ADC::SCLK_GPIO;
        config.quadwp_io_num = GPIO_NUM_NC;
        config.quadhd_io_num = GPIO_NUM_NC;
        config.max_transfer_sz = sizeof(uint16_t);
        return spi_bus_initialize(BoardConfig::ADC::SPI_HOST, &config,
                                  SPI_DMA_DISABLED);
    }
}

void ADC_task(void *parameter)
{
    const auto adc_queue = static_cast<QueueHandle_t>(parameter);
    ESP_ERROR_CHECK(initialise_spi());
    ADC124S051 adc;
    ESP_ERROR_CHECK(adc.init(BoardConfig::ADC::SPI_HOST,
                             BoardConfig::ADC::CS_A_GPIO,
                             BoardConfig::ADC::SPI_CLOCK_HZ,
                             BoardConfig::ADC::REFERENCE_VOLTAGE));

    TickType_t last_wake = xTaskGetTickCount();
    while (true)
    {
        // read the pressure sensor on ADC A, channel IN1.
        uint16_t raw_count = 0;
        const esp_err_t result = adc.read(PRESSURE_CHANNEL, raw_count);
        if (result == ESP_OK)
        {
            // package the count in the existing ADCSample.
            // We only use raw[0]; the other seven entries stay at zero.
            SensorData::ADCSample sample{};
            sample.raw[0] = raw_count;

            // 3. QUEUE: FreeRTOS copies the entire struct, not a pointer.
            if (xQueueSend(adc_queue, &sample, 0) != pdTRUE)
            {
                ESP_LOGW(TAG, "ADC queue full; dropped this sample");
            }
        }
        else
        {
            // A failed acquisition must not become a zero pressure reading.
            ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(result));
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}
