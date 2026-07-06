#include "GPS.hpp"

#include <cstdlib>
#include <cstring>

#include "esp_timer.h"

namespace
{
    constexpr size_t MAX_FIELDS = 20;

    int hex_value(char character)
    {
        if (character >= '0' && character <= '9')
        {
            return character - '0';
        }
        if (character >= 'A' && character <= 'F')
        {
            return character - 'A' + 10;
        }
        if (character >= 'a' && character <= 'f')
        {
            return character - 'a' + 10;
        }
        return -1;
    }

    bool checksum_is_valid(char *sentence)
    {
        if (sentence == nullptr || sentence[0] != '$')
        {
            return false;
        }

        char *asterisk = std::strchr(sentence, '*');
        if (asterisk == nullptr || asterisk[1] == '\0' || asterisk[2] == '\0')
        {
            return false;
        }

        uint8_t calculated = 0;
        for (char *cursor = sentence + 1; cursor < asterisk; cursor++)
        {
            calculated ^= static_cast<uint8_t>(*cursor);
        }

        const int high = hex_value(asterisk[1]);
        const int low = hex_value(asterisk[2]);
        if (high < 0 || low < 0)
        {
            return false;
        }

        *asterisk = '\0';
        const uint8_t received = static_cast<uint8_t>((high << 4) | low);
        return calculated == received;
    }

    size_t split_fields(char *sentence, char *fields[MAX_FIELDS])
    {
        size_t count = 0;
        char *field_start = sentence;

        while (count < MAX_FIELDS)
        {
            fields[count++] = field_start;
            char *comma = std::strchr(field_start, ',');
            if (comma == nullptr)
            {
                break;
            }

            *comma = '\0';
            field_start = comma + 1;
        }

        return count;
    }

    bool sentence_type_is(const char *sentence_id, const char *type)
    {
        if (sentence_id == nullptr || type == nullptr)
        {
            return false;
        }

        const size_t id_length = std::strlen(sentence_id);
        const size_t type_length = std::strlen(type);
        return id_length >= type_length &&
               std::strcmp(sentence_id + id_length - type_length, type) == 0;
    }

    bool field_has_value(const char *field)
    {
        return field != nullptr && field[0] != '\0';
    }

    double parse_coordinate(const char *value, const char *hemisphere)
    {
        if (!field_has_value(value) || !field_has_value(hemisphere))
        {
            return 0.0;
        }

        const double raw = std::strtod(value, nullptr);
        const int degrees = static_cast<int>(raw / 100.0);
        const double minutes = raw - static_cast<double>(degrees * 100);
        double coordinate = static_cast<double>(degrees) + (minutes / 60.0);

        if (hemisphere[0] == 'S' || hemisphere[0] == 'W')
        {
            coordinate = -coordinate;
        }

        return coordinate;
    }

    void parse_time(const char *field, GPS::UtcTime& utc)
    {
        if (!field_has_value(field) || std::strlen(field) < 6)
        {
            return;
        }

        utc.hour = static_cast<uint8_t>((field[0] - '0') * 10 + field[1] - '0');
        utc.minute = static_cast<uint8_t>((field[2] - '0') * 10 + field[3] - '0');
        utc.second = static_cast<uint8_t>((field[4] - '0') * 10 + field[5] - '0');

        const char *decimal = std::strchr(field, '.');
        if (decimal != nullptr && decimal[1] != '\0')
        {
            utc.centisecond = static_cast<uint8_t>((decimal[1] - '0') * 10);
            if (decimal[2] != '\0')
            {
                utc.centisecond += static_cast<uint8_t>(decimal[2] - '0');
            }
        }
    }

    void parse_date(const char *field, GPS::UtcTime& utc)
    {
        if (!field_has_value(field) || std::strlen(field) < 6)
        {
            return;
        }

        utc.day = static_cast<uint8_t>((field[0] - '0') * 10 + field[1] - '0');
        utc.month = static_cast<uint8_t>((field[2] - '0') * 10 + field[3] - '0');
        const uint8_t short_year =
            static_cast<uint8_t>((field[4] - '0') * 10 + field[5] - '0');
        utc.year = static_cast<uint16_t>(2000 + short_year);
    }

    bool parse_gga(char *fields[MAX_FIELDS], size_t count, GPS::Data& data)
    {
        if (count < 10)
        {
            return false;
        }

        parse_time(fields[1], data.utc);
        data.fix_quality =
            field_has_value(fields[6])
                ? static_cast<uint8_t>(std::strtoul(fields[6], nullptr, 10))
                : 0;
        data.has_fix = data.fix_quality > 0;
        data.satellites =
            field_has_value(fields[7])
                ? static_cast<uint8_t>(std::strtoul(fields[7], nullptr, 10))
                : 0;
        data.hdop =
            field_has_value(fields[8]) ? std::strtof(fields[8], nullptr) : 0.0f;
        data.altitude_m =
            field_has_value(fields[9]) ? std::strtof(fields[9], nullptr) : 0.0f;

        if (data.has_fix)
        {
            data.latitude_deg = parse_coordinate(fields[2], fields[3]);
            data.longitude_deg = parse_coordinate(fields[4], fields[5]);
        }

        return true;
    }

    bool parse_rmc(char *fields[MAX_FIELDS], size_t count, GPS::Data& data)
    {
        if (count < 10)
        {
            return false;
        }

        parse_time(fields[1], data.utc);
        parse_date(fields[9], data.utc);
        data.has_fix = field_has_value(fields[2]) && fields[2][0] == 'A';
        data.speed_knots =
            field_has_value(fields[7]) ? std::strtof(fields[7], nullptr) : 0.0f;
        data.course_deg =
            field_has_value(fields[8]) ? std::strtof(fields[8], nullptr) : 0.0f;

        if (data.has_fix)
        {
            data.latitude_deg = parse_coordinate(fields[3], fields[4]);
            data.longitude_deg = parse_coordinate(fields[5], fields[6]);
        }

        return true;
    }
}

