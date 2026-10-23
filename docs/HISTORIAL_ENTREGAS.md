# Historial de entregas (sacado del CLAUDE.md el 23/10/2026)

Notas de cada entrega, tal como estaban en el CLAUDE.md. Leer solo la sección que toque; el estado vigente está en `ESTADO.md`. Las entregas nuevas se apuntan aquí arriba del todo, no en el CLAUDE.md.

## Entrega B 0.11 · sonidos y conservación de A 0.9 · 23/10/2026

Antes de continuar leer [la entrega 0.11](ENTREGA_SONIDOS_0_11_20261023.md) y el estado físico vigente en `docs/ESTADO.md`. Incluye la 0.9 de A sin modificar su corrección Wi-Fi y el lote intermedio 0.10. Instalación 0.11 completada y verificada en COM6: 560 WAV (496 activos, 64 de torneo reservados), 123 anteriores y cuatro partidos conservados. Ver [validación física](VALIDACION_PLACA_0_11_20261023.md). El dueño autoriza la integración en main al terminar estas comprobaciones, mediante PR 21; revisión independiente de A aún pendiente. No fusionar main ni publicar OTA sin autorización. Mantener versiones 0.x y registrar novedades siempre.

Decisión del dueño: en partidos a goles, el gol ganador muestra victoria inmediatamente. Queda pendiente buscar una frase específica «gol y final» o dos clips muy cortos; **no concatenar ahora frases largas ni retrasar la victoria**. Introducción durante la primera cuenta atrás, puede seguir mientras comienza el juego, cortada por gol/anulación. Voces de final normal solo al agotarse el reloj de segunda parte con ganador. Últimos 20 s o último minuto nunca significan fin de primera parte ni partido solo a goles. Remontada pasa a ir ganando tras cualquier déficit previo, incluso de uno. Los animales conservan el tratamiento simplificado de gol/anulación.

## Entrega pendiente de valoración por Cloud · 20/10/2026

El equipo B ha integrado en main la corrección de esperas USB y las opciones de sesión (pantalla 0.2). Antes de continuar, consultar [la entrega a equipo A](ENTREGA_EQUIPO_B_USB_SESIONES_20261020.md) y el estado vigente en [ESTADO](ESTADO.md). La instalación física de 0.2 build 54 ya está autorizada y verificada: leer [su informe](VALIDACION_PLACA_0_2_20261020.md), incluidos los límites e incidencias del diagnóstico USB. Siguen pendientes el login real por QR y la lectura de límites de Supabase; no darlos por realizados. Revisión de Cloud de 0.3–0.7 registrada el 22/10/2026 en [docs/REVISION_CLOUD_0_3_A_0_7_20261022.md](REVISION_CLOUD_0_3_A_0_7_20261022.md).

## Mando físico C3-Zero · entrega en la rama del equipo B

La mini real del dueño es Waveshare ESP32-C3-Zero: leer [su entrega](ENTREGA_MANDO_C3_ZERO_20261020.md). Se ha instalado y comprobado el saludo por radio, pero los contactos y luces de los pulsadores siguen pendientes de cablear y probar. Revisar el perfil RGB y la selección del firmware OTA antes de publicar soporte oficial. No confundir esta adaptación local con una nueva versión de la pantalla.

## Pantalla 0.3 · continuar desde el mando · rama del equipo B

Consultar [la entrega 0.3](ENTREGA_TRANSICIONES_PULSADORES_0_3_20261022.md): permite continuar primera/segunda parte y entrar en penaltis con cualquiera de los pulsadores. Contiene pruebas y estado físico, además de los colores reales del cableado corregidos por el dueño. Actualiza la indicación anterior: ya se han probado un gol y una anulación por equipo en placa con resultado correcto; el fallo intermitente inicial no se reprodujo. La aceptación por Cloud y la publicación oficial siguen pendientes.

## Pantalla 0.4 · fechas y resumen · rama del equipo B

Consultar [la entrega 0.4](ENTREGA_FECHAS_RESUMEN_0_4_20261022.md). Corrige la continuidad del reloj y añade TODOS al resumen para incluir registros sin fecha sin inventar su día. El dueño confirma varios partidos correctos con 0.3; faltan fechas en dos rápidos y aclara que el vacío es la pestaña Resumen. Las fechas recordadas son aproximadas y constan en la entrega: no migrarlas ni atribuirles hora exacta sin verificar. 0.4 ya está instalada por autorización expresa: consultar [la validación física](VALIDACION_PLACA_0_4_20261022.md). Reloj puesto en hora; Resumen real muestra 2 partidos / 27 goles. Los dos registros originales siguen con fecha cero: no darlos por reparados ni inventar horas. No se ha medido retención del reloj al desconectar ni un nuevo guardado físico.

## Sonidos FC26 · grupo 1 y requisitos de sincronización

