#!/bin/sh
# Comprueba que cada sketch lleva una copia idéntica de los ficheros comunes y ejecuta las pruebas en PC.
set -e
export PATH="$HOME/scoop/apps/gcc/current/bin:$PATH"  # g++ en Windows (scoop); en Linux no cambia nada
cd "$(dirname "$0")/.."
check() { cmp -s "common/$1" "$2/$1" || { echo "Copia desactualizada en $2/ (copia common/$1)"; exit 1; }; }
for d in arduino_usb esp32_marcador esp32s3_pantalla7 mando_pulsadores; do check mfv3_core.h "$d"; done
for d in arduino_usb esp32_marcador; do check mfv3_boards.h "$d"; done
check mfv3_engine.h esp32s3_pantalla7
check mfv3_sound.h esp32s3_pantalla7
for d in esp32s3_pantalla7 mando_pulsadores; do check mfv3_radio.h "$d"; check mfv3_ota.h "$d"; done
g++ -std=c++11 -Wall -Wextra -Icommon test/core_test.cpp -o /tmp/mfv3_core_test && /tmp/mfv3_core_test
g++ -std=c++11 -Wall -Wextra -Icommon test/engine_test.cpp -o /tmp/mfv3_engine_test && /tmp/mfv3_engine_test
g++ -std=c++11 -Wall -Wextra -Icommon test/ota_test.cpp -o /tmp/mfv3_ota_test && /tmp/mfv3_ota_test
g++ -std=c++11 -Wall -Wextra -Icommon test/radio_test.cpp -o /tmp/mfv3_radio_test && /tmp/mfv3_radio_test
g++ -std=c++11 -Wall -Wextra -Icommon test/sound_test.cpp -o /tmp/mfv3_sound_test && /tmp/mfv3_sound_test
g++ -std=c++17 -Wall -Wno-unused-function -Itest/mock_s3 -Iesp32s3_pantalla7 test/jugadores_test.cpp -o /tmp/mfv3_jug_test && /tmp/mfv3_jug_test
# Cola y coordinación de la nube: transporte falso, fallos de memoria y dos tareas reales del PC.
g++ -std=c++17 -pthread -Wall -Wno-unused-function -Wno-unused-variable -Itest/mock_s3 -Iesp32s3_pantalla7 test/nube_memory_test.cpp -x c esp32s3_pantalla7/fotojpg.c -o /tmp/mfv3_nube_test && /tmp/mfv3_nube_test

# ELO, XP, niveles y logros de la placa = los de la app (tests/fixtures/progresion)
g++ -std=c++17 -Wall -Wno-unused-function -Wno-unused-variable -Itest/mock_s3 -Iesp32s3_pantalla7 test/progresion_test.cpp -o /tmp/mfv3_prog_test && /tmp/mfv3_prog_test
g++ -std=c++17 -Wall -Wno-unused-variable -DARDUINO_ARCH_AVR -DARDUINO_AVR_UNO -Itest/mock -Iarduino_usb -x c++ test/arduino_sim_test.cpp -o /tmp/mfv3_ard_sim && /tmp/mfv3_ard_sim
# ESP32 clásico, C3, S3 y S2 (este último sin Bluetooth)
for t in CONFIG_IDF_TARGET_ESP32 CONFIG_IDF_TARGET_ESP32C3 CONFIG_IDF_TARGET_ESP32S3 CONFIG_IDF_TARGET_ESP32S2; do
  g++ -std=c++17 -Wall -Wno-unused-variable -DARDUINO_ARCH_ESP32 -D$t=1 -Itest/mock_esp32 -Iesp32_marcador -x c++ test/esp32_sim_test.cpp -o /tmp/mfv3_esp_sim && /tmp/mfv3_esp_sim || exit 1
done
g++ -std=c++17 -Wall -Wno-unused-variable -Itest/mock_s3 -Imando_pulsadores -x c++ test/mando_sim_test.cpp -o /tmp/mfv3_mando_sim && /tmp/mfv3_mando_sim
# Pantalla de 7": Waveshare 7C (4, por defecto), Elecrow (1) y Waveshare 7 (2)
# (con LVGL de verdad, compilada para el PC por test/lvgl_lib.sh)
for b in 4 1 2; do sh test/s3_build.sh $b || exit 1; done
sh test/s3_build.sh 4 test/s3_performance_test.cpp
sh test/s3_build.sh 4 test/s3_lightweight_test.cpp
sh test/s3_build.sh 4 test/s3_updates_test.cpp
sh test/s3_build.sh 4 test/s3_usb_diagnostics_test.cpp
g++ -std=c++17 -Wall -Wno-unused-function -Wno-unused-variable -Itest/mock_s3 -Iesp32s3_pantalla7 test/nube_session_test.cpp -x c esp32s3_pantalla7/fotojpg.c -o /tmp/mfv3_session_test && /tmp/mfv3_session_test
LV=test/.lvgl/lvgl-v9.5.0
sh test/lvgl_lib.sh > /dev/null
LVLIB=test/.lvgl/liblvgl-v9.5.0.a  # ruta relativa: la carpeta del proyecto puede tener espacios

# Igual que el IDE de Arduino: declarar todas las funciones del .ino al principio y volver a compilar
# (detecta en el PC los errores «X does not name a type» que solo salen al compilar de verdad).
# Ruta de /tmp que entiende el compilador (en Windows, g++ no conoce las rutas /tmp de la consola).
TMPN=$(cygpath -m /tmp 2>/dev/null || echo /tmp)
proto() {  # proto <carpeta> <prueba> <mocks> [defines]
  rm -rf /tmp/mfv3_proto && mkdir -p "/tmp/mfv3_proto/$1"
  cp "$1"/*.h "/tmp/mfv3_proto/$1/"
  python3 test/arduino_prototypes.py "$1/$1.ino" "/tmp/mfv3_proto/$1/$1.ino" > /dev/null
  sed "s#../$1/$1.ino#$TMPN/mfv3_proto/$1/$1.ino#" "test/$2" > /tmp/mfv3_proto/prueba.cpp
  g++ -std=c++17 -w $4 -I"$3" -I"/tmp/mfv3_proto/$1" -x c++ /tmp/mfv3_proto/prueba.cpp -x none $5 -o /tmp/mfv3_proto/prueba && /tmp/mfv3_proto/prueba > /dev/null || exit 1
  echo "$1: compila con las declaraciones del IDE de Arduino"
}
proto arduino_usb arduino_sim_test.cpp test/mock "-DARDUINO_ARCH_AVR -DARDUINO_AVR_UNO"
proto esp32_marcador esp32_sim_test.cpp test/mock_esp32 "-DARDUINO_ARCH_ESP32 -DCONFIG_IDF_TARGET_ESP32=1"
proto mando_pulsadores mando_sim_test.cpp test/mock_s3
proto esp32s3_pantalla7 s3_sim_test.cpp test/mock_s3 "-DMFV3_BOARD=4 -DLV_CONF_INCLUDE_SIMPLE -I$LV -I$LV/src" "-x c esp32s3_pantalla7/fotojpg.c -x none $LVLIB"
