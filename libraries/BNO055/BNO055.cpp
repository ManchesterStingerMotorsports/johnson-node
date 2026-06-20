#include "BNO055.hpp"

#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace BNO055
{
    enum Register : uint8_t
    {
        CHIP_ID = 0x00,
        ACCEL_REV_ID = 0x01,
        MAG_REV_ID = 0x02,
        GYRO_REV_ID = 0x03,
        SW_REV_ID_LSB = 0x04,
        SW_REV_ID_MSB = 0x05,
        BL_REV_ID = 0x06,
        PAGE_ID = 0x07,

        ACCEL_DATA_X_LSB = 0x08,
        MAG_DATA_X_LSB = 0x0E,
        GYRO_DATA_X_LSB = 0x14,
        EULER_H_LSB = 0x1A,
        QUATERNION_DATA_W_LSB = 0x20,
        LINEAR_ACCEL_DATA_X_LSB = 0x28,
        GRAVITY_DATA_X_LSB = 0x2E,
        TEMP = 0x34,

        CALIB_STAT = 0x35,
        SELFTEST_RESULT = 0x36,
        SYS_STAT = 0x39,
        SYS_ERR = 0x3A,
        UNIT_SEL = 0x3B,
        OPR_MODE = 0x3D,
        PWR_MODE = 0x3E,
        SYS_TRIGGER = 0x3F,

        ACCEL_OFFSET_X_LSB = 0x55,
    };

    static constexpr uint8_t BNO055_ID = 0xA0;
    static constexpr uint8_t POWER_MODE_NORMAL = 0x00;
    static constexpr uint8_t SYS_TRIGGER_RESET = 0x20;
    static constexpr uint8_t SYS_TRIGGER_EXT_CRYSTAL = 0x80;
    static constexpr uint8_t UNIT_SEL_DEFAULT = 0x02; // Celsius, degrees, rad/s, m/s^2.
    static constexpr size_t SENSOR_DATA_LEN = 45;
    static constexpr size_t OFFSET_DATA_LEN = 22;

    static Config g_config{};
    static i2c_master_dev_handle_t g_dev = nullptr;
    static bool g_initialised = false;
    static OperationMode g_mode = OperationMode::Config;

    static void delay_ms(uint32_t ms)
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    static int16_t read_i16_le(const uint8_t *data)
    {
        return static_cast<int16_t>(
            static_cast<uint16_t>(data[0]) |
            (static_cast<uint16_t>(data[1]) << 8));
    }

    static void write_i16_le(uint8_t *data, int16_t value)
    {
        const uint16_t raw = static_cast<uint16_t>(value);
        data[0] = static_cast<uint8_t>(raw & 0xFF);
        data[1] = static_cast<uint8_t>((raw >> 8) & 0xFF);
    }

    static uint32_t timeout_ms()
    {
        return g_config.i2c_timeout_ms == 0 ? 50 : g_config.i2c_timeout_ms;
    }

    static esp_err_t read_len(uint8_t reg, uint8_t *data, size_t len)
    {
        if (g_dev == nullptr || data == nullptr || len == 0)
        {
            return ESP_ERR_INVALID_STATE;
        }

        return i2c_master_transmit_receive(
            g_dev,
            &reg,
            1,
            data,
            len,
            static_cast<int>(timeout_ms()));
    }

    static esp_err_t write_len(uint8_t reg, const uint8_t *data, size_t len)
    {
        if (g_dev == nullptr || data == nullptr || len == 0)
        {
            return ESP_ERR_INVALID_STATE;
        }

        uint8_t buffer[1 + OFFSET_DATA_LEN] = {};
        if (len > OFFSET_DATA_LEN)
        {
            return ESP_ERR_INVALID_SIZE;
        }

        buffer[0] = reg;
        memcpy(&buffer[1], data, len);

        return i2c_master_transmit(
            g_dev,
            buffer,
            len + 1,
            static_cast<int>(timeout_ms()));
    }

    static esp_err_t read_u8(uint8_t reg, uint8_t& value)
    {
        return read_len(reg, &value, 1);
    }

    static esp_err_t write_u8(uint8_t reg, uint8_t value)
    {
        return write_len(reg, &value, 1);
    }

    static esp_err_t set_page(uint8_t page)
    {
        return write_u8(PAGE_ID, page);
    }

    static esp_err_t wait_for_chip_id(uint32_t timeout_ms)
    {
        const int64_t deadline = esp_timer_get_time() + (static_cast<int64_t>(timeout_ms) * 1000);

        while (esp_timer_get_time() < deadline)
        {
            uint8_t id = 0;
            if (read_u8(CHIP_ID, id) == ESP_OK && id == BNO055_ID)
            {
                return ESP_OK;
            }

            delay_ms(10);
        }

        return ESP_ERR_TIMEOUT;
    }

    static esp_err_t set_mode_internal(OperationMode mode)
    {
        const esp_err_t err = write_u8(OPR_MODE, static_cast<uint8_t>(mode));
        if (err != ESP_OK)
        {
            return err;
        }

        g_mode = mode;
        delay_ms(mode == OperationMode::Config ? 25 : 30);
        return ESP_OK;
    }

    static esp_err_t configure_output_units()
    {
        return write_u8(UNIT_SEL, UNIT_SEL_DEFAULT);
    }

    static esp_err_t configure_external_crystal(bool enable)
    {
        const OperationMode previous_mode = g_mode;
        esp_err_t err = set_mode_internal(OperationMode::Config);
        if (err != ESP_OK)
        {
            return err;
        }

        err = set_page(0);
        if (err == ESP_OK)
        {
            err = write_u8(SYS_TRIGGER, enable ? SYS_TRIGGER_EXT_CRYSTAL : 0x00);
        }

        delay_ms(10);

        if (err == ESP_OK)
        {
            err = set_mode_internal(previous_mode);
        }

        return err;
    }

    static esp_err_t configure_after_reset()
    {
        esp_err_t err = wait_for_chip_id(1000);
        if (err != ESP_OK)
        {
            return err;
        }

        err = set_mode_internal(OperationMode::Config);
        if (err != ESP_OK)
        {
            return err;
        }

        err = write_u8(PWR_MODE, POWER_MODE_NORMAL);
        if (err != ESP_OK)
        {
            return err;
        }
        delay_ms(10);

        err = set_page(0);
        if (err != ESP_OK)
        {
            return err;
        }

        err = configure_output_units();
        if (err != ESP_OK)
        {
            return err;
        }

        err = write_u8(SYS_TRIGGER, 0x00);
        if (err != ESP_OK)
        {
            return err;
        }
        delay_ms(10);

        err = set_mode_internal(g_config.mode);
        if (err != ESP_OK)
        {
            return err;
        }

        if (g_config.use_external_crystal)
        {
            err = configure_external_crystal(true);
        }

        return err;
    }

    static esp_err_t hardware_reset()
    {
        if (g_config.reset_gpio == GPIO_NUM_NC)
        {
            return ESP_ERR_NOT_SUPPORTED;
        }

        esp_err_t err = gpio_set_direction(g_config.reset_gpio, GPIO_MODE_OUTPUT);
        if (err != ESP_OK)
        {
            return err;
        }

        err = gpio_set_level(g_config.reset_gpio, 0);
        if (err != ESP_OK)
        {
            return err;
        }

        delay_ms(10);

        err = gpio_set_level(g_config.reset_gpio, 1);
        if (err != ESP_OK)
        {
            return err;
        }

        delay_ms(650);
        return ESP_OK;
    }

    static void unpack_offsets(const uint8_t raw[OFFSET_DATA_LEN], Offsets& offsets)
    {
        offsets.accel_offset_x = read_i16_le(&raw[0]);
        offsets.accel_offset_y = read_i16_le(&raw[2]);
        offsets.accel_offset_z = read_i16_le(&raw[4]);
        offsets.mag_offset_x = read_i16_le(&raw[6]);
        offsets.mag_offset_y = read_i16_le(&raw[8]);
        offsets.mag_offset_z = read_i16_le(&raw[10]);
        offsets.gyro_offset_x = read_i16_le(&raw[12]);
        offsets.gyro_offset_y = read_i16_le(&raw[14]);
        offsets.gyro_offset_z = read_i16_le(&raw[16]);
        offsets.accel_radius = read_i16_le(&raw[18]);
        offsets.mag_radius = read_i16_le(&raw[20]);
    }

    static void pack_offsets(const Offsets& offsets, uint8_t raw[OFFSET_DATA_LEN])
    {
        write_i16_le(&raw[0], offsets.accel_offset_x);
        write_i16_le(&raw[2], offsets.accel_offset_y);
        write_i16_le(&raw[4], offsets.accel_offset_z);
        write_i16_le(&raw[6], offsets.mag_offset_x);
        write_i16_le(&raw[8], offsets.mag_offset_y);
        write_i16_le(&raw[10], offsets.mag_offset_z);
        write_i16_le(&raw[12], offsets.gyro_offset_x);
        write_i16_le(&raw[14], offsets.gyro_offset_y);
        write_i16_le(&raw[16], offsets.gyro_offset_z);
        write_i16_le(&raw[18], offsets.accel_radius);
        write_i16_le(&raw[20], offsets.mag_radius);
    }

    esp_err_t init(const Config& config)
    {
        if (config.bus == nullptr)
        {
            return ESP_ERR_INVALID_ARG;
        }

        if (g_initialised)
        {
            return ESP_ERR_INVALID_STATE;
        }

        g_config = config;

        i2c_device_config_t dev_config = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = g_config.address,
            .scl_speed_hz = g_config.i2c_speed_hz,
            .scl_wait_us = g_config.scl_wait_us,
            .flags = {
                .disable_ack_check = false,
            },
        };

        esp_err_t err = i2c_master_bus_add_device(g_config.bus, &dev_config, &g_dev);
        if (err != ESP_OK)
        {
            g_dev = nullptr;
            return err;
        }

        err = wait_for_chip_id(1000);
        if (err != ESP_OK)
        {
            (void)i2c_master_bus_rm_device(g_dev);
            g_dev = nullptr;
            return err;
        }

        err = reset();
        if (err != ESP_OK)
        {
            (void)i2c_master_bus_rm_device(g_dev);
            g_dev = nullptr;
            return err;
        }

        g_initialised = true;
        return ESP_OK;
    }

    esp_err_t reset()
    {
        if (g_dev == nullptr)
        {
            return ESP_ERR_INVALID_STATE;
        }

        esp_err_t err = hardware_reset();
        if (err == ESP_ERR_NOT_SUPPORTED)
        {
            err = set_mode_internal(OperationMode::Config);
            if (err != ESP_OK)
            {
                return err;
            }

            err = write_u8(SYS_TRIGGER, SYS_TRIGGER_RESET);
            if (err != ESP_OK)
            {
                return err;
            }

            delay_ms(650);
        }
        else if (err != ESP_OK)
        {
            return err;
        }

        g_mode = OperationMode::Config;
        return configure_after_reset();
    }

    esp_err_t read_data(Data& data)
    {
        if (!g_initialised)
        {
            return ESP_ERR_INVALID_STATE;
        }

        uint8_t raw[SENSOR_DATA_LEN] = {};
        esp_err_t err = read_len(ACCEL_DATA_X_LSB, raw, sizeof(raw));
        if (err != ESP_OK)
        {
            return err;
        }

        data.accel_mps2.x = static_cast<float>(read_i16_le(&raw[0])) / 100.0f;
        data.accel_mps2.y = static_cast<float>(read_i16_le(&raw[2])) / 100.0f;
        data.accel_mps2.z = static_cast<float>(read_i16_le(&raw[4])) / 100.0f;

        data.mag_uT.x = static_cast<float>(read_i16_le(&raw[6])) / 16.0f;
        data.mag_uT.y = static_cast<float>(read_i16_le(&raw[8])) / 16.0f;
        data.mag_uT.z = static_cast<float>(read_i16_le(&raw[10])) / 16.0f;

        data.gyro_rad_s.x = static_cast<float>(read_i16_le(&raw[12])) / 900.0f;
        data.gyro_rad_s.y = static_cast<float>(read_i16_le(&raw[14])) / 900.0f;
        data.gyro_rad_s.z = static_cast<float>(read_i16_le(&raw[16])) / 900.0f;

        data.euler.heading_deg = static_cast<float>(read_i16_le(&raw[18])) / 16.0f;
        data.euler.roll_deg = static_cast<float>(read_i16_le(&raw[20])) / 16.0f;
        data.euler.pitch_deg = static_cast<float>(read_i16_le(&raw[22])) / 16.0f;

        data.quaternion.w = static_cast<float>(read_i16_le(&raw[24])) / 16384.0f;
        data.quaternion.x = static_cast<float>(read_i16_le(&raw[26])) / 16384.0f;
        data.quaternion.y = static_cast<float>(read_i16_le(&raw[28])) / 16384.0f;
        data.quaternion.z = static_cast<float>(read_i16_le(&raw[30])) / 16384.0f;

        data.linear_accel_mps2.x = static_cast<float>(read_i16_le(&raw[32])) / 100.0f;
        data.linear_accel_mps2.y = static_cast<float>(read_i16_le(&raw[34])) / 100.0f;
        data.linear_accel_mps2.z = static_cast<float>(read_i16_le(&raw[36])) / 100.0f;

        data.gravity_mps2.x = static_cast<float>(read_i16_le(&raw[38])) / 100.0f;
        data.gravity_mps2.y = static_cast<float>(read_i16_le(&raw[40])) / 100.0f;
        data.gravity_mps2.z = static_cast<float>(read_i16_le(&raw[42])) / 100.0f;

        data.temperature_c = static_cast<int8_t>(raw[44]);

        err = get_calibration(data.calibration);
        if (err != ESP_OK)
        {
            return err;
        }

        data.timestamp_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
        return ESP_OK;
    }

    esp_err_t get_calibration(Calibration& calibration)
    {
        if (g_dev == nullptr)
        {
            return ESP_ERR_INVALID_STATE;
        }

        uint8_t raw = 0;
        const esp_err_t err = read_u8(CALIB_STAT, raw);
        if (err != ESP_OK)
        {
            return err;
        }

        calibration.system = (raw >> 6) & 0x03;
        calibration.gyro = (raw >> 4) & 0x03;
        calibration.accel = (raw >> 2) & 0x03;
        calibration.mag = raw & 0x03;
        return ESP_OK;
    }

    bool is_fully_calibrated(const Calibration& calibration)
    {
        return calibration.system == 3 &&
               calibration.gyro == 3 &&
               calibration.accel == 3 &&
               calibration.mag == 3;
    }

    esp_err_t get_offsets(Offsets& offsets)
    {
        if (!g_initialised)
        {
            return ESP_ERR_INVALID_STATE;
        }

        const OperationMode previous_mode = g_mode;
        esp_err_t err = set_mode_internal(OperationMode::Config);
        if (err != ESP_OK)
        {
            return err;
        }

        uint8_t raw[OFFSET_DATA_LEN] = {};
        err = read_len(ACCEL_OFFSET_X_LSB, raw, sizeof(raw));
        if (err == ESP_OK)
        {
            unpack_offsets(raw, offsets);
        }

        const esp_err_t restore_err = set_mode_internal(previous_mode);
        return err == ESP_OK ? restore_err : err;
    }

    esp_err_t set_offsets(const Offsets& offsets)
    {
        if (!g_initialised)
        {
            return ESP_ERR_INVALID_STATE;
        }

        const OperationMode previous_mode = g_mode;
        esp_err_t err = set_mode_internal(OperationMode::Config);
        if (err != ESP_OK)
        {
            return err;
        }

        uint8_t raw[OFFSET_DATA_LEN] = {};
        pack_offsets(offsets, raw);
        err = write_len(ACCEL_OFFSET_X_LSB, raw, sizeof(raw));

        const esp_err_t restore_err = set_mode_internal(previous_mode);
        return err == ESP_OK ? restore_err : err;
    }

    esp_err_t get_status(Status& status)
    {
        if (g_dev == nullptr)
        {
            return ESP_ERR_INVALID_STATE;
        }

        esp_err_t err = read_u8(SYS_STAT, status.system_status);
        if (err != ESP_OK)
        {
            return err;
        }

        err = read_u8(SELFTEST_RESULT, status.self_test_result);
        if (err != ESP_OK)
        {
            return err;
        }

        return read_u8(SYS_ERR, status.system_error);
    }
}
