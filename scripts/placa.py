"""Herramienta para la pantalla de 7" (Waveshare ESP32-S3-Touch-LCD-7C) conectada por USB.

  python scripts/placa.py puerto                 -> dice en qué puerto está la placa
  python scripts/placa.py compilar               -> compila hardware/esp32s3_pantalla7
  python scripts/placa.py grabar [--borrar]      -> compila y graba (--borrar: borra la memoria entera antes)
  python scripts/placa.py recursos               -> graba la partición FAT (letras e imágenes) ya preparada
  python scripts/placa.py serie [segundos]       -> enseña lo que dice la placa por el USB
  python scripts/placa.py orden "PING"           -> manda una orden y enseña la respuesta
  python scripts/placa.py captura <nombre>       -> guarda lo que enseña la pantalla en docs/capturas-placa/<nombre>.png

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
SKETCH = os.path.join(ROOT, "hardware", "esp32s3_pantalla7")
FQBN = os.environ.get(
    "PLACA_FQBN", "esp32:esp32:esp32s3:PSRAM=opi,FlashSize=32M,PartitionScheme=custom,CDCOnBoot=cdc"
)
BUILD = os.path.join(os.environ.get("TEMP", "/tmp"), "mfv3_placa_build")
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
    shutil.copy(os.path.join(SKETCH, "lv_conf.h"), os.path.join(libraries_dir(), "lv_conf.h"))
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
    elif cmd == "captura":
        captura(rest[0] if rest else "captura")
    else:
        print(__doc__)


if __name__ == "__main__":
    main(sys.argv[1:])
