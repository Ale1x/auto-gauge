#!/bin/sh
# Build/size only, no forwarded actions or device access.
set -eu
AG_STUDY_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
AG_IDF_ROOT=${AG_IDF_ROOT:-"$HOME/.cache/auto-gauge/esp-idf-v6.1"}
AG_IDF_TOOLS=${AG_IDF_TOOLS:-"$HOME/.cache/auto-gauge/idf-tools-v6.1"}
AG_IDF_PYTHON_ENV=${AG_IDF_PYTHON_ENV:-"$AG_IDF_TOOLS/python_env/idf6.1_py3.14_env"}
AG_XTENSA_BIN=${AG_XTENSA_BIN:-"$AG_IDF_TOOLS/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin"}
if [ "$#" -ne 0 ]; then echo "Build/size only; no actions accepted." >&2; exit 2; fi
if [ "$(git -C "$AG_IDF_ROOT" rev-parse HEAD)" != fff9895c82d744c7237be8847347bdd1b07c6643 ]; then
    echo "Expected pinned ESP-IDF v6.1 checkout" >&2; exit 1
fi
exec env IDF_PATH="$AG_IDF_ROOT" IDF_TOOLS_PATH="$AG_IDF_TOOLS" \
    IDF_PYTHON_ENV_PATH="$AG_IDF_PYTHON_ENV" ESP_IDF_VERSION=6.1 IDF_TARGET=esp32s3 \
    PATH="$AG_XTENSA_BIN:$AG_IDF_PYTHON_ENV/bin:$PATH" \
    "$AG_IDF_PYTHON_ENV/bin/python" "$AG_IDF_ROOT/tools/idf.py" \
    -C "$AG_STUDY_ROOT" -B "$AG_STUDY_ROOT/build" \
    -D "SDKCONFIG=$AG_STUDY_ROOT/build/sdkconfig" build size
