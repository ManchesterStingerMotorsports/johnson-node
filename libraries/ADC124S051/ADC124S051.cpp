#include "ADC124S051.hpp"

namespace
{
    constexpr uint16_t ADC_RESULT_MASK = 0x0FFF;
    constexpr float ADC_CODE_COUNT = 4096.0f;
    constexpr uint8_t CHANNEL_SELECT_SHIFT = 11;

    uint16_t byte_swap(uint16_t value)
    {
        return static_cast<uint16_t>((value << 8) | (value >> 8));
    }

    uint16_t make_command(ADC124S051::Channel channel)
    {
        const auto channel_number = static_cast<uint8_t>(channel);
        return static_cast<uint16_t>(channel_number << CHANNEL_SELECT_SHIFT);
    }
}

esp_err_t ADC124S051::init(spi_host_device_t spi_host,
                           gpio_num_t chip_select_gpio,
                           uint32_t clock_hz,
                           float reference_voltage)
{
    if (device_ != nullptr)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (chip_select_gpio == GPIO_NUM_NC ||
        clock_hz == 0 ||
        reference_voltage <= 0.0f)
    {
        return ESP_ERR_INVALID_ARG;
    }

    spi_device_interface_config_t config = {};
    config.clock_speed_hz = static_cast<int>(clock_hz);
    config.mode = 0;
    config.spics_io_num = chip_select_gpio;
    config.queue_size = 1;

    const esp_err_t err = spi_bus_add_device(spi_host, &config, &device_);
    if (err == ESP_OK)
    {
        reference_voltage_ = reference_voltage;
    }

    return err;
}

esp_err_t ADC124S051::read(Channel channel, uint16_t& raw_count)
{
    if (device_ == nullptr)
    {
        return ESP_ERR_INVALID_STATE;
    }

    // The selected channel is converted during the following transaction.
    uint16_t previous_conversion = 0;
    esp_err_t err = transfer(channel, previous_conversion);
    if (err != ESP_OK)
    {
        return err;
    }

    return transfer(channel, raw_count);
}

float ADC124S051::counts_to_voltage(uint16_t raw_count) const
{
    return (static_cast<float>(raw_count & ADC_RESULT_MASK) *
            reference_voltage_) /
           ADC_CODE_COUNT;
}

esp_err_t ADC124S051::transfer(Channel channel, uint16_t& raw_count)
{
    const uint16_t tx_word = byte_swap(make_command(channel));
    uint16_t rx_word = 0;

    spi_transaction_t transaction = {};
    transaction.length = 16;
    transaction.tx_buffer = &tx_word;
    transaction.rx_buffer = &rx_word;

    const esp_err_t err = spi_device_transmit(device_, &transaction);
    if (err == ESP_OK)
    {
        raw_count = byte_swap(rx_word) & ADC_RESULT_MASK;
    }

    return err;
}
