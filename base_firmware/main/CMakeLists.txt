idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES wamr esp_partition esp_system driver log)
