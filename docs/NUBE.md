# Nube: la cuenta del dueño de la mesa

La cuenta es del **dueño de la mesa**. Dentro de su cuenta se guardan sus jugadores (con foto), sus partidos, sus
torneos y los ajustes de la mesa. Los demás jugadores no tienen cuenta: su ficha pública es la del QR.

- **App web:** Ajustes → Cuenta (entrar, crear cuenta, he olvidado la contraseña, sincronizar, cerrar sesión).
  Sin cuenta, la app funciona en local como siempre. En modo prueba no se sincroniza nada.
- **Pantalla de 7":** AJUSTES → CUENTA (correo y contraseña con el teclado de la pantalla). Guarda solo el
  «refresh token» (en la memoria del chip, `/ffat/nube.txt`), nunca la contraseña. Sola, sincroniza solo
  con la pantalla en reposo (atenuada, o 3 min sin tocar si no se atenúa nunca) y sin partido: lo jugado se guarda
  al momento en la tarjeta y se sube en el siguiente reposo; si alguien toca la pantalla o el mando, se corta tras
  la petición en curso y sigue en otro reposo (decisión del dueño, 23/10/2026: el Wi-Fi descoloca la imagen y la
  radio del mando no oye mientras está conectada). Al entrar y con «Sincronizar ahora», en el momento. Va en una
  tarea aparte para que la pantalla no se pare. Sube los
  partidos jugados en ella y los jugadores creados o cambiados en ella; baja jugadores (con foto) y partidos.
  Un jugador de la placa sin id de la app se une con el de la cuenta que tiene su mismo nombre (y sus partidos se
  corrigen y se vuelven a subir). Código: `hardware/esp32s3_pantalla7/s3_nube.h` y `ui_cuenta.h`; prueba con una
  nube simulada en `hardware/test/s3_capturas.cpp`.
- Base de datos: `supabase/migrations/20261019120000_mesa_datos.sql` (tabla `mesa_datos`). Se aplica a Supabase
  al unir con `main` (workflow «Base de datos») o a mano desde Actions.

## La tabla `mesa_datos`

| columna | qué es |
|---|---|
| `owner_id` | el dueño (lo pone el servidor con la sesión) |
| `tipo` | `player`, `match`, `tournament` o `prefs` |
| `id` | el id de la app (`prefs` siempre es `mesa`) |
| `datos` | el JSON tal cual lo guarda la app (`Player`, `StoredMatch`, `Tournament`, `Preferences` sin `hardware` ni `brightness`) |
| `borrado` | `true` = se borró (la fila se queda para que los demás aparatos se enteren) |
| `actualizado` | la hora del servidor al crear o cambiar (para pedir «lo nuevo desde…») |

Cada dueño solo ve y toca sus filas. Las fotos van dentro del jugador (`photo`: data URL JPEG de 160 × 160).

## Peticiones (las mismas en la app y en la placa)

Todas llevan la cabecera `apikey: <ANON_KEY>`. `<URL>` y `<ANON_KEY>` son los del proyecto (`VITE_SUPABASE_URL`
y `VITE_SUPABASE_ANON_KEY`, los mismos que ya usa la app).

### Entrar
```
POST <URL>/auth/v1/token?grant_type=password
Authorization: Bearer <ANON_KEY>
Content-Type: application/json

{"email":"dueno@correo.es","password":"…"}
```
Responde `access_token` (dura `expires_in` segundos, normalmente 3600), `refresh_token` y `user.id`. Se guardan
los dos tokens (en la placa, en la memoria NVS); la contraseña no se guarda.

### Renovar (antes de que caduque, o si una petición responde 401)
```
POST <URL>/auth/v1/token?grant_type=refresh_token
Authorization: Bearer <ANON_KEY>

{"refresh_token":"…"}
```
Devuelve un `access_token` y un `refresh_token` **nuevos** (el viejo ya no vale: hay que guardar el nuevo).

### Bajar lo que ha cambiado desde una fecha
```
GET <URL>/rest/v1/mesa_datos?select=tipo,id,datos,borrado,actualizado&actualizado=gt.<FECHA>&order=actualizado.asc,tipo.asc,id.asc&limit=500&offset=0
Authorization: Bearer <access_token>
```
Sin `actualizado=gt.…` baja todo. Si vienen 500 filas, se pide la siguiente página (`offset=500`…). Se guarda la
mayor `actualizado` recibida para la próxima vez. Para la placa conviene pedir por tipos y sin fotos cuando haga
falta, p. ej. solo partidos: `&tipo=eq.match`; jugadores sin foto: `select=id,datos->>name,…`.

### Subir (crear o cambiar) uno o varios
```
POST <URL>/rest/v1/mesa_datos?on_conflict=owner_id,tipo,id
Authorization: Bearer <access_token>
Content-Type: application/json
Prefer: resolution=merge-duplicates,return=minimal

[{"tipo":"match","id":"<id del partido>","datos":{…StoredMatch…},"borrado":false}]
```
Los partidos de la placa ya se guardan en la tarjeta con el formato `StoredMatch` de la app
(`/sdcard/partidos/<id>.json`): se suben tal cual.

### Borrar
Lo mismo que subir, con `"borrado":true` y `"datos":{}`.

### Salir
```
POST <URL>/auth/v1/logout
Authorization: Bearer <access_token>
```

### Crear cuenta y olvidar la contraseña (solo en la app)
`POST <URL>/auth/v1/signup` con `{"email","password"}` y `POST <URL>/auth/v1/recover` con `{"email"}`. Si el
proyecto pide confirmar el correo, la cuenta no tiene sesión hasta abrir el enlace del correo.

## Cómo se sincroniza (src/services/cloud/sync.ts)

1. Se baja lo cambiado en la nube desde la última vez y se aplica, **salvo** lo que se ha cambiado aquí desde la
   última sincronización (gana lo de aquí, que es lo último que se hizo).
2. Se sube lo que ha cambiado aquí (se compara la huella de cada cosa con la de lo último sincronizado) y se marca
   como borrado lo que ya no está.
3. La primera vez en un aparato manda lo de la nube (salvo un jugador cambiado aquí después).

La app sincroniza al entrar, al abrirse, unos segundos después de cada cambio, cada minuto y al volver la
conexión. Probado con una nube simulada en `tests/cloud.test.ts`.

## Entrar en la placa con un QR

1. En la placa: AJUSTES → CUENTA → **CON QR**. Enseña un QR con `…/app/#vincular=<código>` (32 letras y números
   al azar; no se puede adivinar) y espera.
2. Con el móvil: la cámara abre esa dirección en la app (o en la app: Ajustes → Cuenta → «Conectar una placa
   (QR)», que lee el QR con la cámara). Con la sesión del dueño, la app pregunta «¿Conectar la placa?» y llama a
   `POST <URL>/functions/v1/vincular-placa` con `{"codigo": "…"}`.
3. La función (`supabase/functions/vincular-placa`, con permisos de servidor) prepara un enlace mágico de un solo
   uso para esa cuenta (sin mandar correo) y lo deja en `vinculos_placa` bajo ese código.
4. La placa pregunta cada 2 s `POST <URL>/rest/v1/rpc/vinculo_placa` con `{"p_codigo": "…"}`; cuando hay algo se
   lo lleva (y se borra) y abre su propia sesión con
   `POST <URL>/auth/v1/verify` y `{"type":"magiclink","token_hash":"…"}`. Cada vínculo vale 5 minutos y una vez.

La contraseña nunca pasa por la placa. La función se publica sola al subir a `main` (workflow «Base de datos»).

