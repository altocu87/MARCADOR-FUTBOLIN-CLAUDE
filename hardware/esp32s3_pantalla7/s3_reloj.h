// MARCADOR FUTBOLÍN V3 · reloj (RTC) y batería de la pantalla de 7"
// --------------------------------------------------------------------
// Solo en la Waveshare ESP32-S3-Touch-LCD-7C / 7C-BOX, que trae en el mismo bus I2C que el táctil (GPIO47/48):
//   - Reloj PCF85063A (dirección 0x51) con su pila CR927: guarda la hora aunque se apague la placa.
//     Registros 0x04–0x0A (segundos…año) en BCD; el bit 7 de los segundos (OS) indica que el reloj se paró
//     (sin pila o nunca puesto en hora): entonces la hora no vale hasta que alguien la ponga.
//   - Medidor de batería BQ27220 (dirección 0x55): BatteryStatus 0x0A (bit 3 = batería puesta), tensión 0x08 (mV),
//     corriente 0x0C (mA, positiva = cargando) y carga 0x2C (%). Se lee tal cual, sin configurarlo: el porcentaje
//     es orientativo hasta que el medidor aprende la capacidad de la batería (un par de cargas completas).
// La hora se guarda en hora local (España peninsular). La ponen la app por el USB («HORA AAAA-MM-DD HH:MM:SS») y el
// Wi-Fi (servidor de hora de Internet) cada vez que la placa se conecta a buscar actualizaciones.

#ifndef MFV3_S3_RELOJ_H
#define MFV3_S3_RELOJ_H

#include <stdio.h>
#include <string.h>
#include "board_config.h"

#if MFV3_BOARD == BOARD_WAVESHARE_7C
#define HAS_RTC 1
#else
#define HAS_RTC 0
#endif

namespace reloj {

struct Clock {
  uint16_t year;
  uint8_t month, day, hour, minute, second;
};

struct Battery {
  bool present;
  bool charging;
  uint8_t percent;
  uint16_t mv;
};

static const uint8_t LOW_BATTERY_PERCENT = 15;  // por debajo (y sin cargar): «BATERIA BAJA»
static const char* TZ_SPAIN = "CET-1CEST,M3.5.0,M10.5.0/3";

inline uint8_t bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }
inline uint8_t dec(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }

inline bool validClock(const Clock& c) {
  static const uint8_t DAYS[12] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (c.year < 2024 || c.year > 2099 || c.month < 1 || c.month > 12 || c.day < 1 ||
      c.hour >= 24 || c.minute >= 60 || c.second >= 60) return false;
  const bool leap = c.year % 4 == 0 && (c.year % 100 != 0 || c.year % 400 == 0);
  const uint8_t limit = c.month == 2 && !leap ? 28 : DAYS[c.month - 1];
  return c.day <= limit;
}

// «2026-10-17 18:42:05» → Clock. Devuelve false si no es una fecha válida.
inline bool parseClock(const char* s, Clock& c) {
  unsigned y, mo, d, h, mi, se;
  if (sscanf(s, "%4u-%2u-%2u %2u:%2u:%2u", &y, &mo, &d, &h, &mi, &se) != 6) return false;
  c = Clock{(uint16_t)y, (uint8_t)mo, (uint8_t)d, (uint8_t)h, (uint8_t)mi, (uint8_t)se};
  return validClock(c);
}

