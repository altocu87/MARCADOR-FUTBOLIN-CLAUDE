// AJUSTES > ACTUALIZACIONES. Se incluye en s3_ajustes.h después de sus datos y helpers.
#ifndef MFV3_S3_UPDATE_UI_H
#define MFV3_S3_UPDATE_UI_H
enum class UpdateTab : uint8_t { Overview, History, Guide, Confirm };
static UpdateTab updateTab = UpdateTab::Overview;
static size_t updateHistoryPage = 0;
static const ui::Rect UPDATE_TABS[] = {{24, 76, 244, 52}, {278, 76, 244, 52}, {532, 76, 244, 52}};
static const ui::Rect HISTORY_PREV = {24, 396, 230, 68}, HISTORY_NEXT = {546, 396, 230, 68};
#if defined(MFV3_FASE3) && defined(ESP_PLATFORM)
// Prueba de la fase 3: volver a la versión anterior (el otro hueco). El primer toque pide confirmación.
static const ui::Rect FASE3_BACK = {500, 140, 276, 64};
static uint32_t fase3ArmedUntil = 0;
static bool fase3Armed() { return fase3ArmedUntil && (int32_t)(fase3ArmedUntil - millis()) > 0; }
#endif
static lv_obj_t* updateBar = nullptr;
static lv_obj_t* updatePercent = nullptr;
static int updateDrawnPercent = -1;
static char updatePercentText[40];

