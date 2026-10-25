idf_component_register(SRCS "main.c" "nimble_transport.c" "endpoint_bridge.c"
    "../../src/config.c" "../../src/can_policy.c" "../../src/connection.c"
    "../../src/admission.c" "../../src/config_store.c" "../../src/history.c"
    "../../src/device_endpoint.c" "../../src/ota_transfer.c" "../../src/ota_framed_endpoint.c" "../../../shared/c/ag_frame.c"
    "../../../shared/c/ag_stream.c" "../../../shared/c/ag_rpc.c"
    "../../../shared/c/ag_config_json.c"
  INCLUDE_DIRS "." "../../include" "../../../shared/c"
  REQUIRES bt nvs_flash esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
# Retain the entry point to validate SDK symbols at link time, never call it.
target_link_libraries(${COMPONENT_LIB} INTERFACE
  "-Wl,--undefined=ag_nimble_transport_start" "-Wl,--undefined=ag_nimble_transport_quiesce" "-Wl,--undefined=ag_nimble_send"
  "-Wl,--undefined=ag_nimble_security" "-Wl,--undefined=ag_nimble_advertise"
  "-Wl,--undefined=ag_nimble_close" "-Wl,--undefined=ag_nimble_lookup_bond"
  "-Wl,--undefined=ag_nimble_bridge_init" "-Wl,--undefined=ag_nimble_bridge_start"
  "-Wl,--undefined=ag_nimble_bridge_pairing_window"
  "-Wl,--undefined=ag_nimble_bridge_init_bound"
  "-Wl,--undefined=ag_nimble_bridge_prepare_ota" "-Wl,--undefined=ag_nimble_bridge_prepare_rpc")
