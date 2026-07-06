#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"

class ADC124S051
{
public:
    static constexpr uint8_t CHANNEL_COUNT = 4;

    enum class Channel : uint8_t //0 indexing mogs
    {
        IN1 = 0,
        IN2 = 1,
        IN3 = 2,
        IN4 = 3,
    };

    esp_err_t init(spi_host_device_t spi_host,
                   gpio_num_t chip_select_gpio,
                   uint32_t clock_hz,
                   float reference_voltage);        //5V on the johnson node but made the lib generic for any ADC124S051 implementation

    esp_err_t read(Channel channel, uint16_t& raw_count);
    float counts_to_voltage(uint16_t raw_count) const;

private:
    esp_err_t transfer(Channel channel, uint16_t& raw_count);

    spi_device_handle_t device_ = nullptr;
    float reference_voltage_ = 0.0f;
};