static lv_obj_t* updateText(lv_obj_t* parent, const char* s, int x, int y, int w, int size = 20,
                          uint32_t color = ui::color::TEXT2) {
  // Reutiliza fuentes que ya van en FFat: no añade recursos ni cae a la letra pequeña por defecto.
  const int actual = size >= 28 ? 30 : size >= 24 ? 24 : size >= 20 ? 22 : 16;
  lv_obj_t* l = ui::label(parent, s, actual >= 24 ? 900 : 400, actual, color);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_width(l, w);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  return l;
}
static void updateButton(const ui::Rect& r, const char* title, uint16_t fill, uint16_t ink, uint16_t border,
                         const lgfx::IFont* = nullptr) {
  auto col = [](uint16_t c) { return lv_color_make(((c >> 11) & 31) * 255 / 31, ((c >> 5) & 63) * 255 / 63, (c & 31) * 255 / 31); };
  lv_obj_t* b = ui::button(gfx.scr, r.x, r.y, r.w, r.h, title, 20, ui::BtnKind::Normal,
    [](lv_event_t* e) {
      lv_area_t a; lv_obj_get_coords(lv_event_get_target_obj(e), &a);
      if (gfx.tapCb) gfx.tapCb((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2);
    });
  lv_obj_set_style_bg_color(b, col(fill), 0);
  lv_obj_set_style_border_color(b, col(border), 0);
  lv_obj_set_style_text_color(lv_obj_get_child(b, 0), col(ink), 0);
}
static bool canInstallUpdate() {
  return !inMatch && checkState == CheckState::Ok && updateAvailable() && wifiSsid[0] &&
         !versionsRunning && !nube::busy() && !s3net::scanning;
}
static size_t pendingHistoryEntries() { return (screenOutdated() ? 1 : 0) + (mandoUpd() == MandoUpd::Outdated ? 1 : 0); }
static size_t updateHistoryCount() { return nativeRelease::COUNT + pendingHistoryEntries(); }

static void drawUpdate() {
  gfx.fillScreen(C_BG);
  updateText(gfx.scr, "ACTUALIZACIONES", 24, 18, 560, 30, ui::color::ACCENT);
  updateButton(ui::BACK, "VOLVER", C_CARD, C_TEXT, C_LINE);
  const char* tabs[] = {"VERSIONES", "NOVEDADES", "CÓMO ACTUALIZAR"};
  for (int i = 0; i < 3; ++i) {
    bool selected = (int)updateTab == i;
    updateButton(UPDATE_TABS[i], tabs[i], selected ? C_CARD : C_SURFACE, selected ? C_ACCENT : C_TEXT2,
           selected ? C_ACCENT : C_LINE, &lgfx::fonts::FreeSansBold12pt7b);
  }
  char t[120];
  if (updateTab == UpdateTab::History) {
    size_t total = updateHistoryCount();
    if (updateHistoryPage >= total) updateHistoryPage = total - 1;
    size_t pending = pendingHistoryEntries();
    const char* notes;
    if (updateHistoryPage < pending) {
      bool isScreen = screenOutdated() && updateHistoryPage == 0;
      if (isScreen) {
        snprintf(t, sizeof(t), "PANTALLA · %s · disponible", publishedRelease.version[0] ? publishedRelease.version : "Sin número");
        notes = publishedRelease.notes[0] ? publishedRelease.notes : "Esta publicación antigua no incluye sus novedades. No se han inventado cambios. Puedes volver a buscar más adelante.";
      } else {
        snprintf(t, sizeof(t), "MANDO · actualización disponible");
        notes = "Esta publicación del mando no incluye notas de cambios. El registro de versiones 0.x corresponde a la pantalla.";
      }
    } else {
      const nativeRelease::Entry& r = nativeRelease::HISTORY[updateHistoryPage - pending];
      snprintf(t, sizeof(t), "PANTALLA · %s · %s%s", r.version, r.date,
               !strcmp(r.version, FW_VERSION) ? " · instalada" : "");
      notes = r.notes;
    }
    updateText(gfx.scr, t, 30, 143, 740, 22, ui::color::ACCENT);
    lv_obj_t* scroll = ui::box(gfx.scr, 30, 182, 740, 174);
    lv_obj_add_flag(scroll, (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_set_style_bg_color(scroll, ui::hex(ui::color::ACCENT), LV_PART_SCROLLBAR);
    lv_obj_set_style_bg_opa(scroll, LV_OPA_COVER, LV_PART_SCROLLBAR);
    lv_obj_set_style_width(scroll, 4, LV_PART_SCROLLBAR);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_AUTO);
    updateText(scroll, notes, 0, 0, 716);
    snprintf(t, sizeof(t), "%u / %u · Desliza para leer", (unsigned)(updateHistoryPage + 1), (unsigned)total);
    updateText(gfx.scr, t, 236, 366, 400, 18);
    updateButton(HISTORY_PREV, "ANTERIOR", C_CARD, updateHistoryPage ? C_TEXT : C_LINE, C_LINE);
    updateButton(HISTORY_NEXT, "SIGUIENTE", C_CARD, updateHistoryPage + 1 < total ? C_TEXT : C_LINE, C_LINE);
    return;
  }
  if (updateTab == UpdateTab::Guide || updateTab == UpdateTab::Confirm) {
    updateText(gfx.scr, updateTab == UpdateTab::Confirm ? "Antes de empezar" : "Así se actualiza tu futbolín", 36, 148, 720, 28, ui::color::TEXT);
    updateText(gfx.scr,
      "1. Conecta la pantalla a la corriente y al Wi-Fi.\n\n"
      "2. Revisa las novedades y confirma la instalación.\n\n"
      "3. Sigue los pasos y espera al reinicio.", 36, 190, 720);
    updateText(gfx.scr, mandoUpd() == MandoUpd::Outdated ?
      "Si se actualiza el mando, déjalo cerca y sigue la indicación de sus dos botones. No apagues los equipos." :
      "No apagues la pantalla durante la instalación. El programa se comprueba antes de reiniciar.", 36, 350, 720, 18, ui::color::ACCENT);
  } else {
#if defined(MFV3_FASE3) && defined(ESP_PLATFORM)
    snprintf(t, sizeof(t), "PANTALLA · %s · %s", FW_VERSION, fase3::LABEL);
    updateText(gfx.scr, t, 36, 150, 450, 28, ui::color::TEXT);
    if (fase3::previous())
      updateButton(FASE3_BACK, fase3Armed() ? "¿SEGURO? TOCA OTRA VEZ" : "VOLVER A LA ANTERIOR", fase3Armed() ? C_ACCENT : C_SURFACE,
                   fase3Armed() ? C_BG : C_TEXT, fase3Armed() ? C_ACCENT : C_LINE);
#else
    snprintf(t, sizeof(t), "PANTALLA · Instalada %s", FW_VERSION);
    updateText(gfx.scr, t, 36, 150, 720, 28, ui::color::TEXT);
#endif
    if (screenOutdated()) snprintf(t, sizeof(t), "Disponible: %s · Consulta NOVEDADES", publishedRelease.version[0] ? publishedRelease.version : "nueva actualización");
    else snprintf(t, sizeof(t), "%s", pubScreen.ok() && checkState == CheckState::Ok ? "Tienes la última versión publicada" : "Pulsa BUSCAR para comprobar si hay novedades");
#if defined(MFV3_FASE3) && defined(ESP_PLATFORM)
    updateText(gfx.scr, t, 36, 210, 720, 20, screenOutdated() ? ui::color::RANKED : ui::color::TEXT2);
#else
    updateText(gfx.scr, t, 36, 192, 720, 20, screenOutdated() ? ui::color::RANKED : ui::color::TEXT2);
#endif
    updateText(gfx.scr, "MANDO", 36, 248, 200, 24, ui::color::TEXT);
    const char* status = mandoUpd() == MandoUpd::Unknown ? "Pulsa un botón del mando para comprobarlo" :
      mandoUpd() == MandoUpd::NeedsCable ? "Necesita una primera actualización por cable" :
      mandoUpd() == MandoUpd::NeedsModel ? "Elige su modelo en Ajustes > Mando para poder actualizarlo" :
      mandoUpd() == MandoUpd::Outdated ? "Hay una actualización disponible" :
      pubMando.ok() ? "Al día" : "Todavía no se ha podido consultar";
    updateText(gfx.scr, status, 36, 284, 720);
    const char* msg = !wifiSsid[0] ? "Configura primero el Wi-Fi en Ajustes" :
      checkState == CheckState::Running ? "Buscando novedades. Puedes seguir usando los menús." :
      checkState == CheckState::NoConnect ? "No conecta al Wi-Fi. Revisa la red y vuelve a buscar." :
      checkState == CheckState::Failed ? "No se pudo consultar. Vuelve a intentarlo más tarde." :
      nube::busy() ? "Espera a que termine la sincronización." : "Tú decides cuándo instalar. Nunca durante un partido.";
    updateText(gfx.scr, msg, 36, 340, 720, 18);
  }
  bool can = canInstallUpdate();
  updateButton(ui::UPDATE[1].r, updateTab == UpdateTab::Confirm ? "AHORA NO" : "BUSCAR", C_SURFACE, C_TEXT, C_LINE);
  updateButton(ui::UPDATE[2].r, updateTab == UpdateTab::Confirm ? "INSTALAR AHORA" : "CONTINUAR",
         can ? C_ACCENT : C_SURFACE, can ? C_BG : C_LINE, can ? C_ACCENT : C_LINE);
}

static bool updateNavigation(int16_t x, int16_t y) {
#if defined(MFV3_FASE3) && defined(ESP_PLATFORM)
  if (updateTab == UpdateTab::Overview && FASE3_BACK.contains(x, y) && fase3::previous()) {
    if (!fase3Armed()) fase3ArmedUntil = millis() + 4000;
    else if (!fase3::rollback()) ui::toast("No hay versión anterior");
    return true;
  }
#endif
  for (int i = 0; i < 3; ++i) if (UPDATE_TABS[i].contains(x, y)) {
    updateTab = (UpdateTab)i;
    return true;
  }
  if (updateTab == UpdateTab::Confirm && ui::UPDATE[1].r.contains(x, y)) {
    updateTab = UpdateTab::Overview;
    return true;
  }
  if (updateTab == UpdateTab::History) {
    if (HISTORY_PREV.contains(x, y) && updateHistoryPage) --updateHistoryPage;
    else if (HISTORY_NEXT.contains(x, y) && updateHistoryPage + 1 < updateHistoryCount()) ++updateHistoryPage;
    return !ui::BACK.contains(x, y);  // las flechas no buscan ni instalan
  }
  return false;
}

static void drawProgressBar(uint8_t pct) {
  if (pct > 100) pct = 100;
  if (updateDrawnPercent == pct) return;
  if (!updateBar || !gfx.active() || !lv_obj_is_valid(updateBar)) return;
  updateDrawnPercent = pct;
  lv_bar_set_value(updateBar, pct, LV_ANIM_OFF);
  snprintf(updatePercentText, sizeof(updatePercentText), "%u %% de este paso", (unsigned)pct);
  lv_label_set_text_static(updatePercent, updatePercentText);
}

static void drawInstall() {
  gfx.fillScreen(C_BG);
  updateBar = updatePercent = nullptr;
  updateDrawnPercent = -1;
  const bool mando = inst <= Inst::MandoConfirm || (inst == Inst::Error && instErrorMando) ||
                     (inst == Inst::Done && instMando && !instScreen) || inst == Inst::Cancelled;
  updateText(gfx.scr, "ACTUALIZANDO", 36, 28, 720, 30, ui::color::ACCENT);
  char stage[100];
  snprintf(stage, sizeof(stage), "%s%s", mando ? "MANDO" : "PANTALLA",
           instMando && instScreen ? (mando ? " · primero el mando, después la pantalla" : " · último equipo") : "");
  updateText(gfx.scr, stage, 36, 80, 720, 20);
  const char* title = "", *hint = "";
  bool bar = false;
  switch (inst) {
    case Inst::MandoConnect: case Inst::ScreenConnect:
      title = "Conectando al Wi-Fi"; hint = "Preparando la descarga. Puede tardar unos segundos."; break;
    case Inst::MandoDownload:
      title = "Descargando el programa del mando"; hint = "Al terminar comprobaremos que ha llegado completo."; bar = true; break;
    case Inst::MandoRadio:
      if (sender.state() == mfv3::ota::Sender::State::Offering) {
        title = sender.waitingButtons() ? "Mantén pulsados los dos botones" : "Buscando el mando";
        hint = sender.waitingButtons() ? "del mando durante 3 segundos para autorizar el cambio." : "Pulsa un botón del mando para despertarlo y déjalo cerca.";
      } else { title = "Enviando al mando"; hint = "Cada parte se confirma antes de continuar."; bar = true; }
      break;
    case Inst::MandoConfirm:
      title = "Comprobando el mando"; hint = "Esperando a que se reinicie y confirme la nueva versión."; break;
    case Inst::ScreenDownload:
      title = "Descargando y comprobando la pantalla"; hint = "El porcentaje corresponde a la descarga de este equipo."; bar = true; break;
    case Inst::Restart:
      title = "Listo. Reiniciando la pantalla"; hint = "La descarga está comprobada. Volverás al inicio en unos segundos."; bar = true; break;
    case Inst::Done:
      title = "Actualización terminada"; hint = "Ya puedes volver al marcador."; bar = true; break;
    case Inst::Cancelled:
      title = "Actualización cancelada"; hint = "Puedes volver a intentarlo cuando quieras."; break;
    case Inst::Error:
      title = "La actualización no ha terminado"; hint = instError; break;
  }
  updateText(gfx.scr, title, 36, 136, 728, 28, inst == Inst::Error ? ui::color::DANGER : ui::color::TEXT);
  updateText(gfx.scr, hint, 36, 208, 728);
  if (bar) {
    updateBar = lv_bar_create(gfx.scr);
    lv_obj_set_pos(updateBar, 36, 282); lv_obj_set_size(updateBar, 728, 32);
    lv_bar_set_range(updateBar, 0, 100);
    lv_obj_set_style_bg_color(updateBar, ui::hex(ui::color::LINE), LV_PART_MAIN);
    lv_obj_set_style_bg_color(updateBar, ui::hex(ui::color::ACCENT), LV_PART_INDICATOR);
    updatePercent = updateText(gfx.scr, "", 36, 326, 728, 24, ui::color::ACCENT);
    drawProgressBar(inst == Inst::Restart || inst == Inst::Done ? 100 : instPct);
  }
  if (inst == Inst::Error) {
    updateText(gfx.scr, "Vuelve a buscar antes de reintentar. No se borran tus partidos para actualizar.", 36, 292, 728);
  } else if (inst != Inst::Done && inst != Inst::Cancelled) updateText(gfx.scr, "Mantén los equipos encendidos hasta terminar.", 36, 366, 728, 18);
  if (inst == Inst::MandoRadio && sender.state() == mfv3::ota::Sender::State::Offering)
    updateButton(ui::INSTALL[0].r, instScreen ? "SALTAR EL MANDO" : "CANCELAR", C_SURFACE, C_TEXT, C_LINE);
  if (inst == Inst::Done || inst == Inst::Error || inst == Inst::Cancelled) updateButton(ui::INSTALL[1].r, "VOLVER", C_ACCENT, C_BG, C_ACCENT);
  instStepDrawn = true;
}
#endif
