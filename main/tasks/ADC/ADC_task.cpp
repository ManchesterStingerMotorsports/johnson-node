#include "ADC_task.hpp"

#include <cstddef>
#include <cstdint>

#include "ADC124S051.hpp"
#include "config/BoardConfig.hpp"
#include "data/SensorQueues.hpp"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace
{
    const char *TAG = "ADC_task";
    constexpr uint32_t SERIAL_LOG_DIVIDER =
        BoardConfig::ADC::SAMPLE_RATE_HZ;

    esp_err_t initialise_spi_bus() //spi_busman, spi_busman does whatever a spi_bus can
    {
        spi_bus_config_t config = {};
        config.mosi_io_num = BoardConfig::ADC::MOSI_GPIO;
        config.miso_io_num = BoardConfig::ADC::MISO_GPIO;
        config.sclk_io_num = BoardConfig::ADC::SCLK_GPIO;
        config.quadwp_io_num = GPIO_NUM_NC;
        config.quadhd_io_num = GPIO_NUM_NC;
        config.max_transfer_sz = sizeof(uint16_t);

        return spi_bus_initialize(
            BoardConfig::ADC::SPI_HOST,
            &config,
            SPI_DMA_DISABLED);
    }

    bool read_channels(ADC124S051& adc,
                       size_t first_sample_index,
                       SensorData::ADCSample& sample)
    {
        bool all_reads_ok = true;

        for (uint8_t channel_index = 0;
             channel_index < ADC124S051::CHANNEL_COUNT;
             channel_index++)
        {
            const auto channel =
                static_cast<ADC124S051::Channel>(channel_index);
            const size_t sample_index = first_sample_index + channel_index;

            uint16_t raw_count = 0;
            const esp_err_t err = adc.read(channel, raw_count);
            if (err != ESP_OK)
            {
                all_reads_ok = false;
                continue;
            }

            sample.raw[sample_index] = raw_count;
        }

        return all_reads_ok;
    }

    void log_sample(const SensorData::ADCSample& sample,
                    const ADC124S051& adc_a,
                    const ADC124S051& adc_b)
    {
        ESP_LOGI(
            TAG,
            "A=%.3fV B=%.3fV C=%.3fV D=%.3fV "
            "E=%.3fV F=%.3fV G=%.3fV H=%.3fV",
            adc_a.counts_to_voltage(sample.raw[0]),
            adc_a.counts_to_voltage(sample.raw[1]),
            adc_a.counts_to_voltage(sample.raw[2]),
            adc_a.counts_to_voltage(sample.raw[3]),
            adc_b.counts_to_voltage(sample.raw[4]),
            adc_b.counts_to_voltage(sample.raw[5]),
            adc_b.counts_to_voltage(sample.raw[6]),
            adc_b.counts_to_voltage(sample.raw[7]));
    }
}

void ADC_task(void *pv_parameters)
{
    (void)pv_parameters;

    esp_err_t err = initialise_spi_bus();
    if (err != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "SPI bus initialization failed: %s",
            esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    ADC124S051 adc_a; //look at me using fancy OOP, really warrants using C++ instead of C eh?
    ADC124S051 adc_b;

    err = adc_a.init(
        BoardConfig::ADC::SPI_HOST,
        BoardConfig::ADC::CS_A_GPIO,
        BoardConfig::ADC::SPI_CLOCK_HZ,
        BoardConfig::ADC::REFERENCE_VOLTAGE);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ADC A initialization failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    err = adc_b.init(
        BoardConfig::ADC::SPI_HOST,
        BoardConfig::ADC::CS_B_GPIO,
        BoardConfig::ADC::SPI_CLOCK_HZ,
        BoardConfig::ADC::REFERENCE_VOLTAGE);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "ADC B initialization failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(
        TAG,
        "Sampling eight ADC channels at %lu Hz",
        static_cast<unsigned long>(BoardConfig::ADC::SAMPLE_RATE_HZ));

    const TickType_t sample_period =
        pdMS_TO_TICKS(1'000 / BoardConfig::ADC::SAMPLE_RATE_HZ);
    TickType_t last_wake = xTaskGetTickCount();
    uint32_t samples_since_log = 0;
    bool read_failed_since_log = false;

    while (true)
    {
        SensorData::ADCSample sample{};
        const bool adc_a_ok = read_channels(adc_a, 0, sample);
        const bool adc_b_ok =
            read_channels(adc_b, ADC124S051::CHANNEL_COUNT, sample);

        SensorQueues::publish(sample);

        read_failed_since_log |= !adc_a_ok || !adc_b_ok;
        samples_since_log++;
        if (samples_since_log >= SERIAL_LOG_DIVIDER)
        {
            if (read_failed_since_log)
            {
                ESP_LOGW(TAG, "One or more ADC reads failed");
            }

            log_sample(sample, adc_a, adc_b);
            samples_since_log = 0;
            read_failed_since_log = false;
        }

        vTaskDelayUntil(&last_wake, sample_period);
    }
}