Leer [la entrega de sonidos](ENTREGA_SONIDOS_FC26_COMODIN_20261022.md) y [el catálogo](sonidos/catalogo_fc26_gol_comodin_01.json). Seis WAV comodín guardados y confirmados en SD, firmware 0.4; no están aún asignados automáticamente (selector actual limitado a cuatro). Los grupos específicos deben poder alternar con comodines. Última instrucción: iniciar audio al subir el gol al marcador visible; al anular, cortar comentario y sustituir por sonido de anulación (aún no entregado). No confundir requisitos con implementación ni activar solo cuatro de los seis. No publicar los WAV del dueño en el repositorio.

## Pantalla 0.5 · selector de comentarios y paquete de 61 voces

Consultar [la entrega 0.5](ENTREGA_COMENTARIOS_0_5_20261022.md). Código en rama B, catálogo nativo hasta 1.024, selección por situaciones con comodines, sustitución al anular y sincronización tras el frame. Instalación 0.5 autorizada y completada: 61 voces y catálogo activo comprobados después de reiniciar en COM6; consultar [validación física](VALIDACION_PLACA_0_5_20261022.md) y catálogo instalado de 61. La SD real tiene unos 8 GB: el dato previo de 13 MB era FFat porque Arduino 3.3.12 consultaba el disco 0. FsStore::sdSpace lo corrige por ruta /sdcard; no volver a usar SD_MMC.totalBytes/usedBytes como capacidad fiable con ambos volúmenes montados. Partidos y fechas originales conservados. Pendiente prueba audible/latencia real con el dueño. El informe físico registra también un desfase de reloj de unos 123 s frente al PC pese a valida=1 rtc=1; no se reajustó y sigue pendiente de diagnóstico. Grupo 4 confirmado: dos o más goles SEGUIDOS del rival, sin exigir ir ganando. Grupo 2 confirmado: marca quien ya había encajado al menos tres, no quien recibe su tercer gol. Nuevo grupo de seis empates: al menos dos goles válidos anteriores de cualquier equipo dentro de los 60 segundos de juego previos, sin contar el actual ni anulados; ventana móvil, no último minuto restante. Mantener comodines y empate normal elegibles. Consultar catálogo actualizado de 61 y pruebas de límite, pausas y cambio de parte. No afirmar latencia física ni instalación total a partir de las simulaciones. Mantener 0.x.

Próximos sonidos anunciados por el dueño: leer [el plan para A/Cloud](PLAN_COMENTARIOS_EVENTOS_20261022.md). Incluye inicio, segunda parte, final, prórroga, penaltis, victorias/campeón y sequía de goles. Son previsiones, no clips recibidos ni automatismos activos; el ejemplo de 90 s debe concretarse. La máscara (ahora `PoolMask` de 32 bits) corresponde a contextos de gol: no añadir todos los nuevos eventos sin revisar representación, persistencia y pruebas.

## Revisión de Cloud y fusión · 22/10/2026

Cloud (equipo A) ha revisado las entregas 0.3–0.7 y el dueño ha autorizado fusionarlas: main contiene la pantalla **0.7** y GitHub la publica como actualización oficial. Leer [la revisión](REVISION_CLOUD_0_3_A_0_7_20261022.md). Las secciones anteriores que dicen «rama del equipo B» describen su origen; el código ya está en main. No instalar desde la pantalla el firmware publicado del mando en el C3-Zero hasta que exista una variante con LED RGB.

## Ajustes de la pantalla y mando por modelo · 0.8 · en main (23/10/2026)

Leer [la entrega](ENTREGA_AJUSTES_PLACA_0_8_20261022.md). Menú de AJUSTES en `s3_opciones.h` (+ `s3_opciones_datos.h`); los sitios de los botones están en `ui::SETTINGS`. Los equipos fijos (`s3_equipos.h`) van en la nube dentro de `prefs`/`mesa`.`clubs`: al subir, cambiar solo ese campo y unir con los de la cuenta. El perfil del QR (`s3_perfil.h`) debe seguir igual que `buildProfileSnapshot` (`tests/perfil-placa.test.ts`). Las tablas grandes, en memoria dinámica (PSRAM), no como globales. Usar solo letras de `fuentes.txt`: una actualización por Wi-Fi no lleva letras nuevas. El mando anuncia su modelo en INFO (`MODEL_*`) y la pantalla descarga `mando-c3` o `mando-c3zero`. Un mando sin modelo no recibe actualizaciones hasta elegirlo en AJUSTES → MANDO. Antes de actualizar el C3-Zero del dueño, elegir C3-ZERO.

## Pantalla 0.9 · Wi-Fi e imagen (23/10/2026)

Con el Wi-Fi activo la imagen se corría (vídeo del dueño). `uiport::setNetwork()` baja el panel a `PCLK_NET_HZ` y realinea cada 250 ms mientras hay red; no quitarlo sin medir en la placa. Pendiente confirmar en la placa.



## Incidente del panel físico 0.11 · B · 23/10/2026

Carga visible seguida de alternancia de colores. Prueba privada con ritmo fijo instalada con autorización del dueño; cuatro partidos y catálogo conservados. Confirmación física y solución de producto pendientes; no publicada. Ver [incidente](INCIDENTE_PANEL_0_11_20261023.md). Una captura interna correcta y ACK USB no demuestran que el panel físico esté bien.
