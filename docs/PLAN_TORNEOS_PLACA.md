# Plan · torneos en la pantalla de 7" · 23/10/2026

Encargo del dueño: llevar los torneos de la app web a la placa, lo más parecidos posible, con las técnicas de ligereza de los menús de la placa. Rama `claude/torneos-placa`. **No se fusiona, publica ni graba sin autorización del dueño.** Versión de pantalla prevista: la siguiente 0.x (no 1.0).

Decisiones del dueño (23/10/2026):
- **Imágenes dibujadas por la placa**: colores neón de la app, escudos y copas como insignias de color (como los equipos fijos). Sin imágenes ni letras nuevas: se instala por Wi-Fi desde Actualizaciones.
- **Creador completo en la placa**: crear, editar, duplicar y borrar ligas, copas y pools sin la app.

## Qué se lleva (referencia en la app)

| Parte | App | Placa |
|---|---|---|
| Reglas: liga, copa, pool, cruces, series, final, deshacer, cambiar reglas, abandono | `services/tournaments/tournaments.ts`, `ladder.ts` | `s3_torneos_reglas.h` |
| Tipos de torneo, encaje con N jugadores, borrador y parejas | `templates.ts` | `s3_torneos_reglas.h` |
| Ficha, MVP, pichichi, ediciones, palmarés, torneos de un jugador | `report.ts` | `s3_torneos_ficha.h` |
| Guardado, índice, plantillas, nube, modo prueba, copias | `persistence`, `cloud/sync.ts` | `s3_torneos_datos.h`, `s3_nube.h`, `s3_datos.h` |
| XP diferida y premio de torneo, logros de campeón | `progression.ts` | `s3_progresion.h` |
| Empate en liga a tiempo | `match-engine` `allowDraw` | `common/mfv3_engine.h` |
| Partido de torneo guardado con `tournament: {id, fixtureId}` | `StoredMatch` | `s3_partidos.h` |
| ¿Quién juega? · ¿Qué torneo? · Elige la liga · Parejas | `TournamentNewScreen` | `ui_torneo_nuevo.h` |
| Creador por pasos (Tipo, Jugadores, Duración, Partidos, Puntos, Final, Nombre) | `TournamentTemplateScreen` | `ui_torneo_creador.h` |
| Torneo: clasificación / cuadro / pistas, siguiente partido, rondas, reglas, repetir, descartar | `TournamentDetailScreen`, `LadderRoundShow` | `ui_torneo.h` |
| Ficha · Palmarés · Palmarés de una competición | `TournamentReportView`, `TournamentListScreen`, `TournamentHonoursScreen` | `ui_torneo_ficha.h` |
| Cartel de fase (GRAN FINAL…), «Volver al torneo» / «Ver campeón», abandono | `PhasePoster`, `VictoryScreen`, `MatchScreen` | `ui_partido.h`, `ui_victoria.h`, `.ino` |
| Ceremonia: campeón y puestos con XP, nivel, ELO y logros | `TournamentCeremony` | `ui_torneo_ceremonia.h` |
| Voces de torneo (25 reservadas en la SD) | — | `s3_comentarios.h` (último paso, tras probar) |

El contrato común está en `hardware/esp32s3_pantalla7/s3_torneos.h`.

## Equipo

| Agente | Trabajo | Archivos |
|---|---|---|
| 1 · Reglas | Reglas, ficha y palmarés en C++, empate en el motor. Prueba cruzada con la app: `tests/torneosPlaca.test.ts` guarda lo que calcula la app y `hardware/test/torneos_test.cpp` comprueba que la placa da lo mismo | `s3_torneos_reglas.h`, `s3_torneos_ficha.h`, `common/mfv3_engine.h` |
| 2 · Datos | JSON igual que la app, índice, plantillas, nube (filas `tournament` y `prefs.tournamentTemplates`), partidos con `tournament`, XP diferida, modo prueba y copias | `s3_torneos_datos.h`, `s3_partidos.h`, `s3_nube.h`, `s3_progresion.h`, `s3_datos.h` |
| 3 · Pantallas | Todas las pantallas y el paso por el partido, con prueba LVGL de toques | `ui_torneo*.h`, `ui_partido.h`, `ui_victoria.h`, `esp32s3_pantalla7.ino` |
| Coordinador | Contrato, unión de las tres partes, pruebas completas, revisión y documentación | `s3_torneos.h`, docs |

## Normas para las pantallas (las de los menús de la placa)

- `CLAUDE.md`: foto (o iniciales) en cada ficha de jugador; opciones como botones grandes con solo su título y la elegida iluminada; «?» solo si no se entiende sola; opciones hijas en `SubPanel` con «↳ Madre» y pestaña; sin scroll salvo listas (jornadas, rondas, palmarés, jugadores) y Ajustes; todo en 800 × 480; nada repetido.
- Cada pantalla LVGL se crea una vez al entrar; al tocar solo cambia lo tocado (color de botones, texto). Nada se repinta si no cambia. Valores vivos (reloj del Pool) cada 500 ms y solo si cambian.
- Sin imágenes nuevas ni letras fuera de `fuentes.txt`. Fondos en degradado como Inicio y configuración. Tarjetas Liga/Copa/Pool en su color neón (`#62d6ff`, `#f2c94c`, `#d65cff`), dos toques para entrar como Inicio (8 s, 180 ms hundida).
- Fotos con la caché reducida y redondeada que ya usa la placa (`ui_jugadores.h`).
- Tablas grandes en PSRAM (`calloc`), nunca globales grandes.
- Animaciones de la app que cuestan (rayos que giran, cartel deslizándose, animación de cambio de pistas): versión fija o por pasos, como ya se hizo con el cartel de gol y la victoria. Se avisa al dueño de cada diferencia.

## Diferencias previstas con la app

- Escudos, copas y banners: insignia de color con iniciales (como los equipos fijos) en vez de los dibujos de la app; el selector ofrece los colores del muestrario.
- «Siguiente ronda» del Pool: se enseñan las pistas nuevas con ▲ ▼ de golpe, sin animación de movimiento.
- Sin pantalla de Previsión aparte: el pronóstico va en la tarjeta del siguiente partido, como en la app.
- Las voces de torneo se activan solo cuando lo demás esté probado.

## Orden y comprobaciones

1. Contrato y plan (hecho).
2. Los tres agentes en paralelo, cada uno con sus pruebas.
3. Unión, `bash hardware/test/check_copies.sh` completo, `npx vitest run`, compilación real en GitHub y capturas app | placa.
4. Entrega para el dueño con capturas; prueba con el dedo en la placa cuando el dueño la instale.
