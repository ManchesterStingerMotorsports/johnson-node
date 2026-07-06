#include "CAN_packets.hpp"

#include <cmath>
#include <limits>

namespace
{
    MSM_CAN::TxFrame make_frame(CANPackets::Id id)
    {
        MSM_CAN::TxFrame frame{};
        frame.id = static_cast<uint16_t>(id);
        MSM_CAN::clear_payload(frame.data);
        return frame;
    }

    int16_t scaled_i16(float value, float scale)
    {
        if (!std::isfinite(value))
        {
            return 0;
        }

        const long scaled = std::lroundf(value * scale);
        const long minimum = std::numeric_limits<int16_t>::min();
        const long maximum = std::numeric_limits<int16_t>::max();

        if (scaled < minimum)
        {
            return static_cast<int16_t>(minimum);
        }
        if (scaled > maximum)
        {
            return static_cast<int16_t>(maximum);
        }
        return static_cast<int16_t>(scaled);
    }

    uint16_t scaled_u16(float value, float scale)
    {
        if (!std::isfinite(value) || value <= 0.0f)
        {
            return 0;
        }

        const long scaled = std::lroundf(value * scale);
        const long maximum = std::numeric_limits<uint16_t>::max();
        return static_cast<uint16_t>(scaled > maximum ? maximum : scaled);
    }

    int32_t scaled_i32(double value, double scale)
    {
        if (!std::isfinite(value))
        {
            return 0;
        }

        const long long scaled = std::llround(value * scale);
        const long long minimum = std::numeric_limits<int32_t>::min();
        const long long maximum = std::numeric_limits<int32_t>::max();

        if (scaled < minimum)
        {
            return static_cast<int32_t>(minimum);
        }
        if (scaled > maximum)
        {
            return static_cast<int32_t>(maximum);
        }
        return static_cast<int32_t>(scaled);
    }

    void pack_vector(MSM_CAN::TxFrame& frame,
                     const BNO055::Vector3& vector,
                     float scale)
    {
        MSM_CAN::pack_i16(frame.data, 0, scaled_i16(vector.x, scale));
        MSM_CAN::pack_i16(frame.data, 2, scaled_i16(vector.y, scale));
        MSM_CAN::pack_i16(frame.data, 4, scaled_i16(vector.z, scale));
    }

    uint8_t pack_calibration(const BNO055::Calibration& calibration)
    {
        return static_cast<uint8_t>(
            ((calibration.system & 0x03U) << 6) |
            ((calibration.gyro & 0x03U) << 4) |
            ((calibration.accel & 0x03U) << 2) |
            (calibration.mag & 0x03U));
    }

}

namespace CANPackets
{
    ADCFrames encode(const SensorData::ADCSample& sample)
    {
        ADCFrames batch{};

        batch.frames[0] = make_frame(Id::ADC_A);
        batch.frames[1] = make_frame(Id::ADC_B);

        for (uint8_t channel = 0; channel < 4; channel++)
        {
            MSM_CAN::pack_u16(
                batch.frames[0].data,
                channel * 2,
                sample.raw[channel]);
            MSM_CAN::pack_u16(
                batch.frames[1].data,
                channel * 2,
                sample.raw[channel + 4]);
        }

        return batch;
    }

