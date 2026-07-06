#pragma once

#include "SensorSamples.hpp"
#include "esp_err.h"

namespace SensorQueues
{
    esp_err_t init();

    void publish(const SensorData::ADCSample& sample);
    void publish(const SensorData::IMUSample& sample);
    void publish(const SensorData::GPSSample& sample);

    bool take(SensorData::ADCSample& sample);
    bool take(SensorData::IMUSample& sample);
    bool take(SensorData::GPSSample& sample);
}
