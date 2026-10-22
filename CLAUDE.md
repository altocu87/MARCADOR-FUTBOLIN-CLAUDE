# Marcador Futbolín — normas de diseño

- Siempre que aparezca la ficha de un jugador (tarjetas, carteles, listas, resúmenes…),
  enseña su foto (`player.photo`, con `Avatar` o `RankPhoto`; si no tiene, sus iniciales).
- «master» significa la rama `main`.
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

## Versiones y actualizaciones de la pantalla nativa (decisión del dueño, 19/10/2026)

- La versión de producto de la pantalla ESP32-S3 empieza en **0.1**. Seguir en **0.x**. No pasar a **1.0 ni superior** hasta que el dueño lo autorice expresamente; no decidirlo por madurez o por estar terminada una fase.
- Antes de **cada publicación de firmware**, subir esa versión y añadir una entrada a `hardware/esp32s3_pantalla7/releases.json`: fecha, título y qué cambia para quien juega, con lenguaje llano. Conservar las entradas antiguas. Los commits internos de una entrega todavía no publicada pueden compartir versión.
- Ese registro es la fuente única para la versión del programa, las notas del actualizador y el historial de la pantalla. Regenerar `s3_release.h` con `node scripts/native-release.mjs generate`; comprobarlo con `node scripts/native-release.mjs check` y ejecutar las pruebas.
- `majorOneAuthorized` permanece en `false` hasta esa autorización expresa. La publicación automática debe rechazar versiones repetidas, notas vacías y el salto no autorizado a 1.x.
- No confundir esta numeración con la versión de la app web, la versión del protocolo MFV3, el motor o el contador interno de compilaciones. No cambiar esos protocolos por subir la versión visible.
- Actualizaciones tiene su propio apartado en Ajustes, novedades legibles, explicación previa, confirmación de instalación y progreso real. No mostrar éxito hasta comprobar el archivo y, para el mando, recibir su confirmación.
- Preparar y probar en la rama; no fusionar, publicar una entrega de firmware ni grabar la placa sin la autorización correspondiente del dueño. Ver `docs/ACTUALIZACIONES_PANTALLA.md` y las entregas del equipo B para Cloud.


## Entrega pendiente de valoración por Cloud · 20/10/2026

El equipo B ha integrado en main la corrección de esperas USB y las opciones de sesión (pantalla 0.2). Antes de continuar, consultar [la entrega a equipo A](docs/ENTREGA_EQUIPO_B_USB_SESIONES_20261020.md) y el estado vigente en [ESTADO](docs/ESTADO.md). La instalación física de 0.2 build 54 ya está autorizada y verificada: leer [su informe](docs/VALIDACION_PLACA_0_2_20261020.md), incluidos los límites e incidencias del diagnóstico USB. Siguen pendientes el login real por QR y la lectura de límites de Supabase; no darlos por realizados. Registrar la revisión de Cloud cuando se realice.

## Mando físico C3-Zero · entrega en la rama del equipo B

La mini real del dueño es Waveshare ESP32-C3-Zero: leer [su entrega](docs/ENTREGA_MANDO_C3_ZERO_20261020.md). Se ha instalado y comprobado el saludo por radio, pero los contactos y luces de los pulsadores siguen pendientes de cablear y probar. Revisar el perfil RGB y la selección del firmware OTA antes de publicar soporte oficial. No confundir esta adaptación local con una nueva versión de la pantalla.

## Pantalla 0.3 · continuar desde el mando · rama del equipo B

Consultar [la entrega 0.3](docs/ENTREGA_TRANSICIONES_PULSADORES_0_3_20261022.md): permite continuar primera/segunda parte y entrar en penaltis con cualquiera de los pulsadores. Contiene pruebas y estado físico, además de los colores reales del cableado corregidos por el dueño. Actualiza la indicación anterior: ya se han probado un gol y una anulación por equipo en placa con resultado correcto; el fallo intermitente inicial no se reprodujo. La aceptación por Cloud y la publicación oficial siguen pendientes.

## Pantalla 0.4 · fechas y resumen · rama del equipo B

Consultar [la entrega 0.4](docs/ENTREGA_FECHAS_RESUMEN_0_4_20261022.md). Corrige la continuidad del reloj y añade TODOS al resumen para incluir registros sin fecha sin inventar su día. El dueño confirma varios partidos correctos con 0.3; faltan fechas en dos rápidos y aclara que el vacío es la pestaña Resumen. Las fechas recordadas son aproximadas y constan en la entrega: no migrarlas ni atribuirles hora exacta sin verificar. 0.4 ya está instalada por autorización expresa: consultar [la validación física](docs/VALIDACION_PLACA_0_4_20261022.md). Reloj puesto en hora; Resumen real muestra 2 partidos / 27 goles. Los dos registros originales siguen con fecha cero: no darlos por reparados ni inventar horas. No se ha medido retención del reloj al desconectar ni un nuevo guardado físico.

## Sonidos FC26 · grupo 1 y requisitos de sincronización

Leer [la entrega de sonidos](docs/ENTREGA_SONIDOS_FC26_COMODIN_20261022.md) y [el catálogo](docs/sonidos/catalogo_fc26_gol_comodin_01.json). Seis WAV comodín guardados y confirmados en SD, firmware 0.4; no están aún asignados automáticamente (selector actual limitado a cuatro). Los grupos específicos deben poder alternar con comodines. Última instrucción: iniciar audio al subir el gol al marcador visible; al anular, cortar comentario y sustituir por sonido de anulación (aún no entregado). No confundir requisitos con implementación ni activar solo cuatro de los seis. No publicar los WAV del dueño en el repositorio.
