# Marcador Futbolín

App React (`src/`) + pantalla nativa ESP32-S3 7" con LVGL (`hardware/esp32s3_pantalla7/`) + mando ESP32-C3 + nube Supabase.
Equipo A = Cloud (Claude); equipo B = esta rama. «master» significa la rama `main`.

## Cómo trabajar con el dueño (Alex)

- No es programador: hablarle en español, frases cortas, sin tecnicismos.
- Decidir solo lo que se pueda y apuntarlo; preguntar solo lo físico de la placa o elegir entre dos diseños.
- **Sin su permiso expreso: no fusionar en `main`, no publicar firmware/OTA, no grabar la placa, no pasar a versión 1.0.**
- No dar por hecho lo no comprobado: código hecho ≠ instalado ≠ probado en la placa. Decir cuál de las tres.

## Dónde mirar (leer solo lo que haga falta)

- Estado actual: `docs/ESTADO.md` (lo nuevo arriba; leer solo la primera sección).
- Notas de entregas anteriores: `docs/HISTORIAL_ENTREGAS.md`. Las entregas nuevas se apuntan ahí y en `ESTADO.md`, **no en este archivo**.
- Decisiones: `docs/DECISIONES.md` · Placa y cableado: `docs/HARDWARE.md` · Actualizaciones: `docs/ACTUALIZACIONES_PANTALLA.md`.
- `docs/ENTREGA_*`, `docs/VALIDACION_*` y `docs/REVISION_*` son historial: abrirlos solo si la tarea trata de esa entrega.
- No leer `dist/`, `build/`, `node_modules/` ni las imágenes `.png` sueltas de la raíz.

## Normas de diseño

- Siempre que aparezca la ficha de un jugador (tarjetas, carteles, listas, resúmenes…),
  enseña su foto (`player.photo`, con `Avatar` o `RankPhoto`; si no tiene, sus iniciales).
- Pantalla táctil y decisiones rápidas: las opciones a elegir son botones grandes con solo su título
  (la elegida se ilumina). Nada de puntos de radio ni casillas que parezcan lo que hay que pulsar, ni
  descripciones debajo de cada opción. Si una opción no se entiende sola, lleva una «?» al lado que la
  explica al tocarla («Al azar» no la necesita; «Equilibradas», sí).
- Opciones que dependen de otra (p. ej. «Cómo se forman las parejas» solo con «Parejas fijas»): van en un
  panel hijo (`SubPanel`) con el nombre de su opción madre arriba («↳ Parejas fijas»), y la opción madre
  elegida se une a él con una pestaña. Botones y letras grandes siempre que sobre sitio.
- Sin scroll, salvo en Ajustes, la clasificación y las listas (partidos, torneos, jugadores…): cada pantalla
  tiene que caber en el lienzo más pequeño (800 × 480). Los botones se reparten el alto que haya (grandes si
  sobra sitio, más bajos si no) en vez de salirse.
- Las pantallas se miran de pie y mientras se juega, no sentado leyendo: lo importante, grande y claro de un
  vistazo; nada repetido (si ya está en el título, en un cartel o en otra parte de la pantalla, no se vuelve a
  poner) y fuera lo que no aporta. Las explicaciones largas, detrás de una «?».

## Versiones de la pantalla (decisión del dueño, 19/10/2026)

- Siempre **0.x**; `majorOneAuthorized` sigue en `false` hasta que el dueño autorice la 1.0.
- Antes de cada publicación de firmware: subir la versión y añadir entrada en `hardware/esp32s3_pantalla7/releases.json`
  (fecha, título, qué cambia para quien juega, en lenguaje llano; conservar las antiguas). Es la fuente única de versión y novedades.
- Después: `node scripts/native-release.mjs generate`, luego `node scripts/native-release.mjs check`, y las pruebas.
- No confundir con la versión de la app web, del protocolo MFV3, del motor ni el contador de compilaciones.
- Actualizaciones: no mostrar éxito hasta comprobar el archivo y, para el mando, recibir su confirmación.

## Pruebas

`npm test` · `npm run typecheck` · `npm run test:hardware` · placa: `scripts/placa.py` (compilar, grabar, captura).

## Trampas conocidas (no repetir)

- Pantalla 0.12: reloj fijo a 12,5 MHz; no recuperar cambios de reloj/reinicios periódicos de 0.9. Ver `docs/INCIDENTE_PANEL_0_11_20261023.md`.
- Capacidad de la SD: usar `FsStore::sdSpace` (ruta `/sdcard`), nunca `SD_MMC.totalBytes/usedBytes`.
- Solo letras de `fuentes.txt`: una actualización por Wi-Fi no lleva letras nuevas.
- Tablas grandes en PSRAM (memoria dinámica), no como globales.
- Perfil del QR (`s3_perfil.h`) igual que `buildProfileSnapshot` (`tests/perfil-placa.test.ts`).
- Equipos fijos en la nube: al subir, cambiar solo `prefs`/`mesa`.`clubs` y unir con los de la cuenta.
- Mando C3-Zero del dueño: elegir C3-ZERO en AJUSTES → MANDO antes de actualizar; no instalarle el firmware del mando publicado hasta que haya variante con LED RGB.
- No subir al repositorio los WAV del dueño.
- Sonidos: antes de tocarlos, leer las secciones de sonidos de `docs/HISTORIAL_ENTREGAS.md` (reglas de grupos, victoria inmediata, etc.).
