idf_component_register(SRCS "main.c" "ota_installer.c" "manifest_verify.c"
  INCLUDE_DIRS "." REQUIRES app_update esp_partition mbedtls)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror)
target_link_libraries(${COMPONENT_LIB} INTERFACE
  "-Wl,--undefined=ag_ota_installer_init" "-Wl,--undefined=ag_ota_installer_begin"
  "-Wl,--undefined=ag_ota_installer_write" "-Wl,--undefined=ag_ota_installer_finish"
  "-Wl,--undefined=ag_ota_installer_select" "-Wl,--undefined=ag_ota_installer_poll"
  "-Wl,--undefined=ag_ota_installer_cancel"
  "-Wl,--undefined=ag_manifest_verify" "-Wl,--undefined=ag_manifest_verify_hook")
