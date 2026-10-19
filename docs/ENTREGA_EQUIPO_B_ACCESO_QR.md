# Equipo B · Recuperación del acceso por QR

Fecha: 19/10/2026. Rama: `equipo-b/rendimiento-esp32s3`. Revisión y aceptación por el equipo A (Cloud): pendiente.

## Punto de partida comprobado

El trabajo QR es el commit `cc064e0`, presente tanto en `pantalla-lvgl` como en `main` y heredado por esta rama. Ya incluye QR nativo LVGL, confirmación móvil, sesión propia de la placa, tabla/RPC y función de servidor. No era una rama QR independiente pendiente de fusionar.

El primer intento de «Base de datos» (run 39227596449, job 117977575752) aplicó `20261019180000_vinculo_placa.sql` y después falló al publicar por `edge_functions_write`. El segundo (job 118134919716), tras cambiar el secreto, falló antes de publicar por «IPv6 is not supported on your current network». Una consulta OPTIONS al endpoint de la función devolvió 404 NOT_FOUND antes de esta intervención. El conector directo de Supabase sigue denegando acceso al proyecto.

## Correcciones de código

- `BoardLinkPrompt`: al ir a Cuenta sin sesión, retira el aviso que tapaba el formulario; mantiene el código en memoria y vuelve a pedir confirmación después de entrar. No vincula automáticamente por iniciar sesión.
- Mensaje de éxito preciso: la app confirma que envió la autorización, sin dar por terminada la entrada de la placa antes de recibirla.
- `BoardQrScanner`: los cambios del contexto ya no reinician el lector. Libera la cámara si el permiso llega después de cerrar; ignora detecciones tardías y detiene las pistas al terminar o fallar.
- `boardCodeFrom`: valida el valor entero. Un QR con sufijo inválido ya no se acepta recortándolo silenciosamente.
- `database.yml`: opción manual `solo_qr=true`, que publica únicamente `vincular-placa` sin `supabase link` ni `db push`. Solo se utiliza después de comprobar que la migración QR está aplicada. El modo habitual sigue aplicando migraciones y publicando funciones. La configuración ausente ahora hace fallar la ejecución explícitamente.

No se modifica el firmware nativo ni se abre el puerto USB. Estas correcciones se ejecutan en el móvil o durante la publicación; no añaden trabajo de dibujo, memoria ni tareas a la placa.

## Verificación

- `npm test`: 304 pruebas pasan.
- `npm run build`: compilación correcta (aviso existente de tamaño del paquete web).
- `node scripts/test-qr-browser.mjs`: Edge aislado, app real y API ficticia. Prueba QR sin sesión → formulario accesible → login → confirmación con el mismo código → error de servidor → reintento correcto; también permiso de cámara tardío y detección después de cerrar. Bloquea peticiones a servicios reales; cámara simulada, sin usar la webcam.
- Sintaxis de los bloques de shell del workflow verificada con Bash.
- Estas pruebas no acreditan la entrada real de una placa: faltan publicación y prueba móvil → Supabase → placa.

## Revisión pendiente y límites

1. Confirmar la publicación de la función y comprobar OPTIONS y rechazo de llamadas sin sesión. No sustituyen una prueba autenticada.
2. Con aprobación del dueño para cambiar el estado de la placa: mostrar QR, leerlo con el móvil, confirmar y verificar la cuenta resultante; repetir cancelación, caducidad y fallo de red. Sin iniciar partidos ni cambiar ajustes para medir.
3. La espera nativa cuenta 150 consultas más sus tiempos de red: los cinco minutos son aproximados. La RPC hace SELECT y DELETE por separado; revisar su consumo atómico antes de considerar robusta la concurrencia. El enlace mágico de Auth es de un solo uso, pero no se ha probado una carrera con varios lectores.
4. El código pendiente del móvil vive en memoria: recargar durante el login exige volver a escanear. El lector integrado depende de BarcodeDetector; cuando no está disponible se indica usar la cámara del móvil.
5. La corrección general de conexión IPv4 del workflow de migraciones queda separada del modo `solo_qr`; no se ha confirmado qué permiso/configuración falta. El modo QR no arregla ni oculta ese problema al aplicar futuras migraciones.

## Estado de entrega

Código y pruebas preparados en la rama. No fusionar con main sin revisión de Cloud. El estado de publicación se actualizará con el resultado real de GitHub Actions.
