cmake_minimum_required(VERSION 3.20)
project(auto_gauge_core VERSION 0.1.0 LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(gauge_core STATIC src/config.c src/can_policy.c src/log_queue.c src/session_clock.c
  src/obd_decode.c src/isotp_rx.c src/scheduler.c src/connection.c)
target_include_directories(gauge_core PUBLIC include)
target_compile_options(gauge_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
option(AG_SANITIZERS "Address/undefined-behavior sanitizer for host tests" ON)
if(AG_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(gauge_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(gauge_core PUBLIC -fsanitize=address,undefined)
endif()

enable_testing()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(AG_SHARED "${CMAKE_CURRENT_SOURCE_DIR}/../shared/fixtures")
add_custom_command(OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/shared_fixture.h"
  COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_fixture.py"
    "${AG_SHARED}/config.valid.json" "${AG_SHARED}/config.cases.json"
    "${CMAKE_CURRENT_BINARY_DIR}/shared_fixture.h"
  DEPENDS tools/generate_fixture.py "${AG_SHARED}/config.valid.json" "${AG_SHARED}/config.cases.json"
  VERBATIM)
add_executable(gauge_core_tests tests/test_core.c idf/main/hal_unimplemented.c "${CMAKE_CURRENT_BINARY_DIR}/shared_fixture.h")
target_include_directories(gauge_core_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(gauge_core_tests PRIVATE gauge_core)
target_compile_options(gauge_core_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
add_test(NAME gauge_core_tests COMMAND gauge_core_tests)
add_executable(gauge_diagnostics_tests tests/test_diagnostics.c)
target_link_libraries(gauge_diagnostics_tests PRIVATE gauge_core)
target_compile_options(gauge_diagnostics_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
add_test(NAME gauge_diagnostics_tests COMMAND gauge_diagnostics_tests)
add_executable(gauge_connection_tests tests/test_connection.c idf/main/hal_unimplemented.c)
target_link_libraries(gauge_connection_tests PRIVATE gauge_core)
target_compile_options(gauge_connection_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -UNDEBUG)
add_test(NAME gauge_connection_tests COMMAND gauge_connection_tests)
