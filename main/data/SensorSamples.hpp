#pragma once

#include <cstddef>
#include <cstdint>

#include "BNO055.hpp"
#include "GPS.hpp"

namespace SensorData
{
    static constexpr size_t ADC_CHANNEL_COUNT = 8;

    struct ADCSample
    {
        uint16_t raw[ADC_CHANNEL_COUNT] = {};
    };

    using IMUSample = BNO055::Data; 
    using GPSSample = GPS::Data; 
}
