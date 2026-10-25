idf_component_register(SRCS "main.c" "partition_io.c" "twai_port.c"
  "../../src/runtime.c" "../../src/discovery.c" "../../src/can_policy.c"
  "../../src/obd_decode.c" "../../src/scheduler.c" "../../src/isotp_rx.c"
  "../../src/config.c" "../../src/config_store.c" "../../src/history.c" "../../src/journal_history.c"
  INCLUDE_DIRS "." "../../include" REQUIRES esp_partition spi_flash esp_driver_twai esp_timer esp_driver_gpio)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
target_link_libraries(${COMPONENT_LIB} INTERFACE
  "-Wl,--undefined=ag_partition_store_bind"
  "-Wl,--undefined=ag_config_store_open" "-Wl,--undefined=ag_config_store_restore"
  "-Wl,--undefined=ag_config_store_commit" "-Wl,--undefined=ag_config_store_get"
  "-Wl,--undefined=ag_config_record_encode"
  "-Wl,--undefined=ag_journal_open" "-Wl,--undefined=ag_journal_bootstrap"
  "-Wl,--undefined=ag_journal_begin_session" "-Wl,--undefined=ag_journal_append"
  "-Wl,--undefined=ag_journal_window" "-Wl,--undefined=ag_journal_acknowledge"
  "-Wl,--undefined=ag_twai_port_init" "-Wl,--undefined=ag_twai_port_start"
  "-Wl,--undefined=ag_twai_port_send_discovery" "-Wl,--undefined=ag_twai_port_send_runtime"
  "-Wl,--undefined=ag_twai_port_send_flow_control" "-Wl,--undefined=ag_twai_port_poll"
  "-Wl,--undefined=ag_twai_port_close")
