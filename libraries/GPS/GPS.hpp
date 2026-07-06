#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

namespace GPS
{
    struct Config
    {
        uart_port_t uart_port = UART_NUM_1;
        gpio_num_t rx_gpio = GPIO_NUM_NC;
        gpio_num_t tx_gpio = GPIO_NUM_NC;
        gpio_num_t pps_gpio = GPIO_NUM_NC;
        uint32_t baud_rate = 9'600;
        size_t uart_buffer_size = 2'048;
    };

    struct UtcTime
    {
        uint16_t year = 0;
        uint8_t month = 0;
        uint8_t day = 0;
        uint8_t hour = 0;
        uint8_t minute = 0;
        uint8_t second = 0;
        uint8_t centisecond = 0;
    };

    struct Data
    {
        bool has_fix = false;
        bool has_pps = false;

        double latitude_deg = 0.0;
        double longitude_deg = 0.0;
        float altitude_m = 0.0f;
        float speed_knots = 0.0f;
        float course_deg = 0.0f;
        float hdop = 0.0f;

        uint8_t fix_quality = 0;
        uint8_t satellites = 0;
        UtcTime utc{};

        uint32_t pps_count = 0;
        int64_t pps_timestamp_us = 0;
        int64_t last_sentence_timestamp_us = 0;
        uint32_t sentences_received = 0;
        uint32_t checksum_errors = 0;
    };

    class Receiver
    {
    public:
        esp_err_t init(const Config& config);

        // Returns true when at least one supported, valid sentence was parsed.
        bool poll(Data& data, uint32_t wait_ms);

    private:
        static void IRAM_ATTR pps_isr(void *context);

        bool consume(char character);
        bool parse_sentence();
        void copy_pps_state();

        Config config_{};
        Data data_{};
        char line_[128] = {};
        size_t line_length_ = 0;
        bool initialised_ = false;

        portMUX_TYPE pps_lock_ = portMUX_INITIALIZER_UNLOCKED;
        volatile uint32_t pps_count_ = 0;
        volatile int64_t pps_timestamp_us_ = 0;
    };
}
