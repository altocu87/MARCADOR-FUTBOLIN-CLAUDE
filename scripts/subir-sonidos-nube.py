"""Sube a la nube (Supabase, almacén privado «sonidos») los sonidos de la SD de la pantalla, comprimidos en MP3.

La app los baja una vez a cada aparato y los usa sin internet. Guía completa: docs/SONIDOS_NUBE.md.

  python scripts/subir-sonidos-nube.py --origen E:\\ [--pitido C:\\ruta\\pitido.mp3] [--prueba]

  --origen   la SD en el lector (o una copia): se buscan todos los <código>.wav de su carpeta «sonidos»
  --pitido   el audio del pitido del árbitro (lo prepara igual que «placa.py pitido»), si aún no está en la SD
  --prueba   solo comprime y cuenta (en build/sonidos-nube), sin subir nada

Necesita ffmpeg (el mismo que usa placa.py) y dos datos del proyecto de Supabase del marcador
(Supabase → Project Settings → API): la URL y la clave «service_role». Se leen de SUPABASE_URL y
SUPABASE_SERVICE_KEY, o del archivo «.env.sonidos» de la raíz del proyecto (dos líneas SUPABASE_URL=… y
SUPABASE_SERVICE_KEY=…; no se sube a GitHub y conviene borrarlo al acabar), o se preguntan al empezar.

Cada sonido se sube como «<código>.mp3» (el mismo código de 8 cifras que en la SD) y al final «indice.json»
con la lista. Solo se sube lo nuevo o cambiado (se compara la huella del WAV). Después de subir cada uno
se vuelve a bajar y se compara byte a byte: si algo no cuadra, lo dice y no lo apunta en el índice.
"""
import argparse
import getpass
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
BUCKET = "sonidos"
INDEX = "indice.json"
CATALOG = os.path.join(ROOT, "docs", "sonidos", "catalogo_fc26_870_0_16.json")
OUT = os.path.join(ROOT, "build", "sonidos-nube")
HEX8 = re.compile(r"^[0-9a-f]{8}$")


def fnv1a(text):
    h = 2166136261
    for b in text.encode("utf-8"):
        h = ((h ^ b) * 16777619) & 0xFFFFFFFF
    return h


PITIDO_CODE = f"{fnv1a('arbitro/pitido_final'):08x}"  # el de hardware/esp32s3_pantalla7/s3_pitido.h


def find_wavs(origen):
    """{código: ruta} de todos los <código>.wav (la carpeta «sonidos» de la SD, con o sin subcarpetas)."""
    base = os.path.join(origen, "sonidos") if os.path.isdir(os.path.join(origen, "sonidos")) else origen
    found = {}
    for dirpath, _, files in os.walk(base):
        for f in files:
            stem, ext = os.path.splitext(f)
            if ext.lower() == ".wav" and HEX8.match(stem.lower()):
                found.setdefault(stem.lower(), os.path.join(dirpath, f))
    return base, found


def wav_ms(data):
    """Duración de un WAV PCM (busca el trozo «data»)."""
    try:
        rate = int.from_bytes(data[24:28], "little")
        block = int.from_bytes(data[32:34], "little")
        i = 12
        while i + 8 <= len(data):
            size = int.from_bytes(data[i + 4 : i + 8], "little")
            if data[i : i + 4] == b"data":
                return round(size / block / rate * 1000)
            i += 8 + size + (size & 1)
    except Exception:
        pass
    return 0


def to_mp3(wav_bytes):
    """MP3 mono de 16 kHz a 32 kbps: para voces no se nota y pesa ~8 veces menos que el WAV."""
    with tempfile.TemporaryDirectory() as tmp:
        src, dst = os.path.join(tmp, "a.wav"), os.path.join(tmp, "a.mp3")
        with open(src, "wb") as f:
            f.write(wav_bytes)
        cmd = ["ffmpeg", "-v", "error", "-y", "-i", src, "-ac", "1", "-ar", "16000", "-codec:a", "libmp3lame", "-b:a", "32k", dst]
        subprocess.run(cmd, check=True)
        with open(dst, "rb") as f:
            return f.read()


class Cloud:
    def __init__(self, url, key):
        self.base = url.rstrip("/") + "/storage/v1/object"
        self.key = key

    def _req(self, method, path, body=None, headers=None):
        h = {"Authorization": f"Bearer {self.key}", "apikey": self.key}
        h.update(headers or {})
        req = urllib.request.Request(f"{self.base}/{path}", data=body, method=method, headers=h)
        for attempt in range(4):
            try:
                with urllib.request.urlopen(req, timeout=60) as r:
                    return r.status, r.read()
            except urllib.error.HTTPError as e:
                if e.code < 500:
                    return e.code, e.read()
                err = e
            except (urllib.error.URLError, TimeoutError, ConnectionError) as e:
                err = e
            time.sleep(2 ** (attempt + 1))
        raise SystemExit(f"Sin conexión con Supabase: {err}")

    def get(self, name):
        status, data = self._req("GET", f"authenticated/{BUCKET}/{name}")
        return data if status == 200 else None

    def put(self, name, data, ctype):
        status, body = self._req(
            "POST", f"{BUCKET}/{name}", data, {"Content-Type": ctype, "x-upsert": "true", "Cache-Control": "max-age=60"}
        )
        if status not in (200, 201):
            raise SystemExit(f"Supabase no deja subir {name} ({status}): {body[:300].decode('utf-8', 'replace')}"
                             "\n¿Has creado el almacén (paso del SQL de la guía) y es la clave «service_role»?")