    IMUFrames encode(const SensorData::IMUSample& sample)
    {
        IMUFrames batch{};

        batch.frames[0] = make_frame(Id::IMU_ACCEL);
        pack_vector(batch.frames[0], sample.accel_mps2, 100.0f);

        batch.frames[1] = make_frame(Id::IMU_LINEAR_ACCEL);
        pack_vector(
            batch.frames[1],
            sample.linear_accel_mps2,
            100.0f);

        batch.frames[2] = make_frame(Id::IMU_GRAVITY);
        pack_vector(batch.frames[2], sample.gravity_mps2, 100.0f);

        batch.frames[3] = make_frame(Id::IMU_GYRO);
        pack_vector(batch.frames[3], sample.gyro_rad_s, 1'000.0f);

        batch.frames[4] = make_frame(Id::IMU_MAG);
        pack_vector(batch.frames[4], sample.mag_uT, 10.0f);

        batch.frames[5] = make_frame(Id::IMU_EULER);
        MSM_CAN::pack_u16(
            batch.frames[5].data,
            0,
            scaled_u16(sample.euler.heading_deg, 100.0f));
        MSM_CAN::pack_i16(
            batch.frames[5].data,
            2,
            scaled_i16(sample.euler.roll_deg, 100.0f));
        MSM_CAN::pack_i16(
            batch.frames[5].data,
            4,
            scaled_i16(sample.euler.pitch_deg, 100.0f));

        batch.frames[6] = make_frame(Id::IMU_QUATERNION);
        MSM_CAN::pack_i16(
            batch.frames[6].data,
            0,
            scaled_i16(sample.quaternion.w, 16'384.0f));
        MSM_CAN::pack_i16(
            batch.frames[6].data,
            2,
            scaled_i16(sample.quaternion.x, 16'384.0f));
        MSM_CAN::pack_i16(
            batch.frames[6].data,
            4,
            scaled_i16(sample.quaternion.y, 16'384.0f));
        MSM_CAN::pack_i16(
            batch.frames[6].data,
            6,
            scaled_i16(sample.quaternion.z, 16'384.0f));

        batch.frames[7] = make_frame(Id::IMU_STATUS);
        MSM_CAN::pack_u8(
            batch.frames[7].data,
            0,
            pack_calibration(sample.calibration));
        MSM_CAN::pack_i8(batch.frames[7].data, 1, sample.temperature_c);

        return batch;
    }

    GPSFrames encode(const SensorData::GPSSample& sample)
    {
        GPSFrames batch{};

        batch.frames[0] = make_frame(Id::GPS_POSITION);
        MSM_CAN::pack_u32(
            batch.frames[0].data,
            0,
            static_cast<uint32_t>(scaled_i32(sample.latitude_deg, 10'000'000.0)));
        MSM_CAN::pack_u32(
            batch.frames[0].data,
            4,
            static_cast<uint32_t>(scaled_i32(sample.longitude_deg, 10'000'000.0)));

        batch.frames[1] = make_frame(Id::GPS_MOTION);
        MSM_CAN::pack_u32(
            batch.frames[1].data,
            0,
            static_cast<uint32_t>(scaled_i32(sample.altitude_m, 100.0)));
        constexpr float KNOTS_TO_CENTIMETRES_PER_SECOND = 51.4444f;
        MSM_CAN::pack_u16(
            batch.frames[1].data,
            4,
            scaled_u16(
                sample.speed_knots,
                KNOTS_TO_CENTIMETRES_PER_SECOND));
        MSM_CAN::pack_u16(
            batch.frames[1].data,
            6,
            scaled_u16(sample.course_deg, 100.0f));

        batch.frames[2] = make_frame(Id::GPS_STATUS);
        uint8_t flags = 0;
        MSM_CAN::set_bit(flags, 0, sample.has_fix);
        MSM_CAN::set_bit(flags, 1, sample.has_pps);
        MSM_CAN::pack_u8(batch.frames[2].data, 0, flags);
        MSM_CAN::pack_u8(batch.frames[2].data, 1, sample.satellites);
        MSM_CAN::pack_u16(
            batch.frames[2].data,
            2,
            scaled_u16(sample.hdop, 100.0f));
        MSM_CAN::pack_u16(
            batch.frames[2].data,
            4,
            static_cast<uint16_t>(sample.pps_count));

        batch.frames[3] = make_frame(Id::GPS_TIME);
        MSM_CAN::pack_u16(batch.frames[3].data, 0, sample.utc.year);
        MSM_CAN::pack_u8(batch.frames[3].data, 2, sample.utc.month);
        MSM_CAN::pack_u8(batch.frames[3].data, 3, sample.utc.day);
        MSM_CAN::pack_u8(batch.frames[3].data, 4, sample.utc.hour);
        MSM_CAN::pack_u8(batch.frames[3].data, 5, sample.utc.minute);
        MSM_CAN::pack_u8(batch.frames[3].data, 6, sample.utc.second);
        MSM_CAN::pack_u8(batch.frames[3].data, 7, sample.utc.centisecond);

        return batch;
    }
}
