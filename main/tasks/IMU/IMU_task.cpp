#include "IMU_task.hpp"

#include "BNO055.hpp"
#include "config/BoardConfig.hpp"
#include "data/SensorQueues.hpp"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace
{
    const char *TAG = "IMU_task";
    constexpr uint32_t LOG_PERIOD_MS = 1'000;
    constexpr uint32_t READ_FAILURES_BEFORE_RESET = 10;

    esp_err_t initialise_sensor(i2c_master_bus_handle_t bus, uint8_t address)
    {
        BNO055::Config config{};
        config.bus = bus;
        config.address = address;
        config.mode = BNO055::OperationMode::NDOF;
        config.use_external_crystal = true;
        config.i2c_speed_hz = BoardConfig::IMU::I2C_CLOCK_HZ;
        return BNO055::init(config);
    }

    esp_err_t initialise_sensor_at_available_address(
        i2c_master_bus_handle_t bus,
        uint8_t& address)
    {
        address = BNO055::DEFAULT_ADDRESS;
        esp_err_t err = initialise_sensor(bus, address);
        if (err == ESP_OK)
        {
            return ESP_OK;
        }

        ESP_LOGW(
            TAG,
            "BNO055 not found at 0x%02X: %s",
            address,
            esp_err_to_name(err));

        address = BNO055::ALTERNATE_ADDRESS;
        return initialise_sensor(bus, address);
    }

    void log_sample(const BNO055::Data& data)
    {
        ESP_LOGI(
            TAG,
            "roll=%.2f pitch=%.2f yaw=%.2f "
            "linear=(%.2f,%.2f,%.2f)m/s2 cal=%u/%u/%u/%u",
            data.euler.roll_deg,
            data.euler.pitch_deg,
            data.euler.heading_deg,
            data.linear_accel_mps2.x,
            data.linear_accel_mps2.y,
            data.linear_accel_mps2.z,
            data.calibration.system,
            data.calibration.gyro,
            data.calibration.accel,
            data.calibration.mag);
    }
}

void IMU_task(void *pv_parameters)
{
    (void)pv_parameters;

    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = BoardConfig::IMU::I2C_PORT;
    bus_config.sda_io_num = BoardConfig::IMU::SDA_GPIO;
    bus_config.scl_io_num = BoardConfig::IMU::SCL_GPIO;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t bus = nullptr;
    esp_err_t err = i2c_new_master_bus(&bus_config, &bus);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C bus initialization failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    uint8_t sensor_address = 0;
    while ((err = initialise_sensor_at_available_address(bus, sensor_address)) !=
           ESP_OK)
    {
        ESP_LOGW(
            TAG,
            "BNO055 initialization failed: %s; retrying",
            esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(BoardConfig::IMU::RETRY_PERIOD_MS));
    }

    ESP_LOGI(
        TAG,
        "BNO055 initialized at 0x%02X; sampling at %lu Hz",
        sensor_address,
        static_cast<unsigned long>(BoardConfig::IMU::SAMPLE_RATE_HZ));

    const TickType_t sample_period =
        pdMS_TO_TICKS(1'000 / BoardConfig::IMU::SAMPLE_RATE_HZ);
    TickType_t last_wake = xTaskGetTickCount();
    int64_t next_log_time_us = esp_timer_get_time();
    int64_t next_error_log_time_us = 0;
    uint32_t consecutive_failures = 0;

    while (true)
    {
        SensorData::IMUSample sample{};
        err = BNO055::read_data(sample);

        if (err == ESP_OK)
        {
            consecutive_failures = 0;

            SensorQueues::publish(sample);

            const int64_t now_us = esp_timer_get_time();
            if (now_us >= next_log_time_us)
            {
                log_sample(sample);
                next_log_time_us =
                    now_us + (LOG_PERIOD_MS * 1'000);
            }
        }
        else
        {
            consecutive_failures++;
            const int64_t now_us = esp_timer_get_time();
            if (now_us >= next_error_log_time_us)
            {
                ESP_LOGW(TAG, "BNO055 read failed: %s", esp_err_to_name(err));
                next_error_log_time_us =
                    now_us + (LOG_PERIOD_MS * 1'000);
            }

            if (consecutive_failures >= READ_FAILURES_BEFORE_RESET)
            {
                ESP_LOGW(TAG, "Resetting BNO055 after repeated read failures");
                err = BNO055::reset();
                consecutive_failures = 0;

                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "BNO055 reset failed: %s", esp_err_to_name(err));
                    vTaskDelay(
                        pdMS_TO_TICKS(BoardConfig::IMU::RETRY_PERIOD_MS));
                    last_wake = xTaskGetTickCount();
                }
            }
        }

        vTaskDelayUntil(&last_wake, sample_period);
    }
}
