"""Capturas de la pantalla de 7" simulada en el PC (mismo programa y LVGL, sin placa).

  python scripts/capturas_placa_pc.py      -> build/placa/capturas-pc/<escena>.png

Compila hardware/test/s3_capturas.cpp con LVGL para el PC, lo ejecuta y pasa cada imagen RGB565 a PNG.
"""
import glob
import shutil
import os
import struct
import subprocess
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "build", "placa", "capturas-pc")


def to_png(raw, png):
    data = open(raw, "rb").read()
    px = struct.unpack(f"<{800 * 480}H", data)
    rgb = bytearray(800 * 480 * 3)
    for i, v in enumerate(px):
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        rgb[i * 3] = (r << 3) | (r >> 2)
        rgb[i * 3 + 1] = (g << 2) | (g >> 4)
        rgb[i * 3 + 2] = (b << 3) | (b >> 2)
    Image.frombytes("RGB", (800, 480), bytes(rgb)).save(png)


def main():
    os.makedirs(OUT, exist_ok=True)
    for f in glob.glob(os.path.join(OUT, "*.rgb565")):
        os.remove(f)
    sh = os.environ.get("SH", r"C:\Program Files\Git\bin\sh.exe" if os.name == "nt" else "sh")
    if not os.path.exists(sh):
        sh = "sh"
    r = subprocess.run([sh, "test/s3_build.sh", "4", "test/s3_capturas.cpp"], cwd=os.path.join(ROOT, "hardware"))
    if r.returncode != 0:
        sys.exit("Fallo al compilar o ejecutar las capturas")
    for raw in sorted(glob.glob(os.path.join(OUT, "*.rgb565"))):
        png = raw[:-7] + ".png"
        to_png(raw, png)
        os.remove(raw)
        print(png)


if __name__ == "__main__":
    main()