def read_env_file():
    path = os.path.join(ROOT, ".env.sonidos")
    if not os.path.exists(path):
        return
    for line in open(path, encoding="utf-8-sig"):
        k, sep, v = line.strip().partition("=")
        if sep and k.strip() in ("SUPABASE_URL", "SUPABASE_SERVICE_KEY") and not os.environ.get(k.strip()):
            os.environ[k.strip()] = v.strip().strip('"')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--origen", required=True)
    ap.add_argument("--pitido")
    ap.add_argument("--prueba", action="store_true")
    a = ap.parse_args()
    if not shutil.which("ffmpeg"):
        sys.exit("Falta ffmpeg (el mismo que usa placa.py pitido)")

    base, wavs = find_wavs(a.origen)
    print(f"En {base}: {len(wavs)} sonidos")
    extra = {}
    if a.pitido:
        import placa  # preparar_wav: recorta silencios, mono 16 kHz, igual que «placa.py pitido»

        extra[PITIDO_CODE] = placa.preparar_wav(a.pitido)
    elif PITIDO_CODE not in wavs:
        print("Aviso: el pitido no está en la SD; añádelo con --pitido <audio> si quieres que suene en la app")
    if not wavs and not extra:
        sys.exit("No hay ningún <código>.wav ahí: ¿es la letra de la SD?")

    catalog = {}
    if os.path.exists(CATALOG):
        d = json.load(open(CATALOG, encoding="utf-8"))
        for f in d.get("files", []) + d.get("reserved_files", []):
            catalog[f["code"]] = f["crc32"]
    missing = sorted(c for c in catalog if c not in wavs)
    if missing:
        print(f"Aviso: faltan en la SD {len(missing)} del catálogo de comentarios (p. ej. {', '.join(missing[:5])})")

    cloud = None
    if not a.prueba:
        read_env_file()
        url = os.environ.get("SUPABASE_URL") or input("URL del proyecto de Supabase del marcador: ").strip()
        key = os.environ.get("SUPABASE_SERVICE_KEY") or getpass.getpass("Clave service_role (no se ve al escribir): ").strip()
        cloud = Cloud(url, key)
    old = {}
    if cloud:
        raw = cloud.get(INDEX)
        if raw:
            old = json.loads(raw).get("files", {})
    os.makedirs(OUT, exist_ok=True)

    files, bad, sent, kept = {}, [], 0, 0
    total_wav = total_mp3 = 0
    items = sorted(wavs.items())
    for i, (code, path) in enumerate(items + sorted(extra.items()), 1):
        data = extra[code] if code in extra else open(path, "rb").read()
        total_wav += len(data)
        crc = f"{zlib.crc32(data) & 0xFFFFFFFF:08x}"
        if code in catalog and catalog[code] != crc:
            bad.append(f"{code}: el WAV de la SD no es el del catálogo (CRC {crc} ≠ {catalog[code]})")
            continue
        sha = hashlib.sha256(data).hexdigest()
        if old.get(code, {}).get("wavSha256") == sha:
            files[code] = old[code]
            kept += 1
            total_mp3 += old[code]["bytes"]
            continue
        mp3 = to_mp3(data)
        total_mp3 += len(mp3)
        entry = {"bytes": len(mp3), "ms": wav_ms(data), "wavSha256": sha, "mp3Sha256": hashlib.sha256(mp3).hexdigest()}
        if cloud:
            cloud.put(f"{code}.mp3", mp3, "audio/mpeg")
            back = cloud.get(f"{code}.mp3")
            if back != mp3:
                bad.append(f"{code}: al bajarlo otra vez no coincide")
                continue
            sent += 1
        else:
            with open(os.path.join(OUT, f"{code}.mp3"), "wb") as f:
                f.write(mp3)
        files[code] = entry
        if i % 25 == 0 or i == len(items) + len(extra):
            print(f"  {i}/{len(items) + len(extra)}…")

    index = {"version": 1, "updatedAt": int(time.time() * 1000), "files": files}
    blob = json.dumps(index, separators=(",", ":"), sort_keys=True).encode()
    if cloud:
        cloud.put(INDEX, blob, "application/json")
        if cloud.get(INDEX) != blob:
            bad.append("indice.json: al bajarlo otra vez no coincide")
    else:
        with open(os.path.join(OUT, INDEX), "wb") as f:
            f.write(blob)

    print()
    print(f"WAV: {total_wav / 1e6:.1f} MB → MP3: {total_mp3 / 1e6:.1f} MB")
    if cloud:
        print(f"Subidos y comprobados: {sent} · ya estaban iguales: {kept} · en el índice: {len(files)}")
    else:
        print(f"Prueba: {len(files)} MP3 en {OUT} (no se ha subido nada)")
    if bad:
        print(f"PROBLEMAS ({len(bad)}):")
        for b in bad:
            print("  " + b)
        sys.exit(1)
    print("Todo correcto")


if __name__ == "__main__":
    main()
