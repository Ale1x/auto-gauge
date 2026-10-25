#!/bin/sh
# Build only; accepts only the declared volatile bench switch, never flash args.
set -eu
AG_APP_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
AG_IDF_ROOT=${AG_IDF_ROOT:-"$HOME/.cache/auto-gauge/esp-idf-v6.1"}
AG_IDF_TOOLS=${AG_IDF_TOOLS:-"$HOME/.cache/auto-gauge/idf-tools-v6.1"}
AG_IDF_PYTHON_ENV=${AG_IDF_PYTHON_ENV:-"$AG_IDF_TOOLS/python_env/idf6.1_py3.14_env"}
AG_XTENSA_BIN=${AG_XTENSA_BIN:-"$AG_IDF_TOOLS/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin"}
AG_APP_BUILD="$AG_APP_ROOT/build"
AG_APP_DEFAULTS="$AG_APP_ROOT/sdkconfig.defaults"
if [ "$#" -eq 1 ] && [ "$1" = --software-bench ]; then
  AG_APP_BUILD="$AG_APP_ROOT/build-bench"
  AG_APP_DEFAULTS="$AG_APP_DEFAULTS;$AG_APP_ROOT/sdkconfig.bench.defaults"
elif [ "$#" -eq 1 ] && [ "$1" = --native-profile ]; then
  AG_APP_BUILD="$AG_APP_ROOT/build-native"
  AG_APP_DEFAULTS="$AG_APP_DEFAULTS;$AG_APP_ROOT/sdkconfig.native.defaults"
elif [ "$#" -ne 0 ]; then
  echo "Build only: supported optional arguments --software-bench or --native-profile" >&2; exit 2
fi
if [ "$(git -C "$AG_IDF_ROOT" rev-parse HEAD)" != fff9895c82d744c7237be8847347bdd1b07c6643 ]; then
  echo "Expected pinned ESP-IDF v6.1 checkout" >&2; exit 1
fi
if [ -n "$(git -C "$AG_IDF_ROOT" status --porcelain=v1 --untracked-files=all --ignore-submodules=none)" ]; then
  echo "Pinned SDK checkout must be clean" >&2; exit 1
fi
exec env IDF_PATH="$AG_IDF_ROOT" IDF_TOOLS_PATH="$AG_IDF_TOOLS" \
  IDF_PYTHON_ENV_PATH="$AG_IDF_PYTHON_ENV" ESP_IDF_VERSION=6.1 IDF_TARGET=esp32s3 \
  PATH="$AG_XTENSA_BIN:$AG_IDF_PYTHON_ENV/bin:$PATH" \
  "$AG_IDF_PYTHON_ENV/bin/python" "$AG_IDF_ROOT/tools/idf.py" \
  -C "$AG_APP_ROOT" -B "$AG_APP_BUILD" -D "SDKCONFIG=$AG_APP_BUILD/sdkconfig" \
  -D "SDKCONFIG_DEFAULTS=$AG_APP_DEFAULTS" build size
