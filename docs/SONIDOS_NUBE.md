# Sonidos de la SD en la nube · guía para la sesión de Claude en el PC del dueño

Objetivo: subir los sonidos de la SD de la pantalla (comentarios, voces de los hándicaps y pitido) comprimidos en MP3
a un almacén **privado** del Supabase del marcador, para que la app los baje una vez a cada aparato y los use sin internet.
La app que los usa se está preparando en la misma rama (`claude/sonidos-nube`).

Reglas (de `CLAUDE.md`): hablar con Alex en español, frases cortas, sin tecnicismos. **No** fusionar en `main`, **no** grabar
la placa, **no** publicar firmware. No subir WAV ni la clave a GitHub. No tocar el Supabase de Logística Pro (es otro proyecto).

## Pasos

1. **Rama.** En la carpeta del proyecto: `git fetch origin claude/sonidos-nube` y `git checkout claude/sonidos-nube`
   (si hay cambios sin guardar, no los borres: pregunta a Alex).
2. **La SD.** Pide a Alex que saque la SD de la placa (con la placa apagada) y la meta en el lector del PC. Busca su letra:
   es la unidad que tiene una carpeta `sonidos` con cientos de archivos como `fd0284b7.wav`.
   Los nombres de la SD son códigos; `docs/sonidos/catalogo_fc26_870_0_16.json` dice qué es cada uno (no hay que renombrar nada).
3. **El pitido.** Todavía no está en la SD. Pregunta a Alex dónde tiene el audio del pitido del árbitro (mp3 o wav).
   Si no lo encuentra, se sigue sin él.
4. **Crear el almacén (una vez).** Alex entra en supabase.com → el proyecto **del marcador** → *SQL Editor* → *New query*,
   pega el contenido de `supabase/migrations/20261102120000_sonidos_nube.sql` y pulsa *Run*. Debe decir «Success».
5. **La clave.** En ese mismo proyecto: *Project Settings* → *API* (o *API Keys*). Alex crea con el Bloc de notas el archivo
   `.env.sonidos` en la raíz del proyecto con dos líneas (sin pegarlas en el chat):
   ```
   SUPABASE_URL=https://xxxx.supabase.co
   SUPABASE_SERVICE_KEY=la clave service_role (o «secret»)
   ```
   `.env.sonidos` está en `.gitignore`: comprueba con `git status` que no aparece.
6. **Prueba sin subir:** `python scripts/subir-sonidos-nube.py --origen X:\ --pitido "<ruta del pitido>" --prueba`
   (X = letra de la SD). Debe decir unos 860–930 sonidos y «Todo correcto». Avisos de «faltan en la SD» se cuentan a Alex.
7. **Subida de verdad:** el mismo comando sin `--prueba`. Cada archivo se sube, se vuelve a bajar y se compara.
   Debe acabar en «Todo correcto». Si se corta, se vuelve a lanzar: solo sube lo que falte.
8. **Al acabar:** borra `.env.sonidos`. Devuelve la SD a la placa (con la placa apagada).
   Si Alex quiere, el pitido también a la placa: `python scripts/placa.py pitido "<ruta>"` (solo con su permiso).
9. **Cuéntale a Alex** en dos líneas: cuántos sonidos se subieron, cuánto ocupan y si hubo problemas.
   Apunta el resultado arriba en `docs/ESTADO.md` (sección «Sonidos en la nube») y haz commit y push a `claude/sonidos-nube`.

## Qué queda en la nube

Almacén `sonidos` (privado: solo se lee con una cuenta de la app). Un `<código>.mp3` por sonido (mono, 16 kHz, 32 kbps) y
`indice.json` con la lista (`files[código] = { bytes, ms, wavSha256, mp3Sha256 }`). El código es el mismo de la SD
(FNV-1a del id del sonido: `soundCode()` en `src/inputs/hardware/boardSounds.ts`).
