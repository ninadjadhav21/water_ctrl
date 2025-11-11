idf_component_register(SRCS "main.c" "water_ctrl.c" "schedule_manager.c" "ntp_time.c" "web_server.c"
                    "water_status.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES "favicon.ico")


spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)