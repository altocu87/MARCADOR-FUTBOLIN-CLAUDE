# Torneos en la pantalla · 0.10 · 23/10/2026

Equipo A (Cloud), por encargo del dueño: llevar los torneos de la app web a la placa, lo más parecidos posible y con la ligereza de los menús. Trabajo repartido entre tres agentes (reglas, datos, pantallas) y unido por el coordinador. Plan y contrato: [PLAN_TORNEOS_PLACA.md](PLAN_TORNEOS_PLACA.md). Rama `claude/torneos-placa`. **Sin fusionar en main, sin publicar y sin grabar en la placa**: falta la autorización del dueño y la prueba con el dedo.

## Qué cambia para quien juega

- **TORNEO en Inicio** (antes, «Los torneos llegan más adelante»):
  1. «¿Quién juega?», con fotos y los torneos en juego para continuarlos o descartarlos.
  2. «¿Qué torneo jugamos?»: LIGA, COPA y POOL en su neón, con dos toques.
  3. «Elige la liga», con edición y reglas.
  4. Las parejas: sortear, formarlas a mano o ponerles nombre.
- **Creador completo** en la pantalla. Pasos: Tipo, Jugadores, Duración, Partidos, Puntos, Final y Nombre. Se puede crear, editar, duplicar y borrar.
- **Pantalla del torneo**:
  - La liga con su clasificación (flechas ▲▼), la copa con su cuadro y el pool con sus pistas (corona, banquillo, subidas y bajadas, reloj).
  - El siguiente partido, con fotos y pronóstico.
  - REGLAS, REPETIR ÚLTIMO PARTIDO y descartar.
- **En el partido**:
  - Cartel de fase (GRAN FINAL, SEMIFINAL, PISTA 2 · RONDA 3, ¡PARTIDO DECISIVO!).
  - Empate en la liga a tiempo.
  - Al acabar, VOLVER AL TORNEO o VER CAMPEÓN.
  - Si se abandona, el partido queda «a medias».
- **Ceremonia**: ¡CAMPEÓN! y después cada puesto con su XP, nivel, ELO y logros. El último se lleva su título.
- **Ficha y palmarés**: MVP, pichichi y destacados. Cada competición tiene sus ediciones y su cuadro de honor.
- **Experiencia como en la app**: los partidos de un torneo en juego mueven el ELO, pero la XP y los logros llegan al terminarlo, más 300 XP por ganarlo.

## Historial casi ilimitado (petición del dueño)

- En la SD, todo: cada torneo en `/sdcard/torneos/<id>.json` y un índice con una línea por torneo.
- En memoria, solo los torneos en juego y las 10 últimas ediciones de cada competición. Las anteriores se leen de la SD de 10 en 10 (VER MÁS o llegar al final de la lista).
- El cuadro de honor recorre el índice entero, sin límite de jugadores.
- La nube sirve de respaldo. Cada torneo es una fila `tournament` y los tipos van en `prefs.tournamentTemplates`, unidos con los de la cuenta sin pisar el resto de preferencias. Una SD vacía se rehace entera desde la cuenta.
- La prueba con 500 torneos usa la misma memoria que con 100. Un corte de luz a mitad de escritura se recupera.

## Diferencias con la app (por fluidez o por no llevar imágenes)

- **Dibujos**: los escudos, las copas y los banners son insignias de color con iniciales. La placa guarda el banner como `color:<n>`, así que la app no lo dibuja (enseña su banner por defecto).
- **Animaciones**:
  - Las tarjetas se encienden de golpe, sin fundido.
  - En el pool, la nueva ronda aparece de golpe, sin la animación.
  - No hay rayos que giran.
  - El confeti es el de la victoria: 16 trocitos.
- **Pronóstico**: solo con el ELO medio de cada lado. La app suma además enfrentamientos y forma.
- **Ceremonia**: la XP sale sin desglose por concepto.
- **Lista «en juego»**: no dice «siguiente: X vs Y», para no abrir cada torneo.
- **Ficha**: los destacados no abren el partido.
- **Partidos a medias**: no hay «CONTINUAR un partido a medias», porque la placa no guarda los partidos en curso. Sí queda marcado como «a medias».
- **Sonidos**: hasta tener los sonidos propios de «ganar un torneo» y la sirena, se usan los de victoria y gol anulado.
- **Voces de torneo**: las 25 de la SD siguen reservadas, sin activar.

## Datos del dueño

- **Primer arranque**: el índice de partidos gana cuatro campos. El antiguo se lee igual y cada partido se vuelve a leer de su JSON una vez. Los partidos no se tocan.
- **Primera sincronización**: baja todos los torneos de la cuenta, en páginas de 4.
- **Volver a 0.9**: tolera el índice nuevo.

## Pruebas

- `bash hardware/test/check_copies.sh`: todo correcto. Incluye:
  - `engine_test`, con el empate.
  - `torneos_test`: la placa da lo mismo que la app en 9 torneos de prueba y en el palmarés.
  - `torneos_datos_test`: JSON, nube, 500 torneos y cortes de luz.
  - `progresion_test`, con los torneos.
  - `s3_torneos_test`: las pantallas con LVGL real y toques; nada se sale de 800 × 480 y solo usa letras de `fuentes.txt`.
  - Las tres placas.
- `check_copies.sh` genera ahora las letras de `fuentes.txt` en `/tmp` antes de la prueba de pantallas, con `placa_recursos.py --fuentes`. Así funciona también en GitHub.
- `npx vitest run`: 327 pruebas. Son nuevas `torneosPlaca`, `torneosDatosPlaca` y las ampliaciones de `boardMatches`, `progresionPlaca` y `perfil-placa`.
- `npx tsc --noEmit`, sin errores. `native-release.mjs check`: «0.10 correcto».
- GitHub (rama, commit `36b65c2`): Pruebas y Firmware correctos en todas las placas. Para que la Elecrow de 4 MB siga cabiendo (se pasaba por 33 KB), `lv_conf.h` desactiva las piezas de LVGL que el programa no usa (calendario, gráficas, menús, ventanas, interruptores, tablas, pestañas…); no cambia nada de lo que se ve.
- Capturas del PC en `docs/capturas-placa/torneos/` (42). Son de la placa simulada en el PC, no fotografías de la placa.

## Pendiente

- Prueba física con el dedo: fluidez de cada pantalla, velocidad de la SD con muchos torneos y nube real.
- Autorización del dueño para fusionar en main (eso publica 0.10 por Wi-Fi).
- Activar las 25 voces de torneo cuando lo demás esté probado.
- Sonidos propios de «ganar torneo» y sirena en `mfv3_sound.h`.
- Previo a este trabajo: borrar un partido desde HISTORIAL no se avisa a la nube. REPETIR ÚLTIMO PARTIDO del torneo sí lo hace.
