idf_component_register(
    SRCS "main.cpp"
         "tasks/ADC/ADC_task.cpp"
         "tasks/CAN/CAN_task.cpp"
         "../libraries/ADC124S051/ADC124S051.cpp"
         "../libraries/MSM_CAN/MSM_CAN.cpp"
    PRIV_REQUIRES esp_driver_gpio esp_driver_spi esp_driver_twai esp_timer
    INCLUDE_DIRS "." "../libraries/ADC124S051" "../libraries/MSM_CAN")
