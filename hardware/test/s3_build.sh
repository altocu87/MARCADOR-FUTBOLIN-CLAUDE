#!/bin/sh
# Compila y ejecuta la prueba del sketch de 7" en el PC con LVGL (uso: sh test/s3_build.sh <placa> [prueba.cpp]).
set -e
export PATH="$HOME/scoop/apps/gcc/current/bin:$PATH"
cd "$(dirname "$0")/.."
T=$(cygpath -m /tmp 2>/dev/null || echo /tmp)
LIB=$(sh test/lvgl_lib.sh)
LIB=$(cygpath -m "$LIB" 2>/dev/null || echo "$LIB")
LV=test/.lvgl/lvgl-v9.5.0
SRC=${2:-test/s3_sim_test.cpp}
g++ -std=c++17 -Wall -Wno-unused-variable -Wno-unused-function -DMFV3_BOARD=$1 -DLV_CONF_INCLUDE_SIMPLE -Itest/mock_s3 -Iesp32s3_pantalla7 \
  -I$LV -I$LV/src -x c++ "$SRC" -x none "$LIB" -o "$T/mfv3_s3_sim" && "$T/mfv3_s3_sim"