namespace GPS
{
    esp_err_t Receiver::init(const Config& config)
    {
        if (initialised_)
        {
            return ESP_ERR_INVALID_STATE;
        }

        if (config.rx_gpio == GPIO_NUM_NC ||
            config.tx_gpio == GPIO_NUM_NC ||
            config.pps_gpio == GPIO_NUM_NC ||
            config.baud_rate == 0 ||
            config.uart_buffer_size == 0)
        {
            return ESP_ERR_INVALID_ARG;
        }

        config_ = config;

        uart_config_t uart_config = {};
        uart_config.baud_rate = static_cast<int>(config_.baud_rate);
        uart_config.data_bits = UART_DATA_8_BITS;
        uart_config.parity = UART_PARITY_DISABLE;
        uart_config.stop_bits = UART_STOP_BITS_1;
        uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
        uart_config.source_clk = UART_SCLK_DEFAULT;

        esp_err_t err = uart_param_config(config_.uart_port, &uart_config);
        if (err != ESP_OK)
        {
            return err;
        }

        err = uart_set_pin(
            config_.uart_port,
            config_.tx_gpio,
            config_.rx_gpio,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE);
        if (err != ESP_OK)
        {
            return err;
        }

        err = uart_driver_install(
            config_.uart_port,
            config_.uart_buffer_size,
            0,
            0,
            nullptr,
            0);
        if (err != ESP_OK)
        {
            return err;
        }

        gpio_config_t pps_config = {};
        pps_config.pin_bit_mask = 1ULL << config_.pps_gpio;
        pps_config.mode = GPIO_MODE_INPUT;
        pps_config.pull_up_en = GPIO_PULLUP_DISABLE;
        pps_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
        pps_config.intr_type = GPIO_INTR_POSEDGE;

        err = gpio_config(&pps_config);
        if (err != ESP_OK)
        {
            (void)uart_driver_delete(config_.uart_port);
            return err;
        }

        err = gpio_install_isr_service(0);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        {
            (void)uart_driver_delete(config_.uart_port);
            return err;
        }

        err = gpio_isr_handler_add(config_.pps_gpio, pps_isr, this);
        if (err != ESP_OK)
        {
            (void)uart_driver_delete(config_.uart_port);
            return err;
        }

        initialised_ = true;
        return ESP_OK;
    }

    bool Receiver::poll(Data& data, uint32_t wait_ms)
    {
        if (!initialised_)
        {
            return false;
        }

        uint8_t buffer[256] = {};
        const int bytes_read = uart_read_bytes(
            config_.uart_port,
            buffer,
            sizeof(buffer),
            pdMS_TO_TICKS(wait_ms));

        bool data_updated = false;
        for (int index = 0; index < bytes_read; index++)
        {
            data_updated = consume(static_cast<char>(buffer[index])) || data_updated;
        }

        copy_pps_state();
        data = data_;
        return data_updated;
    }

    void IRAM_ATTR Receiver::pps_isr(void *context)
    {
        auto *receiver = static_cast<Receiver *>(context);
        if (receiver == nullptr)
        {
            return;
        }

        portENTER_CRITICAL_ISR(&receiver->pps_lock_);
        receiver->pps_count_++;
        receiver->pps_timestamp_us_ = esp_timer_get_time();
        portEXIT_CRITICAL_ISR(&receiver->pps_lock_);
    }

    bool Receiver::consume(char character)
    {
        if (character == '\r')
        {
            return false;
        }

        if (character == '$')
        {
            line_length_ = 0;
            line_[line_length_++] = character;
            return false;
        }

        if (character == '\n')
        {
            line_[line_length_] = '\0';
            const bool parsed = parse_sentence();
            line_length_ = 0;
            return parsed;
        }

        if (line_length_ >= sizeof(line_) - 1)
        {
            line_length_ = 0;
            return false;
        }

        line_[line_length_++] = character;
        return false;
    }

    bool Receiver::parse_sentence()
    {
        if (line_length_ == 0 || line_[0] != '$')
        {
            return false;
        }

        if (!checksum_is_valid(line_))
        {
            data_.checksum_errors++;
            return false;
        }

        char *fields[MAX_FIELDS] = {};
        const size_t field_count = split_fields(line_, fields);

        bool parsed = false;
        if (field_count > 0 && sentence_type_is(fields[0], "GGA"))
        {
            parsed = parse_gga(fields, field_count, data_);
        }
        else if (field_count > 0 && sentence_type_is(fields[0], "RMC"))
        {
            parsed = parse_rmc(fields, field_count, data_);
        }

        if (parsed)
        {
            data_.sentences_received++;
            data_.last_sentence_timestamp_us = esp_timer_get_time();
        }

        return parsed;
    }

    void Receiver::copy_pps_state()
    {
        portENTER_CRITICAL(&pps_lock_);
        data_.pps_count = pps_count_;
        data_.pps_timestamp_us = pps_timestamp_us_;
        portEXIT_CRITICAL(&pps_lock_);

        constexpr int64_t PPS_STALE_TIME_US = 2'000'000;
        data_.has_pps =
            data_.pps_timestamp_us > 0 &&
            (esp_timer_get_time() - data_.pps_timestamp_us) < PPS_STALE_TIME_US;
    }
}
