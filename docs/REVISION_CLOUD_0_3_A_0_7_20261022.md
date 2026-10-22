# Revisión de Cloud (equipo A) · pantalla 0.3 a 0.7 · 22/10/2026

Revisión independiente de la rama `equipo-b/rendimiento-esp32s3` hasta `296ab08`, pedida por el dueño junto con su autorización para fusionarla en main. **Resultado: aceptada, sin fallos que bloqueen.** Fusionada en main por avance directo (main no tenía cambios propios desde `5fdc306`).

## Qué se revisó

- `s3_comentarios.h`: el reparto 70/30 es exacto (7·k casillas específicas y 3·k comodín) y se mantiene con varios contextos a la vez. Las bolsas agotan cada grupo antes de repetir, y la última frase de una ronda queda en la posición 0, excluida del primer sorteo de la siguiente. Las anulaciones nunca usan frases de gol. Reglas de grupos según el dueño: más de ocho desde el noveno gol, ventaja de dos incluida y tres encajados antes del gol. Las voces de torneo no están en el catálogo activo.
- `.ino`: el comentario arranca después de presentar el fotograma del marcador (descarte a 500 ms) y el guardado del resultado se hace después (rescate a 250 ms). Los pulsadores del mando, por radio o USB, continúan la parte o pasan a penaltis con `inputButtonGoal`. Los sensores de portería siguen con `inputGoal` y no pueden confirmar un final de parte. Gestión de sonidos y diagnósticos bloqueados durante el partido. `now` se vuelve a leer después de atender el USB, que era la causa probable del toque perdido al despertar descrita en 0.2. `FFat.begin(false)`: ya no se formatea solo si falla el montaje.
- `s3_reloj.h`: una lectura fallida del RTC ya no borra la hora conocida, que sigue avanzando. Se validan los años bisiestos. La hora obtenida por Internet se aplica aunque se cancele la consulta de versiones.
- Motor: comparaciones de tiempo firmadas, tolerantes al desbordamiento de `millis()`.
- Mando C3-Zero: LED RGB solo con `-DMFV3_MANDO_RGB_PIN=10`; sin esa opción, el comportamiento anterior no cambia.

## Pruebas repetidas por Cloud

Ejecutadas en una copia limpia de `296ab08`: `bash hardware/test/check_copies.sh` correcto (incluye variedad 70/30, comentarios con 800 WAV, Partido Loco, USB y sesiones), `vitest` 314/314, preparador de comentarios 6/6 y `native-release.mjs check` («0.7 correcto»). El instalador de Windows no se pudo ejecutar aquí; GitHub sí lo pasó.

## Observaciones no bloqueantes

1. Si coinciden varios contextos (p. ej. de 0–3 a 1–3: tres encajados, sigue perdiendo y respuesta), el 70 % se reparte entre ellos y cada uno suena menos. Es lo diseñado.
2. Empate, respuesta y tres encajados tienen solo 3 frases: se repetirán a menudo hasta que haya más voces.
3. Una frase descartada por `frame_timeout` cuenta como usada en su bolsa (reconocido en la entrega).
4. El firmware del mando que se publica es el de la Super Mini (LED GPIO8). En el C3-Zero del dueño no conviene instalarlo desde la pantalla hasta publicar una variante con LED RGB: los botones funcionarían, pero no la luz.
5. La nota de CLAUDE.md sobre «máscara de 8 bits» ha quedado superada: ahora es `PoolMask` de 32 bits.

## Sigue pendiente (no se da por hecho)

Prueba audible y de latencia jugando, desfase del RTC de unos 2 minutos, voces de anulación, torneos en la pantalla, login real por QR y lectura de límites de Supabase (`auth_config_read`).
