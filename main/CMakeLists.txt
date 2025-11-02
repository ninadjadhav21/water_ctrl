idf_component_register(SRCS "main.c" "file_server.c" "water_ctrl.c" "schedule_manager.c" "ntp_time.c" "web_server.c"
                    INCLUDE_DIRS ".")


spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)