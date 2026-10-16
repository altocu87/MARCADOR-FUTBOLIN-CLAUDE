#!/bin/sh
# Capturas de la pantalla de 7" (ESP32-S3) sin la placa: compila el sketch en el PC con la LovyanGFX de verdad
# dibujando en un lienzo de 800×480 y guarda un PNG por pantalla en docs/capturas-placa/.
# Necesita g++, gcc, zlib y git (la primera vez descarga LovyanGFX en hardware/test/.lgfx).
set -e
cd "$(dirname "$0")/.."
LGFX_TAG=1.2.33
LG=test/.lgfx/LovyanGFX-$LGFX_TAG
OUT=${1:-../docs/capturas-placa}
if [ ! -d "$LG" ]; then
  mkdir -p test/.lgfx
  git clone -q --depth 1 --branch "$LGFX_TAG" https://github.com/lovyan03/LovyanGFX "$LG"
fi
LIB=test/.lgfx/liblgfx-$LGFX_TAG.a
SKIP='platforms/(esp|rp2040|samd|stm32|spresense|arduino|sdl|opencv)'
if [ ! -f "$LIB" ]; then
  echo "Compilando LovyanGFX para el PC (solo la primera vez)…"
  rm -rf test/.lgfx/obj && mkdir -p test/.lgfx/obj
  for f in $(find "$LG/src" -name '*.c' -o -name '*.cpp' | grep -v -E "$SKIP"); do
    o=test/.lgfx/obj/$(echo "$f" | tr '/' '_').o
    case "$f" in
      *.c) gcc -c -O1 -w -I"$LG/src" "$f" -o "$o" ;;
      *) g++ -c -O1 -w -std=c++17 -DLGFX_LINUX_FB -I"$LG/src" "$f" -o "$o" ;;
    esac
  done
  ar rcs "$LIB" test/.lgfx/obj/*.o
fi
T=/tmp/mfv3_capturas
rm -rf "$T" && mkdir -p "$T/sketch"
cp esp32s3_pantalla7/*.h esp32s3_pantalla7/esp32s3_pantalla7.ino "$T/sketch/"
cp test/render/board_config.h "$T/sketch/"   # placa de PC en lugar de la real
g++ -std=c++17 -O1 -w -DLGFX_LINUX_FB -I"$LG/src" -I"$T/sketch" -Itest/mock_s3 -x c++ test/render/capturas.cpp \
  -x none "$LIB" -lz -lpthread -o "$T/capturas"
mkdir -p "$OUT"
echo "Capturas en $OUT:"
"$T/capturas" "$OUT"
