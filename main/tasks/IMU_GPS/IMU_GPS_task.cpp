#include "IMU_GPS_task.hpp"

#include "BNO055.hpp"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "IMU_GPS_task";
static constexpr gpio_num_t I2C_SDA_GPIO = GPIO_NUM_15;
static constexpr gpio_num_t I2C_SCL_GPIO = GPIO_NUM_16;
static constexpr uint32_t I2C_SPEED_HZ = 100000;
static constexpr uint32_t SAMPLE_PERIOD_MS = 50;

void IMU_GPS_task(void *pv_parameters)
{
    (void)pv_parameters;

    ESP_LOGI(TAG, "Started");

    i2c_master_bus_handle_t bus = nullptr;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = true,
            .allow_pd = false,
        },
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &bus);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    BNO055::Config imu_config{};
    imu_config.bus = bus;
    imu_config.address = BNO055::DEFAULT_ADDRESS;
    imu_config.mode = BNO055::OperationMode::NDOF;
    imu_config.use_external_crystal = true;
    imu_config.i2c_speed_hz = I2C_SPEED_HZ;

    err = BNO055::init(imu_config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "BNO055 init failed: %s", esp_err_to_name(err));
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(TAG, "BNO055 initialised");

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t sample_period = pdMS_TO_TICKS(SAMPLE_PERIOD_MS);

    while (true)
    {
        BNO055::Data data{};
        err = BNO055::read_data(data);
        if (err == ESP_OK)
        {
            const float demo_value =
                data.euler.heading_deg + data.euler.roll_deg + data.euler.pitch_deg;

            ESP_LOGI(TAG,
                     "demo=%.2f heading=%.2f roll=%.2f pitch=%.2f cal=%u/%u/%u/%u",
                     demo_value,
                     data.euler.heading_deg,
                     data.euler.roll_deg,
                     data.euler.pitch_deg,
                     data.calibration.system,
                     data.calibration.gyro,
                     data.calibration.accel,
                     data.calibration.mag);
        }
        else
        {
            ESP_LOGW(TAG, "BNO055 read failed: %s", esp_err_to_name(err));
        }

        vTaskDelayUntil(&last_wake, sample_period);
    }
}
        
