#pragma once

#include <cstddef>
#include <cstdint>

namespace SensorData
{
    static constexpr size_t ADC_CHANNEL_COUNT = 8;

    struct ADCSample
    {
        uint16_t raw[ADC_CHANNEL_COUNT] = {};
    };
}
