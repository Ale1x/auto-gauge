idf_component_register(
  SRCS "app_main.c" "hal_unimplemented.c"
    "../../src/config.c" "../../src/can_policy.c"
    "../../src/log_queue.c" "../../src/session_clock.c"
    "../../src/obd_decode.c" "../../src/isotp_rx.c" "../../src/scheduler.c" "../../src/connection.c" "../../src/history.c"
    "../../src/config_store.c" "../../src/flash_sim.c"
  INCLUDE_DIRS "../../include"
  REQUIRES esp_timer esp_hw_support)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
