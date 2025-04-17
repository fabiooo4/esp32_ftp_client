idf_component_register(SRCS "esp32_ftp_client.c" INCLUDE_DIRS ".")
spiffs_create_partition_image(storage ../partition FLASH_IN_PROJECT)
