idf_component_register(SRCS "main.cpp"
                            "data/SensorQueues.cpp"
                            "tasks/ADC/ADC_task.cpp"
                            "tasks/CAN/CAN_task.cpp"
                            "tasks/CAN/CAN_packets.cpp"
                            "tasks/GPS/GPS_task.cpp"
                            "tasks/IMU/IMU_task.cpp"
                            "../libraries/ADC124S051/ADC124S051.cpp"
                            "../libraries/BNO055/BNO055.cpp"
                            "../libraries/GPS/GPS.cpp"
                            "../libraries/MSM_CAN/MSM_CAN.cpp"
                       PRIV_REQUIRES esp_driver_gpio
                                     esp_driver_i2c
                                     esp_driver_spi
                                     esp_driver_twai
                                     esp_driver_uart
                                     esp_timer
                       INCLUDE_DIRS "."
                                    "../libraries/ADC124S051"
                                    "../libraries/BNO055"
                                    "../libraries/GPS"
                                    "../libraries/MSM_CAN")
