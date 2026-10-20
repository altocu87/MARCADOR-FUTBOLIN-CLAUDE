# Equipo B → Equipo A (Cloud): USB, QR y sesiones · 20/10/2026

Estado: preparado en `equipo-b/rendimiento-esp32s3`; integración en main autorizada por el propietario. Aceptación de Cloud pendiente. Esta entrega sustituye las observaciones anteriores sobre el apagado del diagnóstico. Firmware de producto **0.2**, manteniendo la prohibición de pasar a 1.x sin autorización.

## Qué notará el propietario

- La pantalla no espera a que el ordenador recoja los avisos USB al navegar. Apagar «RENDIMIENTO» apaga también sus avisos de dibujo.
- Web → Ajustes → Cuenta ya tiene «Conectar una placa (QR)». Pide confirmar con el correo de la cuenta que está abierta; no utiliza una cuenta distinta ni copia sus tokens a la placa.
- Web: «Recordar mi sesión» está activado por defecto, también para las sesiones que ya existían. Desactivado, la sesión permanece en esta pestaña al recargar y se pierde al cerrarla. Activado, usa almacenamiento persistente y renovación de tokens.
- Placa: la sesión se recuerda por defecto. Se puede cambiar en la pantalla del QR y, después de entrar, en Cuenta. «NO» mantiene el acceso en RAM hasta apagar; «SÍ» conserva el permiso al reiniciar. No se guarda la contraseña. La preferencia no se sube a la nube.
- Cerrar sesión en la web usa `scope=local`: la placa y otros aparatos conservan su sesión. La placa ya tenía cierre local; ahora no finge éxito si está sincronizando o no puede guardar el cierre. Cerrar aquí no borra jugadores ni partidos.

## Evidencias del bloqueo y límites del diagnóstico

**Confirmado en código y regresión:** `ui_port.h::logCb`, `perfRender`, `perfInvalidate` y `step` escribían con espera desde la tarea de la interfaz. `perfEnable(0)` dejaba callbacks activos que imprimían dibujos lentos. Además `goHome()` y otros cambios espontáneos de estado usaban `sendLine`, que también esperaba al USB. La regresión detectó esta segunda ruta antes de corregirla.

