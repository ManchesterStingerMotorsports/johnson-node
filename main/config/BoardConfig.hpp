

#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "driver/uart.h"

namespace BoardConfig
{
    namespace StatusLed
    {
        static constexpr gpio_num_t GPIO = GPIO_NUM_18;
        static constexpr uint32_t BLINK_PERIOD_MS = 500;
    }

    namespace ADC
    {
        static constexpr spi_host_device_t SPI_HOST = SPI2_HOST;
        static constexpr gpio_num_t MOSI_GPIO = GPIO_NUM_11;
        static constexpr gpio_num_t SCLK_GPIO = GPIO_NUM_12;
        static constexpr gpio_num_t MISO_GPIO = GPIO_NUM_13;
        static constexpr gpio_num_t CS_A_GPIO = GPIO_NUM_10;
        static constexpr gpio_num_t CS_B_GPIO = GPIO_NUM_9;

        // TI specifies ADC performance from 3.2 MHz to 8 MHz.
        static constexpr uint32_t SPI_CLOCK_HZ = 4'000'000;
        static constexpr uint32_t SAMPLE_RATE_HZ = 100;
        // The ADC full-scale reference is its 5 V VA supply.
        static constexpr float REFERENCE_VOLTAGE = 5.0f;
    }

    namespace IMU
    {
        static constexpr i2c_port_num_t I2C_PORT = I2C_NUM_0;
        static constexpr gpio_num_t SDA_GPIO = GPIO_NUM_16;
        static constexpr gpio_num_t SCL_GPIO = GPIO_NUM_15;

        static constexpr uint32_t I2C_CLOCK_HZ = 100'000;
        static constexpr uint32_t SAMPLE_RATE_HZ = 100;
        static constexpr uint32_t RETRY_PERIOD_MS = 2'000;
    }

    namespace GPS
    {
        static constexpr uart_port_t UART_PORT = UART_NUM_1;
        // ESP RX receives GPS TX; ESP TX drives GPS RX.
        static constexpr gpio_num_t RX_GPIO = GPIO_NUM_6;
        static constexpr gpio_num_t TX_GPIO = GPIO_NUM_5;
        static constexpr gpio_num_t PPS_GPIO = GPIO_NUM_17;

        static constexpr uint32_t BAUD_RATE = 9'600;
        static constexpr uint32_t READ_TIMEOUT_MS = 100;
    }

    namespace CAN
    {
        // ESP TX drives transceiver CAND; ESP RX receives transceiver CANR.
        static constexpr gpio_num_t RX_GPIO = GPIO_NUM_2;
        static constexpr gpio_num_t TX_GPIO = GPIO_NUM_1;
    }
}
