set(AG_NATIVE_SOURCES)
set(AG_NATIVE_INCLUDES)
set(AG_NATIVE_REQUIRES)
if(CONFIG_AG_NATIVE_PROFILE)
  if(NOT CONFIG_BT_NIMBLE_ENABLED)
    message(FATAL_ERROR "Native profile requires the explicit secure NimBLE configuration")
  endif()
  list(APPEND AG_NATIVE_SOURCES "native_profile.c"
    "../../platform_study/main/partition_io.c" "../../platform_study/main/maintenance_store.c"
    "../../platform_study/main/twai_port.c"
    "../../ble_study/main/nimble_transport.c" "../../ble_study/main/endpoint_bridge.c"
    "../../ota_study/main/boot_storage.c" "../../ota_study/main/manifest_verify.c"
    "../../ota_study/main/ota_installer.c" "../../ota_study/main/transfer_adapter.c"
    "../../src/ota_transfer.c" "../../src/ota_framed_endpoint.c")
  list(APPEND AG_NATIVE_INCLUDES "../../platform_study/main" "../../ble_study/main" "../../ota_study/main")
  list(APPEND AG_NATIVE_REQUIRES bt nvs_flash app_update esp_partition spi_flash mbedtls esp_driver_twai esp_driver_gpio)
endif()
idf_component_register(
  SRCS "app_main.c" "hal_unimplemented.c" ${AG_NATIVE_SOURCES}
    "../../src/config.c" "../../src/can_policy.c"
    "../../src/log_queue.c" "../../src/session_clock.c"
    "../../src/obd_decode.c" "../../src/isotp_rx.c" "../../src/scheduler.c" "../../src/connection.c" "../../src/history.c"
    "../../src/config_store.c" "../../src/flash_sim.c" "../../src/admission.c"
    "../../src/flash_history.c" "../../src/flash_history_sim.c"
    "../../src/device_endpoint.c"
    "../../src/journal_history.c"
    "../../src/journal_endpoint.c"
    "../../src/application.c" "../../src/bench_profile.c" "../../src/gauge_renderer.c" "../../src/runtime.c" "../../src/discovery.c" "../../src/boot_health.c" "../../src/gauge_ui.c"
    "../../../shared/c/ag_config_json.c"
    "../../../shared/c/ag_frame.c" "../../../shared/c/ag_stream.c"
    "../../../shared/c/ag_rpc.c"
  INCLUDE_DIRS "../../include" "../../../shared/c" ${AG_NATIVE_INCLUDES}
  # Dependencies are unconditional: IDF's early dependency scan runs before
  # CONFIG_AG_NATIVE_PROFILE is available; only native SRCS are conditional.
  REQUIRES esp_timer esp_hw_support bt nvs_flash app_update esp_partition spi_flash mbedtls esp_driver_twai esp_driver_gpio)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
