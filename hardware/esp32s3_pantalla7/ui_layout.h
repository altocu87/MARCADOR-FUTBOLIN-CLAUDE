// MARCADOR FUTBOLÍN V3 · distribución de botones de la pantalla de 7" (800×480).
// Solo geometría y detección de toques: no depende de la pantalla, se prueba en el PC.

#ifndef MFV3_UI_LAYOUT_H
#define MFV3_UI_LAYOUT_H

#include <stdint.h>

namespace ui {

const int16_t W = 800;
const int16_t H = 480;

enum class Btn : uint8_t {
  None,
  // Inicio / configuración
  CondGoals, CondTime, CondBoth, GoalsMinus, GoalsPlus, MinutesMinus, MinutesPlus, Play,
  // Partido
  ScoreWhite, ScoreBlue, MinusWhite, MinusBlue, Undo, Pause,
  // Superposiciones
  Resume, Abandon, Next, SkipCountdown,
  // Penaltis
  PenWhiteGoal, PenWhiteMiss, PenBlueGoal, PenBlueMiss, PenUndo,
  // Final
  NewMatch, Rematch,
  // Pantalla de prueba
  Test, TestDone,
  // Inicio: ajustes y aviso de versión nueva
  Settings, UpdateAvail,
  // Ajustes
  Back, SetWifi, SetUpdate, VolMinus, VolPlus, SetTest, SetPlayers,
  // Wi-Fi: redes (WifiNet0 + n), buscar otra vez, olvidar
  WifiNet0, WifiNet1, WifiNet2, WifiNet3, WifiNet4, WifiRescan, WifiForget,
  // Teclado
  KeyShift, KeyMode, KeySpace, KeyBack, KeyConnect, KeyShow,
  // Actualizar
  UpdSearch, UpdInstall, UpdSkipMando,
};

struct Rect {
  int16_t x, y, w, h;
  bool contains(int16_t px, int16_t py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

struct Button {
  Btn id;
  Rect r;
};

// ---- Inicio
const Button HOME[] = {
    {Btn::CondGoals, {40, 150, 230, 64}},  {Btn::CondTime, {285, 150, 230, 64}},  {Btn::CondBoth, {530, 150, 230, 64}},
    {Btn::GoalsMinus, {40, 270, 72, 72}},   {Btn::GoalsPlus, {290, 270, 72, 72}},
    {Btn::MinutesMinus, {438, 270, 72, 72}}, {Btn::MinutesPlus, {688, 270, 72, 72}},
    {Btn::Play, {240, 380, 320, 80}},
    {Btn::Settings, {20, 396, 170, 64}},
    {Btn::UpdateAvail, {610, 396, 170, 64}},  // solo si hay versión nueva
};

// ---- Ajustes (VOLVER arriba a la derecha en todas las pantallas de ajustes)
const Rect BACK = {610, 8, 170, 56};
const Button SETTINGS[] = {
    {Btn::Back, BACK},
    {Btn::SetWifi, {40, 80, 720, 72}},
    {Btn::SetUpdate, {40, 164, 720, 72}},
    {Btn::VolMinus, {460, 248, 110, 72}}, {Btn::VolPlus, {650, 248, 110, 72}},
    {Btn::SetTest, {40, 332, 720, 72}},
    {Btn::SetPlayers, {40, 416, 720, 56}},
};

// ---- Wi-Fi: hasta 5 redes
const uint8_t WIFI_ROWS = 5;
const Button WIFI[] = {
    {Btn::Back, BACK},
    {Btn::WifiNet0, {40, 76, 720, 58}},  {Btn::WifiNet1, {40, 142, 720, 58}}, {Btn::WifiNet2, {40, 208, 720, 58}},
    {Btn::WifiNet3, {40, 274, 720, 58}}, {Btn::WifiNet4, {40, 340, 720, 58}},
    {Btn::WifiRescan, {40, 410, 340, 62}}, {Btn::WifiForget, {420, 410, 340, 62}},
};

// ---- Teclado (la clave del Wi-Fi). Las letras se calculan con keyAt(); aquí, las teclas especiales.
const Rect KEY_FIELD = {40, 66, 580, 56};
const int16_t KEY_TOP = 132, KEY_ROW_H = 64, KEY_W = 68, KEY_STEP = 72;
const Button KEYBOARD[] = {
    {Btn::Back, BACK},
    {Btn::KeyShow, {630, 66, 130, 56}},
    {Btn::KeyBack, {616, KEY_TOP + 3 * KEY_ROW_H + 8, 144, 56}},
    {Btn::KeyShift, {40, 420, 130, 56}},  {Btn::KeyMode, {176, 420, 130, 56}},
    {Btn::KeySpace, {312, 420, 250, 56}}, {Btn::KeyConnect, {568, 420, 192, 56}},
};

// ---- Actualizar
const Button UPDATE[] = {
    {Btn::Back, BACK},
    {Btn::UpdSearch, {40, 396, 340, 72}}, {Btn::UpdInstall, {420, 396, 340, 72}},
};
const Button INSTALL[] = {{Btn::UpdSkipMando, {250, 396, 300, 70}}, {Btn::Back, {250, 396, 300, 70}}};

// ---- Pantalla de prueba (primer arranque): 4 esquinas para comprobar el táctil y botón TERMINAR
const Rect TEST_CORNERS[4] = {{0, 0, 90, 90}, {710, 0, 90, 90}, {0, 390, 90, 90}, {710, 390, 90, 90}};
const Button TEST[] = {{Btn::TestDone, {290, 396, 220, 70}}};

// ---- Partido (paneles de marcador = botones de gol)
const Rect PANEL_WHITE = {12, 52, 296, 332};
const Rect PANEL_BLUE = {492, 52, 296, 332};
const Rect CLOCK = {314, 60, 172, 70};
const Rect LOCK_MSG = {314, 140, 172, 30};
const Button MATCH[] = {
    {Btn::ScoreWhite, PANEL_WHITE}, {Btn::ScoreBlue, PANEL_BLUE},
    {Btn::MinusWhite, {12, 396, 90, 72}}, {Btn::MinusBlue, {698, 396, 90, 72}},
    {Btn::Undo, {320, 316, 160, 64}},     {Btn::Pause, {320, 396, 160, 72}},
};

// ---- Pausa / final de periodo / cuenta atrás
const Button PAUSED[] = {{Btn::Resume, {250, 220, 300, 84}}, {Btn::Abandon, {300, 330, 200, 60}}};
const Button PERIOD_END[] = {{Btn::Next, {230, 330, 340, 84}}};
const Button COUNTDOWN[] = {{Btn::SkipCountdown, {0, 0, W, H}}};

// ---- Penaltis
const Button PENALTIES[] = {
    {Btn::PenWhiteGoal, {20, 370, 140, 90}},  {Btn::PenWhiteMiss, {170, 370, 140, 90}},
    {Btn::PenBlueGoal, {490, 370, 140, 90}},  {Btn::PenBlueMiss, {640, 370, 140, 90}},
    {Btn::PenUndo, {330, 390, 140, 70}},
};

// ---- Final
const Button FINISHED[] = {{Btn::NewMatch, {150, 370, 240, 80}}, {Btn::Rematch, {410, 370, 240, 80}}};

// ---- Letras del teclado: 4 filas (10, 10, 9 y 7 teclas), la 3ª y 4ª centradas.
const char* const KEYS_LOWER[4] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};
const char* const KEYS_UPPER[4] = {"1234567890", "QWERTYUIOP", "ASDFGHJKL", "ZXCVBNM"};
const char* const KEYS_SYMBOLS[4] = {"1234567890", "!@#$%&*()-", "_=+.,;:/?", "'\"<>[]~"};

inline Rect keyRect(uint8_t row, uint8_t col, uint8_t rowLen) {
  int16_t x0 = 40 + (10 - rowLen) * KEY_STEP / 2;
  if (row == 3) x0 = 40 + KEY_STEP / 2;  // la última fila deja sitio al borrar
  return Rect{(int16_t)(x0 + col * KEY_STEP), (int16_t)(KEY_TOP + row * KEY_ROW_H + 8), KEY_W, 56};
}

// Letra tocada (0 = ninguna). mode: 0 minúsculas, 1 mayúsculas, 2 símbolos.
inline char keyAt(uint8_t mode, int16_t x, int16_t y) {
  const char* const* rows = mode == 2 ? KEYS_SYMBOLS : mode == 1 ? KEYS_UPPER : KEYS_LOWER;
  for (uint8_t r = 0; r < 4; r++) {
    uint8_t n = 0;
    while (rows[r][n]) n++;
    for (uint8_t c = 0; c < n; c++)
      if (keyRect(r, c, n).contains(x, y)) return rows[r][c];
  }
  return 0;
}

template <unsigned N>
Btn hit(const Button (&list)[N], int16_t x, int16_t y) {
  for (unsigned i = 0; i < N; i++)
    if (list[i].r.contains(x, y)) return list[i].id;
  return Btn::None;
}

template <unsigned N>
const Rect* rectOf(const Button (&list)[N], Btn id) {
  for (unsigned i = 0; i < N; i++)
    if (list[i].id == id) return &list[i].r;
  return nullptr;
}

}  // namespace ui

#endif  // MFV3_UI_LAYOUT_H
