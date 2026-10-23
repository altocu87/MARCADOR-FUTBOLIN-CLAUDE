"""Herramienta para la pantalla de 7" (Waveshare ESP32-S3-Touch-LCD-7C) conectada por USB.

  python scripts/placa.py puerto                 -> dice en qué puerto está la placa
  python scripts/placa.py compilar               -> compila hardware/esp32s3_pantalla7
  python scripts/placa.py grabar [--borrar]      -> compila y graba (--borrar: borra la memoria entera antes)
  python scripts/placa.py recursos               -> graba la partición FAT (letras e imágenes) ya preparada
  python scripts/placa.py serie [segundos]       -> enseña lo que dice la placa por el USB
  python scripts/placa.py orden "PING"           -> manda una orden y enseña la respuesta
  python scripts/placa.py captura <nombre>       -> guarda lo que enseña la pantalla en docs/capturas-placa/<nombre>.png
  python scripts/placa.py jugadores [archivo]    -> pasa jugadores y fotos (build/placa/jugadores-demo.txt, de
                                                    node scripts/jugadores-demo-placa.mjs), como hace la app
  python scripts/placa.py ranuras                -> qué programa hay en cada uno de los dos huecos y cuál arranca
  python scripts/placa.py probar <programa.bin>  -> graba un programa de prueba (el «-ota.bin») en el hueco que NO
                                                    está en uso y arranca con él: el de ahora se queda intacto
  python scripts/placa.py volver                 -> vuelve a arrancar con el programa del otro hueco (el anterior)

Usa arduino-cli con su propia carpeta de bibliotecas (LVGL 9.5, LovyanGFX, WebSockets) y copia lv_conf.h junto a lvgl.
"""
import glob
import os
import shutil
import struct
import subprocess
import sys
import time
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKETCH = os.environ.get("PLACA_SKETCH") or os.path.join(ROOT, "hardware", "esp32s3_pantalla7")
FQBN = os.environ.get(
    "PLACA_FQBN", "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=32M,PartitionScheme=custom,CDCOnBoot=cdc"
)
BUILD = os.path.join(os.environ.get("TEMP", "/tmp"), "mfv3_build_" + os.path.basename(SKETCH))
FAT_OFFSET = 0xC10000
W, H = 800, 480


def cli():
    exe = shutil.which("arduino-cli") or os.path.expanduser(r"~\arduino-cli\arduino-cli.exe")
    if not os.path.exists(exe) and not shutil.which("arduino-cli"):
        sys.exit("No encuentro arduino-cli")
    return exe


def libraries_dir():
    out = subprocess.run([cli(), "config", "get", "directories.user"], capture_output=True, text=True).stdout.strip()
    return os.path.join(out, "libraries")


def find_port():
    from serial.tools import list_ports

    for p in list_ports.comports():
        if p.vid == 0x303A:  # Espressif (USB nativo del ESP32-S3)
            return p.device
    return None


def need_port():
    p = find_port()
    if not p:
        sys.exit("No veo la placa por USB (¿cable de datos? ¿enchufada al conector USB?)")
    return p


def compilar():
    conf = os.path.join(ROOT, "hardware", "esp32s3_pantalla7", "lv_conf.h")
    shutil.copy(conf, os.path.join(libraries_dir(), "lv_conf.h"))
    os.makedirs(BUILD, exist_ok=True)
    t = time.time()
    r = subprocess.run(
        [cli(), "compile", "--fqbn", FQBN, "--build-path", BUILD, "--warnings", "none", SKETCH],
        capture_output=True,
        text=True,
    )
    if r.returncode != 0:
        print(r.stdout[-4000:])
        print(r.stderr[-6000:])
        sys.exit("Error al compilar")
    print(r.stdout.strip().splitlines()[0] if r.stdout.strip() else "")
    print(f"Compilado en {time.time() - t:.0f} s")


def esptool(*args):
    tools = glob.glob(os.path.expandvars(r"%LOCALAPPDATA%\Arduino15\packages\esp32\tools\esptool_py\*\esptool.exe"))
    exe = sorted(tools)[-1] if tools else "esptool"
    r = subprocess.run([exe, *args])
    if r.returncode != 0:
        sys.exit("esptool falló")


# ---------------------------------------------------------------- dos huecos para el programa (partitions.csv)
# La placa tiene dos huecos de 6 MB (app0 y app1); «otadata» dice con cuál arranca. Una actualización por Wi-Fi
# escribe siempre en el que no está en uso, y lo mismo hace «probar»: así el programa de antes se queda entero y
# «volver» solo cambia otra vez el hueco de arranque. Formato de otadata (ESP-IDF): dos copias de 4 KB; cada una
# empieza por ota_seq (4 bytes), etiqueta (20), estado (4) y CRC32 de ota_seq. Arranca la de ota_seq mayor válida:
# hueco (ota_seq - 1) % 2.
OTADATA = 0xE000
SLOTS = (0x10000, 0x610000)
SLOT_SIZE = 0x600000


