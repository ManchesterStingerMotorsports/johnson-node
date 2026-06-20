#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"

namespace BNO055
{
    static constexpr uint8_t DEFAULT_ADDRESS = 0x28;
    static constexpr uint8_t ALTERNATE_ADDRESS = 0x29;

    enum class OperationMode : uint8_t
    {
        Config = 0x00,
        AccelOnly = 0x01,
        MagOnly = 0x02,
        GyroOnly = 0x03,
        AccelMag = 0x04,
        AccelGyro = 0x05,
        MagGyro = 0x06,
        AMG = 0x07,
        IMUPlus = 0x08,
        Compass = 0x09,
        M4G = 0x0A,
        NDOF_FMC_Off = 0x0B,
        NDOF = 0x0C,
    };

    struct Config
    {
        i2c_master_bus_handle_t bus = nullptr;
        uint8_t address = DEFAULT_ADDRESS;
        gpio_num_t reset_gpio = GPIO_NUM_NC;
        bool use_external_crystal = true;
        OperationMode mode = OperationMode::NDOF;
        uint32_t i2c_speed_hz = 100000;
        uint32_t i2c_timeout_ms = 50;
        uint32_t scl_wait_us = 2000;
    };

    struct Vector3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct Quaternion
    {
        float w = 1.0f;
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct Euler
    {
        float heading_deg = 0.0f;
        float roll_deg = 0.0f;
        float pitch_deg = 0.0f;
    };

    struct Calibration
    {
        uint8_t system = 0;
        uint8_t gyro = 0;
        uint8_t accel = 0;
        uint8_t mag = 0;
    };

    struct Status
    {
        uint8_t system_status = 0;
        uint8_t self_test_result = 0;
        uint8_t system_error = 0;
    };

    struct Offsets
    {
        int16_t accel_offset_x = 0;
        int16_t accel_offset_y = 0;
        int16_t accel_offset_z = 0;
        int16_t mag_offset_x = 0;
        int16_t mag_offset_y = 0;
        int16_t mag_offset_z = 0;
        int16_t gyro_offset_x = 0;
        int16_t gyro_offset_y = 0;
        int16_t gyro_offset_z = 0;
        int16_t accel_radius = 0;
        int16_t mag_radius = 0;
    };

    struct Data
    {
        Euler euler;
        Quaternion quaternion;
        Vector3 accel_mps2;
        Vector3 linear_accel_mps2;
        Vector3 gravity_mps2;
        Vector3 gyro_rad_s;
        Vector3 mag_uT;
        int8_t temperature_c = 0;
        Calibration calibration;
        uint32_t timestamp_ms = 0;
    };

    esp_err_t init(const Config& config);
    esp_err_t reset();
    esp_err_t read_data(Data& data);

    esp_err_t get_calibration(Calibration& calibration);
    bool is_fully_calibrated(const Calibration& calibration);

    esp_err_t get_offsets(Offsets& offsets);
    esp_err_t set_offsets(const Offsets& offsets);

    esp_err_t get_status(Status& status);
}
