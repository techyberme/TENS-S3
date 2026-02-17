# components/led_strip/CMakeLists.txt
idf_component_register(SRCS "src/led_strip_api.c"
                            "src/led_strip_rmt_dev.c"
                            "src/led_strip_rmt_encoder.c"
                            "src/led_strip_spi_dev.c"
                       INCLUDE_DIRS "include" 
                       PRIV_INCLUDE_DIRS "interface"  # <--- ESTA ES LA CLAVE
                       REQUIRES driver esp_driver_rmt esp_rom)