El núcleo utilizado en el firmware oficial es Arduino-ESP32 3.3.12 (registro de la compilación 39391954909). Su `HWCDC::write` puede acumular 20 esperas de 100 ms por escritura con el receptor saturado. Varios avisos consecutivos pueden producir pausas largas. [Código primario](https://github.com/espressif/arduino-esp32/blob/3.3.12/cores/esp32/HWCDC.cpp).

**Probable para el incidente comunicado:** espera acumulada de USB. No se afirma haber reproducido el minuto exacto en la placa. La observación previa duró 300 s con lector continuo: PING máximo 17 ms, memoria interna final 64.832 bytes, hueco mayor 31.732, PSRAM 6.011.780; nube sin cuenta y sin trabajo pendiente. Abrir USB produjo un reinicio USB_UART_CHIP_RESET, distinto del incidente anterior. La lectura continua puede ocultar precisamente la saturación del receptor.

Las flechas de Configuración solo modifican `cfg` en RAM (`onConfigChanged`); el comentario antiguo que decía NVS no corresponde al código vigente. Las fuentes ausentes de menús antiguos siguen siendo una incidencia separada; sus avisos pasan ahora por la salida sin espera.

## Implementación y revisión pedida a Cloud

- `uiport::diagnostic`: sin nueva tarea, cola ni reserva dinámica. Búfer de pila 256 bytes; omite avisos si no hay espacio. En USB nativo usa timeout 0 durante la escritura y restaura explícitamente 100 ms. Solo loop escribe en ese canal; no añadir escritores de otras tareas sin revisar esta condición. Los avisos espontáneos pasan por `sendEvent`; UART auxiliar conserva su salida anterior.
- `sendLine` para respuestas solicitadas y `CAPTURA` conservan su transporte. Una captura pedida explícitamente sigue siendo síncrona y no mide fluidez normal. Si el lector no consume avisos espontáneos, pueden perderse: nunca son una cola ilimitada.
- Los callbacks de rendimiento salen inmediatamente cuando está apagado y al cambiar de modo se limpian tiempos/áreas. No se desactivan límites del watchdog ni se oculta un fallo de memoria.
- `cloudSync.ts`: sessionStorage o almacenamiento persistente según la opción. Una sola renovación por pestaña; Web Locks, si está disponible, serializa entre pestañas. Relee el permiso guardado. Los eventos de almacenamiento mantienen el cierre/cuenta coherentes. Una respuesta de renovación anterior al cierre no puede reabrir la cuenta. En navegadores sin Web Locks solo se garantiza la serialización dentro de esa pestaña.
- `auth.ts`: JWT de duración habitual y refresh token rotatorio; no se aumenta su caducidad. Un permiso revocado exige entrar de nuevo; un fallo de red conserva el acceso guardado. Un error 403 de datos no borra por sí solo la cuenta.
- `s3_nube.h`: conserva las cuatro líneas del formato anterior y añade una quinta opcional para recordar. Escribe temporal + copia anterior + renombrado; recupera `.bak` si se interrumpió la sustitución. No es una garantía ante todos los fallos físicos de la SD/flash. Desactivar recordar quita el token del archivo conservando la sesión actual en RAM. Si hay un QR o trabajo de red en curso, solo cambia la intención en RAM y se guarda desde apply() al terminar, también si se cancela el QR; no escribe almacenamiento mientras la tarea está activa.
- El cierre nativo borra sus copias locales; no revoca remotamente la sesión de Supabase. No existe una lista de placas ni un botón para expulsar remotamente una placa desde la web en este cambio.
- No hay migraciones, cambios de RLS ni funciones de servidor en esta entrega. El QR ya usa `getUser(jwt)` en servidor y crea una sesión propia para la placa. Siguen pendientes la prueba con la cuenta real y la revisión de consumo atómico de la RPC descrita en la entrega QR anterior.

## Pruebas

- `npm test`: 314 pruebas correctas en 25 archivos.
- `npm run build:web`: correcto; permanece el aviso existente de tamaño del paquete.
- `node scripts/test-qr-browser.mjs`: login previo a QR, confirmación, error/reintento, permiso de cámara tardío y detección después de cerrar, todo con API ficticia.
- `node scripts/test-session-browser.mjs`: recordar por defecto, recargar, abrir otra pestaña, no recordar/cerrar, renovación con dos pestañas, detección QR positiva con el token de la cuenta abierta y cierre mientras la renovación está pendiente. Navegador Edge real aislado; cámara y Supabase simulados.
- `bash hardware/test/check_copies.sh`: motor, radio, sonidos, datos, memoria/concurrencia de nube, pantallas de las tres placas, actualizaciones y nuevas regresiones correctos; incluye generación de prototipos de Arduino.
- `s3_usb_diagnostics_test.cpp`: puerto lleno modelado con espera de 2 s en escrituras normales (control), 1.000 avisos sin espera, encender/apagar/reactivar rendimiento, 20 recorridos Inicio→Configuración→flechas→Inicio→Ajustes sin bloqueo, respuesta HELLO y captura completa 768.000 bytes con CRC correcto. No equivale a un ensayo físico de latencia.
- `nube_session_test.cpp`: formato antiguo, token renovado, recordar/no recordar, recuperación del renombrado interrumpido, fallo de escritura y cierre confirmado solo si se pudo guardar.
- CI incorpora las dos pruebas de navegador con Chrome preinstalado. La compilación real y los resultados de publicación se registrarán tras terminar Actions.

## Acceso indefinido: alcance y comprobación del servidor

No hay un plazo de caducidad añadido por la app ni por la placa. Sigue siendo posible perder acceso si el usuario cierra sesión, revoca permisos, cambia credenciales o borra el almacenamiento. También debe permitirlo la configuración de Supabase: sin tiempo máximo, sin límite de inactividad y sin sesión única por usuario. [Documentación oficial](https://supabase.com/docs/guides/auth/sessions).

El conector MCP directo deniega el acceso al proyecto. El workflow manual «Diagnóstico de sesiones» consulta solo GET `/config/auth` usando el secreto existente de Actions, sin SQL ni modificaciones. El script imprime solo cinco campos permitidos y falla si no puede comprobarlos; nunca imprime la respuesta completa ni claves.

## Plan de prueba física y reversión

1. Instalar solo la aplicación 0.2, sin borrar NVS, FFat o SD; conservar el firmware anterior de recuperación.
2. Repetir el recorrido comunicado con lector USB continuo y con el puerto abierto sin recoger salida. Probar también después de `RENDIMIENTO 1` → `RENDIMIENTO 0`, sin iniciar partidos ni guardar ajustes.
3. Comprobar capturas por CRC, botones, reposo y memoria. Medir por separado la latencia de dibujo y la respuesta a PING; no equipararlas.
4. Con el propietario: QR de la placa → lector de Ajustes de la web → confirmar su correo → verificar que ambos siguen conectados. Apagar/encender con recordar; cerrar la sesión de la web y verificar que la placa sigue dentro. No crear partidos de prueba en su cuenta.
5. Para revertir código usar revert del commit de entrega, sin borrar datos. Para firmware seguir el respaldo y las particiones de VALIDACION_PLACA_20261020.md; no prometer un respaldo completo de flash que no existe. Una publicación de reversión necesita nueva versión 0.x y sus notas.
