idf_component_register(SRCS "main.c" "ota_installer.c" "manifest_verify.c" "transfer_adapter.c" "../../src/ota_transfer.c" "framed_adapter.c" "boot_storage.c" "../../src/boot_health.c" "../../src/ota_framed_endpoint.c"
  "../../../shared/c/ag_frame.c" "../../../shared/c/ag_stream.c"
  INCLUDE_DIRS "." "../../include" "../../../shared/c" REQUIRES app_update esp_partition mbedtls nvs_flash)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
target_link_libraries(${COMPONENT_LIB} INTERFACE
  "-Wl,--undefined=ag_ota_installer_init" "-Wl,--undefined=ag_ota_installer_begin"
  "-Wl,--undefined=ag_ota_installer_write" "-Wl,--undefined=ag_ota_installer_finish"
  "-Wl,--undefined=ag_ota_installer_select" "-Wl,--undefined=ag_ota_installer_poll"
  "-Wl,--undefined=ag_ota_installer_cancel"
  "-Wl,--undefined=ag_manifest_verify" "-Wl,--undefined=ag_manifest_verify_hook"
  "-Wl,--undefined=ag_ota_transfer_adapter_init" "-Wl,--undefined=ag_ota_transfer_adapter_handle"
  "-Wl,--undefined=ag_ota_transfer_adapter_poll"
  "-Wl,--undefined=ag_ota_framed_adapter_init" "-Wl,--undefined=ag_ota_framed_adapter_feed"
  "-Wl,--undefined=ag_ota_framed_adapter_step" "-Wl,--undefined=ag_ota_framed_adapter_take_output"
  "-Wl,--undefined=ag_ota_framed_adapter_poll" "-Wl,--undefined=ag_ota_framed_adapter_close"
  "-Wl,--undefined=ag_boot_health_open" "-Wl,--undefined=ag_boot_health_provision"
  "-Wl,--undefined=ag_boot_health_floor" "-Wl,--undefined=ag_boot_health_stage"
  "-Wl,--undefined=ag_boot_health_reconcile" "-Wl,--undefined=ag_boot_health_confirm"
  "-Wl,--undefined=ag_ota_boot_storage_bind" "-Wl,--undefined=ag_ota_boot_prepare_selection"
  "-Wl,--undefined=ag_ota_boot_storage_provision_namespace")
