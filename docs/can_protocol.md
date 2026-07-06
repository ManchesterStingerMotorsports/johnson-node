# Johnson Node CAN Protocol

The Johnson Node uses 11-bit CAN identifiers at 1 Mbit/s. All multi-byte
values are big-endian. Signed values use two's-complement representation.

This is the initial node-local allocation. It should be reconciled with the
vehicle-wide CAN database before multiple Johnson Nodes are deployed.

## ADC

| ID | Name | Payload |
| --- | --- | --- |
| `0x100` | ADC A | A, B, C, D as four unsigned 16-bit raw counts |
| `0x101` | ADC B | E, F, G, H as four unsigned 16-bit raw counts |

Raw ADC values occupy the range 0 to 4095. Voltage is calculated locally as
`raw * 5.0 / 4096`, but raw values are transmitted so the reference voltage
can be calibrated during analysis.

| Input | Device/channel | Base-board connector |
| --- | --- | --- |
| A | ADC A IN1 | J2 |
| B | ADC A IN2 | J4 |
| C | ADC A IN3 | J3 |
| D | ADC A IN4 | J5 |
| E | ADC B IN1 | J6 |
| F | ADC B IN2 | J7 |
| G | ADC B IN3 | J8 |
| H | ADC B IN4 | J9 |

## IMU

Vector frames contain X, Y, and Z as three signed 16-bit values followed by
two reserved bytes.

| ID | Name | Scale |
| --- | --- | --- |
| `0x110` | Acceleration | `0.01 m/s^2` per bit |
| `0x111` | Linear acceleration | `0.01 m/s^2` per bit |
| `0x112` | Gravity | `0.01 m/s^2` per bit |
| `0x113` | Gyroscope | `0.001 rad/s` per bit |
| `0x114` | Magnetometer | `0.1 uT` per bit |
| `0x115` | Euler orientation | Heading `u16`, roll `i16`, pitch `i16`, two reserved bytes; angles are `0.01 degree` per bit |
| `0x116` | Quaternion | W, X, Y, Z as signed Q14 values |
| `0x117` | IMU status | Packed calibration `u8`, temperature `i8`, six reserved bytes |

The calibration byte is packed as system, gyro, accelerometer, and
magnetometer, with two bits per field in that order.

## GPS

| ID | Name | Payload |
| --- | --- | --- |
| `0x120` | Position | Latitude `i32` and longitude `i32`, both in degrees times `1e7` |
| `0x121` | Motion | Altitude `i32` in centimetres, speed `u16` in centimetres per second, course `u16` in centidegrees |
| `0x122` | GPS status | Flags `u8`, satellites `u8`, HDOP times 100 `u16`, PPS count `u16`, two reserved bytes |
| `0x123` | UTC time | Year `u16`, month, day, hour, minute, second, centisecond |

GPS status flag bit 0 indicates a valid fix. Bit 1 indicates that at least one
PPS edge has been received.
