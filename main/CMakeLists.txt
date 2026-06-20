idf_component_register(SRCS "main.cpp"
                            "tasks/CAN/CAN_task.cpp"
                            "tasks/IMU_GPS/IMU_GPS_task.cpp"
                            "../libraries/BNO055/BNO055.cpp"
                       PRIV_REQUIRES esp_driver_gpio esp_driver_i2c esp_timer
                       INCLUDE_DIRS "." "../libraries/BNO055")
