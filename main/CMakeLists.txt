idf_component_register(SRCS "main.cpp"
                       PRIV_REQUIRES spi_flash
                       REQUIRES PMW3360
                       REQUIRES tinyusb
                       INCLUDE_DIRS ".")
