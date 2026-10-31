#pragma once

#include <cstdint>
#include "driver/gpio.h"
#include "driver/spi_master.h"

namespace BoardConfig
{
    namespace ADC
    {
        constexpr spi_host_device_t SPI_HOST = SPI2_HOST;
        constexpr gpio_num_t MOSI_GPIO = GPIO_NUM_11;
        constexpr gpio_num_t SCLK_GPIO = GPIO_NUM_12;
        constexpr gpio_num_t MISO_GPIO = GPIO_NUM_13;
        constexpr gpio_num_t CS_A_GPIO = GPIO_NUM_10;
        constexpr uint32_t SPI_CLOCK_HZ = 4'000'000;
        constexpr float REFERENCE_VOLTAGE = 5.0f;
    }

    namespace CAN
    {
        constexpr gpio_num_t RX_GPIO = GPIO_NUM_2;
        constexpr gpio_num_t TX_GPIO = GPIO_NUM_1;
    }
}
