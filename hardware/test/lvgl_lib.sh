#!/bin/sh
# LVGL para el PC (pruebas y capturas sin placa): descarga la misma versión que usa la placa y la compila una vez
# con el lv_conf.h del sketch. Deja la biblioteca en hardware/test/.lvgl/liblvgl-<versión>.a e imprime su ruta.
set -e
export PATH="$HOME/scoop/apps/gcc/current/bin:$PATH"
cd "$(dirname "$0")"
LVGL_TAG=v9.5.0
D=.lvgl/lvgl-$LVGL_TAG
CONF=../esp32s3_pantalla7/lv_conf.h
LIB=.lvgl/liblvgl-$LVGL_TAG.a
if [ ! -d "$D" ]; then
  mkdir -p .lvgl
  git clone -q --depth 1 --branch "$LVGL_TAG" https://github.com/lvgl/lvgl "$D" 2>/dev/null
fi
# Se recompila si cambia lv_conf.h.
STAMP=.lvgl/conf.stamp
if [ ! -f "$LIB" ] || ! cmp -s "$CONF" "$STAMP"; then
  echo "Compilando LVGL para el PC (solo la primera vez o si cambia lv_conf.h)…" >&2
  rm -rf .lvgl/obj && mkdir -p .lvgl/obj
  find "$D/src" -name '*.c' | xargs -P 12 -I{} sh -c \
    'o=.lvgl/obj/$(echo "{}" | sed "s|.*/src/||; s|/|_|g").o; gcc -c -O2 -w -DLV_CONF_INCLUDE_SIMPLE -I../esp32s3_pantalla7 -I'"$D"' "{}" -o "$o"'
  rm -f "$LIB"
  ar rcs "$LIB" .lvgl/obj/*.o
  cp "$CONF" "$STAMP"
fi
echo "$(pwd)/$LIB"
