# Pressure sensor meeting example

Pressure sensor → ADC A / IN1 → ADCSample → queue → CAN

Start in [main/main.cpp](main/main.cpp): it creates the queue and starts two tasks.

- [ADC task](main/tasks/ADC/ADC_task.cpp): reads the pressure sensor every 100 ms
  and queues its raw count in ADCSample's raw[0].
- [CAN task](main/tasks/CAN/CAN_task.cpp): takes each sample, packs it and sends it.

- [main/data/ADCSample.hpp](main/data/ADCSample.hpp): existing sample structure.
- [main/config/BoardConfig.hpp](main/config/BoardConfig.hpp): ADC and CAN wiring.
- libraries/: the ADC124S051 and MSM_CAN drivers used by the example.

## Build

Use an ESP-IDF 5.5.2 terminal in this folder:

~~~sh
idf.py set-target esp32s3
idf.py build
~~~

## CAN frame

ID 0x500, eight bytes. Bytes 0–1 contain raw[0] as an unsigned 16-bit,
big-endian value; bytes 2–7 are zero. A raw count of 2068 gives:

~~~text
08 14 00 00 00 00 00 00
~~~

This sends raw ADC counts. The board ADC reference is 5 V; convert to pressure
using the actual sensor calibration and input circuit.

## Scheduled-send alternative

The running example uses send_msg(frame). To use the other method from the
slides, declare bool scheduled = false before the CAN task's loop and replace
the send_msg call with:

~~~cpp
esp_err_t result;
if (!scheduled)
{
    result = MSM_CAN::schedule(frame, 100);
    scheduled = (result == ESP_OK);
}
else
{
    result = MSM_CAN::update_scheduled_payload(frame);
}
~~~

This schedules once, then updates the payload as samples arrive.
If readings stop, the last payload continues to transmit.

## Optional receive setup

For a receive example, put MSM_CAN::set_hardware_filters(0x200) before init(),
then ESP_ERROR_CHECK(MSM_CAN::subscribe(0x200)) after init().
