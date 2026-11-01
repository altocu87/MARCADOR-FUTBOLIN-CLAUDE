# Guion de voces de los hándicaps (Partido Loco)

Pedido por el dueño el 01/11/2026. Estado: **guion**; las voces aún no están grabadas (salvo los 3 avisos) ni programadas.

## Cómo suena en el partido

1. Sale el hándicap (el reloj se para): suena el **aviso** («¡Atentos! Hándicap en 3 segundos»).
2. Cuando acaba el aviso, la cuenta atrás de la pantalla «3 · 2 · 1» (decisión del dueño, 01/11/2026: primero el aviso y después la cuenta atrás).
3. Al terminar la cuenta atrás y verse el hándicap: **quién** (nombre del jugador o equipo) + **frase del hándicap**.
4. Al acabar: **final** («¡Todo vuelve a la normalidad!»).

Ejemplo: «¡Atentos, hándicap en 3 segundos!» … «¡Vicky!» + «¡Congelada! 15 segundos sin moverte.» … «¡Todo vuelve a la normalidad!».

## Reglas para grabar

- **Una sola voz** para nombres, frases y finales (si no, al juntar trozos se nota el cambio). Los avisos pueden ir con otra.
- Nombres: todos en **un audio**, en el orden de la lista, con **2 s de silencio** entre uno y otro. Se cortan solos.
- Frases: mejor **un audio por frase** (así se elige la mejor toma). También vale un audio por bloque con 2 s de silencio entre frases, en el orden de la tabla.
- Nombre del archivo: el código de la tabla (p. ej. `F07a.mp3`). Para un bloque: `F01-F06.mp3`.
- Tono de locutor animado, sin música. Nada de silencio largo al principio ni al final.

## A · Avisos (ya hay 3)

| Código | Frase |
|---|---|
| A1–A3 | «¡Atentos! Hándicap en 3 segundos» (las 3 recibidas el 01/11/2026) |

Preparadas el 01/11/2026 (WAV mono 16 kHz, 16 bits, pico −1 dB, silencio final recortado), fuera del repositorio en `FUTBOLIN ESP30/voces-handicaps/avisos/`. Aún no copiadas a la SD ni programadas.

| Código | Voz (ElevenLabs) | Duración |
|---|---|---|
| A1 | Theo | 3,4 s |
| A2 | Ana Alice | 5,1 s (la más larga: retrasa más la cuenta atrás) |
| A3 | Guillermo Brazález | 3,9 s |

## N · Nombres (uno por jugador)

Con energía y signos de exclamación: «¡Vicky!», «¡Victoria!», «¡Lucía!», «¡María!», «¡Alex!», «¡José!»… (lista completa por confirmar con el dueño). Si entra un jugador nuevo, solo hace falta su nombre.

## Q · Quién (fijos)

| Código | Frase |
|---|---|
| Q1 | «¡Equipo blanco!» |
| Q2 | «¡Equipo azul!» |
| Q3 | «¡Los dos equipos!» |
| Q4 | «y» (para unir dos nombres: «¡Vicky!» + «y» + «¡Alex!») |

## F · Frases de cada hándicap

| Código | Hándicap | Va después de | Frase |
|---|---|---|---|
| F01 | Goles dobles | — | «¡Goles dobles para todos durante 30 segundos!» |
| F02 | Goles dobles de un equipo | Q1/Q2 | «¡Vuestros goles valen doble durante 30 segundos!» |
| F03 | Gol triple | — | «¡El próximo gol vale triple!» |
| F04 | Robo | Q1/Q2 | «¡Si marcáis el próximo gol, le robáis uno al rival!» |
| F05 | Marcador congelado | — | «¡Marcador congelado! Durante 15 segundos los goles no cuentan.» |
| F06 | Cambio de posiciones | Q1/Q2/Q3 | «¡Cambio de posiciones! El de atrás pasa delante y el de delante, atrás.» |
| F07a | Jugador congelado (chico) | nombre | «¡Congelado! 15 segundos sin moverte.» |
| F07b | Jugadora congelada (chica) | nombre | «¡Congelada! 15 segundos sin moverte.» |
| F07c | Dos congelados (algún chico) | nombre y nombre | «¡Congelados! 15 segundos sin moveros.» |
| F07d | Dos congeladas (las dos chicas) | nombre y nombre | «¡Congeladas! 15 segundos sin moveros.» |
| F08a | Barra bloqueada, uno | nombre | «No puedes mover la barra del portero durante 15 segundos.» |
| F08b | | nombre | «No puedes mover la barra de la defensa durante 15 segundos.» |
| F08c | | nombre | «No puedes mover la barra del medio campo durante 15 segundos.» |
| F08d | | nombre | «No puedes mover la barra de la delantera durante 15 segundos.» |
| F08e | Barra bloqueada, dos | nombre y nombre | «No podéis mover la barra del portero durante 15 segundos.» |
| F08f | | nombre y nombre | «No podéis mover la barra de la defensa durante 15 segundos.» |
| F08g | | nombre y nombre | «No podéis mover la barra del medio campo durante 15 segundos.» |
| F08h | | nombre y nombre | «No podéis mover la barra de la delantera durante 15 segundos.» |
| F09a | Penalti | — | «¡Penalti para el equipo blanco!» |
| F09b | Penalti | — | «¡Penalti para el equipo azul!» |
| F10a | Traspaso (1.ª parte) | nombre (blanco) | «¡Te vas al equipo azul!» |
| F10b | Traspaso (2.ª parte) | nombre (azul) | «¡Y tú, al blanco! Durante 30 segundos.» |
| F11 | Prohibido el molinillo | — | «¡Prohibido el molinillo durante 30 segundos!» |
| F12 | Solo desde lejos | — | «¡Solo valen los goles desde la defensa o el medio campo, durante 30 segundos!» |
| F13 | Una sola mano | — | «¡Todos a jugar con una sola mano durante 20 segundos!» |

## R · Finales

| Código | Cuándo | Frase |
|---|---|---|
| R1–R3 | Al acabar cualquiera con tiempo | «¡Todo vuelve a la normalidad!» (3 versiones distintas, p. ej. «¡Se acabó! Todo vuelve a la normalidad») |
| R4 | Fin del cambio de posiciones | «¡Fin del cambio! Cada uno a su sitio.» |
| R5 | Fin del traspaso | «¡Fin del traspaso! Cada uno a su equipo.» |

## Notas

- Las duraciones van dentro de la frase. Si se cambia la duración de un hándicap, hay que volver a grabar su frase.
- «Jugador congelado» dura 15 s (el ejemplo del dueño decía 20): pendiente de que confirme.
- Total: 3 avisos + 4 «quién» + 25 frases + 5 finales = **37 audios fijos**, más un nombre por jugador.
- Los audios del dueño **no se suben al repositorio** (regla del proyecto); van a la SD.
