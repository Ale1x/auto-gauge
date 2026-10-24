#!/bin/sh
# Build only: deliberately no flash/monitor/eFuse command or forwarded actions.
set -eu
AG_REPO_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
AG_IDF_ROOT=${AG_IDF_ROOT:-"$HOME/.cache/auto-gauge/esp-idf-v6.1"}
AG_IDF_TOOLS=${AG_IDF_TOOLS:-"$HOME/.cache/auto-gauge/idf-tools-v6.1"}
AG_IDF_PYTHON_ENV=${AG_IDF_PYTHON_ENV:-"$AG_IDF_TOOLS/python_env/idf6.1_py3.14_env"}
AG_XTENSA_BIN=${AG_XTENSA_BIN:-"$AG_IDF_TOOLS/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin"}
AG_IDF_BUILD_DIR=${AG_IDF_BUILD_DIR:-"$AG_REPO_ROOT/firmware/idf/build"}
if [ "$#" -ne 0 ]; then
    echo "This script accepts no actions: build and size only." >&2
    exit 2
fi
if [ ! -x "$AG_IDF_PYTHON_ENV/bin/python" ] || [ ! -x "$AG_XTENSA_BIN/xtensa-esp32s3-elf-gcc" ]; then
    echo "ESP-IDF Python environment or Xtensa compiler missing; see firmware/DEVICE_BUILD.md." >&2
    exit 1
fi
if [ "$(git -C "$AG_IDF_ROOT" rev-parse HEAD)" != "fff9895c82d744c7237be8847347bdd1b07c6643" ]; then
    echo "Expected the verified ESP-IDF v6.1 source commit; use a separate qualified SDK for another release." >&2
    exit 1
fi
exec env IDF_PATH="$AG_IDF_ROOT" IDF_TOOLS_PATH="$AG_IDF_TOOLS" \
    IDF_PYTHON_ENV_PATH="$AG_IDF_PYTHON_ENV" ESP_IDF_VERSION=6.1 IDF_TARGET=esp32s3 \
    PIP_CACHE_DIR="$HOME/.cache/auto-gauge/pip" \
    PATH="$AG_XTENSA_BIN:$AG_IDF_PYTHON_ENV/bin:$PATH" \
    "$AG_IDF_PYTHON_ENV/bin/python" "$AG_IDF_ROOT/tools/idf.py" \
    -C "$AG_REPO_ROOT/firmware/idf" -B "$AG_IDF_BUILD_DIR" \
    -D "SDKCONFIG=$AG_IDF_BUILD_DIR/sdkconfig" build size
