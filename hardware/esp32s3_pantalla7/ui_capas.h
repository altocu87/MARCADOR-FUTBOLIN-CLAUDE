// Generado por scripts/placa_recursos.py a partir de las capas de la app (scripts/placa-capas.mjs).
// No editar a mano: posición y tamaño de cada capa en la pantalla de 800×480.
#ifndef MFV3_UI_CAPAS_H
#define MFV3_UI_CAPAS_H
#include <stdint.h>
namespace capas {
struct Capa {
  const char* archivo;
  int16_t x, y, w, h;
};
const Capa INICIO = {"capas/inicio.jpg", 0, 0, 800, 480};
const Capa INICIO_SEL_RAPIDO = {"capas/inicio-sel-rapido.jpg", 0, 36, 302, 444};
const Capa INICIO_SEL_LOCO = {"capas/inicio-sel-loco.jpg", 226, 40, 333, 440};
const Capa INICIO_SEL_CLASIFICATORIO = {"capas/inicio-sel-clasificatorio.jpg", 478, 32, 322, 448};
const Capa INICIO_PUL_RAPIDO = {"capas/inicio-pul-rapido.jpg", 0, 45, 295, 435};
const Capa INICIO_PUL_LOCO = {"capas/inicio-pul-loco.jpg", 230, 45, 322, 435};
const Capa INICIO_PUL_CLASIFICATORIO = {"capas/inicio-pul-clasificatorio.jpg", 486, 43, 314, 437};
const Capa INICIO_PUL_TORNEO = {"capas/inicio-pul-torneo.jpg", 200, 0, 212, 111};
const Capa INICIO_PUL_RANKING = {"capas/inicio-pul-ranking.jpg", 363, 0, 211, 109};
const Capa INICIO_PUL_RETOS = {"capas/inicio-pul-retos.jpg", 528, 0, 208, 95};
const Capa INICIO_AYUDA_LOCO = {"capas/inicio-ayuda-loco.jpg", 0, 0, 800, 480};
const Capa INICIO_AYUDA_CLASIFICATORIO = {"capas/inicio-ayuda-clasificatorio.jpg", 0, 0, 800, 480};
}  // namespace capas
#endif
