idf_component_register(SRCS "main.c" "wasm_runner.c" "function_registry.c"
                    INCLUDE_DIRS "."
                    REQUIRES wamr esp_partition esp_system driver log)