def otadata_entries(raw):
    out = []
    for k in (0, 0x1000):
        seq, label, state, crc = struct.unpack_from("<I20sII", raw, k)
        valid = seq != 0xFFFFFFFF and crc == (zlib.crc32(struct.pack("<I", seq), 0xFFFFFFFF) & 0xFFFFFFFF)
        out.append((seq, valid))
    return out


def boot_slot(raw):
    """Hueco con el que arranca (0 o 1) y la ota_seq mayor válida (0 si otadata está vacío: arranca app0)."""
    seqs = [seq for seq, ok in otadata_entries(raw) if ok]
    if not seqs:
        return 0, 0
    top = max(seqs)
    return (top - 1) % 2, top


def otadata_for(raw, slot):
    """otadata nuevo para arrancar con `slot`: una entrada más alta en la copia que no tiene la mayor."""
    current, top = boot_slot(raw)
    seq = top + 1
    while (seq - 1) % 2 != slot:
        seq += 1
    entries = otadata_entries(raw)
    keep = 0 if entries[0][1] and entries[0][0] == top else 1
    sector = 1 - keep if top else 0
    entry = struct.pack("<I20sII", seq, b"\xff" * 20, 0xFFFFFFFF, zlib.crc32(struct.pack("<I", seq), 0xFFFFFFFF) & 0xFFFFFFFF)
    out = bytearray(raw)
    out[sector * 0x1000 : (sector + 1) * 0x1000] = entry + b"\xff" * (0x1000 - len(entry))
    return bytes(out)


def app_info(head):
    """Fecha y hora de compilación del programa de un hueco (esp_app_desc_t), o None si está vacío."""
    if len(head) < 0x100 or head[0] != 0xE9 or struct.unpack_from("<I", head, 0x20)[0] != 0xABCD5432:
        return None
    text = lambda a, n: head[a : a + n].split(b"\0")[0].decode("ascii", "replace")
    return f"compilado {text(0x90, 16)} {text(0x80, 16)} (IDF {text(0xA0, 32)})"


def read_flash(port, addr, size):
    tmp = os.path.join(os.environ.get("TEMP", "/tmp"), f"mfv3_lee_{addr:x}.bin")
    esptool("--chip", "esp32s3", "--port", port, "--after", "no-reset", "read-flash", hex(addr), hex(size), tmp)
    data = open(tmp, "rb").read()
    os.remove(tmp)
    return data


def ranuras(port=None):
    port = port or need_port()
    raw = read_flash(port, OTADATA, 0x2000)
    slot, _ = boot_slot(raw)
    for i, addr in enumerate(SLOTS):
        info = app_info(read_flash(port, addr, 0x100)) or "vacío"
        print(f"app{i}: {info}{'   <- arranca con este' if i == slot else ''}")
    return raw, slot


def cambiar_hueco(port, raw, slot):
    tmp = os.path.join(os.environ.get("TEMP", "/tmp"), "mfv3_otadata.bin")
    open(tmp, "wb").write(otadata_for(raw, slot))
    esptool("--chip", "esp32s3", "--port", port, "write-flash", hex(OTADATA), tmp)
    os.remove(tmp)


def probar(path):
    data = open(path, "rb").read()
    if not data or data[0] != 0xE9 or len(data) > SLOT_SIZE:
        sys.exit("Eso no es un programa de la pantalla (usa el «-ota.bin», no el «.bin» entero)")
    port = need_port()
    raw, slot = ranuras(port)
    other = 1 - slot
    print(f"Grabando la prueba en app{other}; app{slot} se queda como está")
    esptool("--chip", "esp32s3", "--port", port, "--after", "no-reset", "write-flash", hex(SLOTS[other]), path)
    cambiar_hueco(port, raw, other)
    print("Listo: arranca con la prueba. Para volver a la de antes: python scripts/placa.py volver")


def volver():
    port = need_port()
    raw, slot = ranuras(port)
    other = 1 - slot
    if not app_info(read_flash(port, SLOTS[other], 0x100)):
        sys.exit(f"app{other} está vacío: no hay programa anterior al que volver")
    cambiar_hueco(port, raw, other)
    print(f"Listo: arranca otra vez con app{other}")


def grabar(borrar=False):
    compilar()
    port = need_port()
    if borrar:
        esptool("--chip", "esp32s3", "--port", port, "erase-flash")
        time.sleep(1)
        port = need_port()
    r = subprocess.run([cli(), "upload", "--fqbn", FQBN, "--input-dir", BUILD, "-p", port, SKETCH])
    if r.returncode != 0:
        sys.exit("No se pudo grabar")
    print("Grabado")


