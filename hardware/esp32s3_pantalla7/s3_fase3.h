// MARCADOR FUTBOLÍN V3 · versión de prueba de la fase 3 (compilada con hardware/fase3/platformio.ini).
// ---------------------------------------------------------------------------------------------------
// Se instala en el hueco del programa que NO está en uso (por Wi-Fi o con «placa.py probar»): el de antes se queda
// entero en el otro. Desde aquí se puede volver a él:
//   - AJUSTES → ACTUALIZACIONES → «VOLVER A LA ANTERIOR» (dos toques) o la orden USB «VOLVER_ANTERIOR»;
//   - sola: si esta versión se reinicia por un fallo tres veces seguidas sin llegar a 2 minutos encendida.
// En las compilaciones normales (arduino-cli) nada de esto existe.

#ifndef MFV3_S3_FASE3_H
#define MFV3_S3_FASE3_H

#if defined(MFV3_FASE3) && defined(ESP_PLATFORM)
#include <esp_ota_ops.h>
#include <esp_system.h>

namespace fase3 {

static const char* const LABEL = "PRUEBA FASE 3";

// El otro hueco, si tiene un programa válido (el anterior).
static const esp_partition_t* previous() {
  const esp_partition_t* other = esp_ota_get_next_update_partition(nullptr);
  esp_app_desc_t d;
  if (!other || esp_ota_get_partition_description(other, &d) != ESP_OK) return nullptr;
  return other;
}

// Arranca con el programa anterior. false si no hay (y no hace nada).
static bool rollback() {
  const esp_partition_t* p = previous();
  if (!p || esp_ota_set_boot_partition(p) != ESP_OK) return false;
  delay(200);
  esp_restart();
  return true;
}

// Al encender: cuenta los reinicios por fallo seguidos (en la NVS); al tercero, vuelve al anterior.
static uint8_t crashes = 0;
static bool healthy = false;
template <typename Prefs> static void onBoot(Prefs& prefs) {
  esp_reset_reason_t r = esp_reset_reason();
  bool crash = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT;
  crashes = crash ? (uint8_t)(prefs.getUChar("f3fallos", 0) + 1) : 0;
  prefs.putUChar("f3fallos", crashes);
  if (crashes >= 3) {
    prefs.putUChar("f3fallos", 0);
    rollback();
  }
}
// Desde loop(): tras 2 minutos encendida sin fallos, la cuenta vuelve a cero.
template <typename Prefs> static void tick(Prefs& prefs, uint32_t now) {
  if (healthy || now < 120000) return;
  healthy = true;
  if (crashes) prefs.putUChar("f3fallos", 0);
}

}  // namespace fase3
#endif

#endif  // MFV3_S3_FASE3_H
