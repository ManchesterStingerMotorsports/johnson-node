#pragma once

#include <cstddef>
#include <cstdint>

#include "MSM_CAN.hpp"
#include "data/SensorSamples.hpp"

namespace CANPackets
{
    enum class Id : uint16_t
    {
        ADC_A = 0x100,
        ADC_B = 0x101,

        IMU_ACCEL = 0x110,
        IMU_LINEAR_ACCEL = 0x111,
        IMU_GRAVITY = 0x112,
        IMU_GYRO = 0x113,
        IMU_MAG = 0x114,
        IMU_EULER = 0x115,
        IMU_QUATERNION = 0x116,
        IMU_STATUS = 0x117,

        GPS_POSITION = 0x120,
        GPS_MOTION = 0x121,
        GPS_STATUS = 0x122,
        GPS_TIME = 0x123,
    };

    template <size_t Capacity>
    struct FrameBatch
    {
        MSM_CAN::TxFrame frames[Capacity] = {};
    };

    using ADCFrames = FrameBatch<2>;
    using IMUFrames = FrameBatch<8>;
    using GPSFrames = FrameBatch<4>;

    ADCFrames encode(const SensorData::ADCSample& sample);
    IMUFrames encode(const SensorData::IMUSample& sample);
    GPSFrames encode(const SensorData::GPSSample& sample);
}
