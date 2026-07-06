#include "GPS_task.hpp"

#include "GPS.hpp"
#include "config/BoardConfig.hpp"
#include "data/SensorQueues.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace
{
    const char *TAG = "GPS_task";
    constexpr int64_t STALE_DATA_US = 2'000'000;
    constexpr uint32_t LOG_PERIOD_MS = 1'000;

    GPS::Config make_gps_config()
    {
        GPS::Config config{};
        config.uart_port = BoardConfig::GPS::UART_PORT;
        config.rx_gpio = BoardConfig::GPS::RX_GPIO;
        config.tx_gpio = BoardConfig::GPS::TX_GPIO;
        config.pps_gpio = BoardConfig::GPS::PPS_GPIO;
        config.baud_rate = BoardConfig::GPS::BAUD_RATE;
        return config;
    }

    void log_sample(const GPS::Data& data)
    {
        ESP_LOGI(
            TAG,
            "fix=%u lat=%.7f lon=%.7f alt=%.1fm sats=%u "
            "speed=%.2fkn course=%.1f pps=%lu",
            data.has_fix ? 1U : 0U,
            data.latitude_deg,
            data.longitude_deg,
            data.altitude_m,
            data.satellites,
            data.speed_knots,
            data.course_deg,
            static_cast<unsigned long>(data.pps_count));
    }
}

void GPS_task(void *pv_parameters)
{
    (void)pv_parameters;

    GPS::Receiver gps;
    const esp_err_t init_result = gps.init(make_gps_config());
    if (init_result != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "GPS initialization failed: %s",
            esp_err_to_name(init_result));
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(
        TAG,
        "GPS initialized on UART%d RX=GPIO%d TX=GPIO%d PPS=GPIO%d",
        static_cast<int>(BoardConfig::GPS::UART_PORT),
        static_cast<int>(BoardConfig::GPS::RX_GPIO),
        static_cast<int>(BoardConfig::GPS::TX_GPIO),
        static_cast<int>(BoardConfig::GPS::PPS_GPIO));

    int64_t next_log_time_us = esp_timer_get_time();
    bool received_data = false;
    bool data_was_stale = false;

    while (true)
    {
        GPS::Data data{};
        const bool updated =
            gps.poll(data, BoardConfig::GPS::READ_TIMEOUT_MS);
        received_data = received_data || updated;
        if (!received_data)
        {
            continue;
        }

        const int64_t now_us = esp_timer_get_time();
        const bool data_is_stale =
            now_us - data.last_sentence_timestamp_us > STALE_DATA_US;
        if (data_is_stale)
        {
            data.has_fix = false;
        }

        if (!updated && data_is_stale == data_was_stale)
        {
            continue;
        }
        data_was_stale = data_is_stale;

        SensorQueues::publish(data);

        if (now_us >= next_log_time_us)
        {
            log_sample(data);
            next_log_time_us = now_us + (LOG_PERIOD_MS * 1'000);
        }
    }
}