// Día de la semana (0 = domingo), para la pantalla de reposo.
inline uint8_t weekday(const Clock& c) {
  static const uint8_t T[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  unsigned y = c.year - (c.month < 3);
  return (uint8_t)((y + y / 4 - y / 100 + y / 400 + T[c.month - 1] + c.day) % 7);
}

// Minutos desde el 1-1-2000 (cabe en 32 bits hasta el año 10 000): para guardar cuándo fue cada partido.
inline uint32_t stamp(const Clock& c) {
  uint32_t days = 0;
  for (uint16_t y = 2000; y < c.year; y++) days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
  static const uint16_t CUM[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  days += CUM[c.month - 1] + c.day - 1;
  if (c.month > 2 && c.year % 4 == 0 && (c.year % 100 != 0 || c.year % 400 == 0)) days++;
  return (days * 24 + c.hour) * 60 + c.minute + 1;  // +1: 0 significa «sin hora»
}

inline Clock fromStamp(uint32_t s) {
  Clock c{2000, 1, 1, 0, 0, 0};
  if (s == 0) return c;
  s--;
  c.minute = (uint8_t)(s % 60);
  s /= 60;
  c.hour = (uint8_t)(s % 24);
  uint32_t days = s / 24;
  for (;;) {
    uint32_t len = (c.year % 4 == 0 && (c.year % 100 != 0 || c.year % 400 == 0)) ? 366 : 365;
    if (days < len) break;
    days -= len;
    c.year++;
  }
  static const uint8_t DAYS[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  bool leap = c.year % 4 == 0 && (c.year % 100 != 0 || c.year % 400 == 0);
  for (uint8_t m = 0; m < 12; m++) {
    uint32_t len = DAYS[m] + (m == 1 && leap ? 1 : 0);
    if (days < len) {
      c.month = (uint8_t)(m + 1);
      c.day = (uint8_t)(days + 1);
      break;
    }
    days -= len;
  }
  return c;
}

#if HAS_RTC
static const uint8_t RTC_ADDR = 0x51, GAUGE_ADDR = 0x55;

inline bool rtcRead(Clock& c) {
  uint8_t reg = 0x04, b[7] = {0};
  if (lgfx::i2c::transactionWriteRead(exio::I2C_PORT, RTC_ADDR, &reg, 1, b, 7).has_error()) return false;
  if (b[0] & 0x80) return false;  // OS: el reloj se paró, la hora no vale
  c = Clock{(uint16_t)(2000 + dec(b[6])), dec(b[5] & 0x1F), dec(b[3] & 0x3F), dec(b[2] & 0x3F), dec(b[1] & 0x7F),
            dec(b[0] & 0x7F)};
  return validClock(c);
}

inline bool rtcWrite(const Clock& c) {
  if (!validClock(c)) return false;
  uint8_t d[8] = {0x04, bcd(c.second), bcd(c.minute), bcd(c.hour), bcd(c.day), weekday(c), bcd(c.month),
                  bcd((uint8_t)(c.year - 2000))};  // escribir los segundos borra OS
  return !lgfx::i2c::transactionWrite(exio::I2C_PORT, RTC_ADDR, d, 8).has_error();
}

inline bool gaugeWord(uint8_t reg, uint16_t& v) {
  uint8_t b[2] = {0, 0};
  if (lgfx::i2c::transactionWriteRead(exio::I2C_PORT, GAUGE_ADDR, &reg, 1, b, 2).has_error()) return false;
  v = (uint16_t)(b[0] | (b[1] << 8));
  return true;
}

// Hora de Internet (con el Wi-Fi ya conectado). Espera como mucho `waitMs` a que conteste el servidor.
inline bool internetClock(Clock& c, uint32_t waitMs = 4000) {
  configTzTime(TZ_SPAIN, "pool.ntp.org", "time.google.com");
  struct tm t;
  if (!getLocalTime(&t, waitMs)) return false;
  c = Clock{(uint16_t)(t.tm_year + 1900), (uint8_t)(t.tm_mon + 1), (uint8_t)t.tm_mday, (uint8_t)t.tm_hour,
            (uint8_t)t.tm_min, (uint8_t)t.tm_sec};
  return validClock(c);
}

inline bool gaugeRead(Battery& out) {
  uint16_t status, soc, mv, ma;
  out = Battery{false, false, 0, 0};
  if (!gaugeWord(0x0A, status)) return false;  // sin medidor
  out.present = (status & 0x0008) != 0;
  if (!out.present) return true;
  if (!gaugeWord(0x2C, soc) || !gaugeWord(0x08, mv) || !gaugeWord(0x0C, ma)) return false;
  out.percent = (uint8_t)(soc > 100 ? 100 : soc);
  out.mv = mv;
  out.charging = (int16_t)ma > 20;  // un poco de margen: en reposo la corriente baila alrededor de 0
  return true;
}
#else
inline bool rtcRead(Clock&) { return false; }
inline bool rtcWrite(const Clock&) { return false; }
inline bool internetClock(Clock&, uint32_t = 0) { return false; }
inline bool gaugeRead(Battery& out) {
  out = Battery{false, false, 0, 0};
  return false;
}
#endif

// Estado que usa la pantalla: se lee del reloj una vez por segundo y de la batería cada 10 s.
struct State {
  bool clockOk = false;  // hay hora válida
  Clock now{2000, 1, 1, 0, 0, 0};
  Battery bat{false, false, 0, 0};
  uint32_t clockAt = 0, batAt = 0;
  uint32_t timeAt = 0;  // referencia monotónica de la hora conocida; tolera el wrap de millis()
  bool rtcOk = false;   // última lectura/escritura física correcta; no equivale a clockOk
  bool softwareTime = false;  // una puesta en hora no guardada no debe volver a una fecha vieja del RTC
  bool first = true;

  void advance(uint32_t ms) {
    if (!clockOk || (int32_t)(ms - timeAt) < 0) return;
    uint32_t seconds = (uint32_t)(ms - timeAt) / 1000u;
    if (!seconds) return;
    uint32_t total = (uint32_t)now.second + seconds;
    now = fromStamp(stamp(now) + total / 60u);
    now.second = (uint8_t)(total % 60u);
    timeAt += seconds * 1000u;
    clockOk = validClock(now);
  }

  void poll(uint32_t ms) {
    if (first || ms - clockAt >= 1000) {
      clockAt = ms;
      Clock c;
      rtcOk = !softwareTime && rtcRead(c);
      if (rtcOk) {
        now = c;
        clockOk = true;
        timeAt = ms;
      } else {
        // Un fallo I2C no borra una hora válida: sigue avanzando durante este encendido.
        advance(ms);
      }
    }
    if (first || ms - batAt >= 10000) {
      batAt = ms;
      gaugeRead(bat);
    }
    first = false;
  }

  bool set(const Clock& c) {
    if (!validClock(c)) return false;
    rtcOk = rtcWrite(c);
    softwareTime = !rtcOk;
    now = c;
    clockOk = true;
    timeAt = millis();
    // La hora obtenida por USB/Internet también es válida en RAM si el RTC no responde.
    // Eso no promete conservarla al apagar: rtcOk permite distinguirlo en el diagnóstico.
    return true;
  }

  // Con el Wi-Fi conectado: pone el reloj con la hora de Internet.
  bool syncInternet() {
    Clock c;
    return internetClock(c) && set(c);
  }

  bool lowBattery() const { return bat.present && !bat.charging && bat.percent <= LOW_BATTERY_PERCENT; }
};

}  // namespace reloj

#endif  // MFV3_S3_RELOJ_H
