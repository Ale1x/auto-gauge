idf_component_register(
  SRCS "app_main.c" "hal_unimplemented.c"
    "../../src/config.c" "../../src/can_policy.c"
    "../../src/log_queue.c" "../../src/session_clock.c"
    "../../src/obd_decode.c" "../../src/isotp_rx.c" "../../src/scheduler.c" "../../src/connection.c" "../../src/history.c"
    "../../src/config_store.c" "../../src/flash_sim.c" "../../src/admission.c"
    "../../src/flash_history.c" "../../src/flash_history_sim.c"
    "../../src/device_endpoint.c"
    "../../src/journal_history.c"
    "../../src/journal_endpoint.c"
    "../../../shared/c/ag_config_json.c"
    "../../../shared/c/ag_frame.c" "../../../shared/c/ag_stream.c"
    "../../../shared/c/ag_rpc.c"
  INCLUDE_DIRS "../../include" "../../../shared/c"
  REQUIRES esp_timer esp_hw_support)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