def recursos():
    img = os.path.join(ROOT, "build", "placa", "ffat.bin")
    if not os.path.exists(img):
        sys.exit("Falta build/placa/ffat.bin (python scripts/placa_recursos.py)")
    port = need_port()
    esptool("--chip", "esp32s3", "--port", port, "--baud", "921600", "write-flash", hex(FAT_OFFSET), img)


def open_serial(timeout=0.2):
    import serial

    s = serial.Serial(need_port(), 115200, timeout=timeout)
    s.dtr = True
    s.rts = False
    return s


def serie(seconds=8.0):
    with open_serial() as s:
        end = time.time() + seconds
        while time.time() < end:
            line = s.readline()
            if line:
                print(line.decode("utf-8", "replace").rstrip())


def orden(text, seconds=2.0):
    with open_serial() as s:
        s.reset_input_buffer()
        s.write((text + "\n").encode())
        end = time.time() + seconds
        while time.time() < end:
            line = s.readline()
            if line:
                print(line.decode("utf-8", "replace").rstrip())


def captura(name):
    """La placa contesta «CAPTURA <ancho> <alto> <bytes> <crc32>» y a continuación la imagen en RGB565."""
    from PIL import Image

    with open_serial(timeout=5) as s:
        s.reset_input_buffer()
        s.write(b"CAPTURA\n")
        end = time.time() + 10
        head = None
        while time.time() < end:
            line = s.readline().decode("utf-8", "replace").strip()
            if line.startswith("CAPTURA "):
                head = line.split()
                break
        if not head:
            sys.exit("La placa no contestó a CAPTURA")
        w, h, n, crc = int(head[1]), int(head[2]), int(head[3]), int(head[4], 16)
        data = bytearray()
        t = time.time()
        while len(data) < n and time.time() - t < 30:
            data += s.read(n - len(data))
    if len(data) != n or (zlib.crc32(data) & 0xFFFFFFFF) != crc:
        sys.exit(f"Captura incompleta ({len(data)} de {n} bytes)")
    px = struct.unpack(f"<{w * h}H", bytes(data))
    rgb = bytearray(w * h * 3)
    for i, v in enumerate(px):
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        rgb[i * 3] = (r << 3) | (r >> 2)
        rgb[i * 3 + 1] = (g << 2) | (g >> 4)
        rgb[i * 3 + 2] = (b << 3) | (b >> 2)
    out = os.path.join(ROOT, "docs", "capturas-placa", f"{name}.png")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    Image.frombytes("RGB", (w, h), bytes(rgb)).save(out)
    print(f"{out} ({time.time() - t:.1f} s)")


def jugadores(path):
    """Manda las órdenes de jugadores esperando cada respuesta, igual que syncPlayers() de la app."""
    lines = [l.rstrip("\r\n") for l in open(path, encoding="utf-8") if l.strip()]
    t = time.time()
    with open_serial(timeout=3) as s:
        s.reset_input_buffer()
        for i, l in enumerate(lines):
            s.write((l + "\n").encode("utf-8"))
            if l.startswith("JUGADOR "):
                time.sleep(0.08)  # sin agobiar a la placa
                continue
            reply = s.readline().decode("utf-8", "replace").strip()
            while reply and not reply.startswith(("JUGADORES", "FOTO")):
                reply = s.readline().decode("utf-8", "replace").strip()
            if "ERROR" in reply or not reply:
                sys.exit(f"Línea {i + 1}: {l[:40]}… → {reply or 'sin respuesta'}")
            if l.startswith("JUGADORES_FIN") or l.startswith("FOTO_FIN"):
                print(reply)
    print(f"{len(lines)} órdenes en {time.time() - t:.1f} s")


def main(argv):
    if not argv:
        print(__doc__)
        return
    cmd, rest = argv[0], argv[1:]
    if cmd == "puerto":
        print(find_port() or "No veo la placa")
    elif cmd == "compilar":
        compilar()
    elif cmd == "grabar":
        grabar("--borrar" in rest)
    elif cmd == "recursos":
        recursos()
    elif cmd == "serie":
        serie(float(rest[0]) if rest else 8.0)
    elif cmd == "orden":
        orden(" ".join(rest))
    elif cmd == "jugadores":
        # Por defecto los de la placa (los 6 que dejó Alex); si no, los de prueba de la app.
        mine = os.path.join(ROOT, "build", "placa", "jugadores-placa.txt")
        jugadores(rest[0] if rest else mine if os.path.exists(mine) else os.path.join(ROOT, "build", "placa", "jugadores-demo.txt"))
    elif cmd == "ranuras":
        ranuras()
    elif cmd == "probar" and rest:
        probar(rest[0])
    elif cmd == "volver":
        volver()
    elif cmd == "captura":
        captura(rest[0] if rest else "captura")
    else:
        print(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])
