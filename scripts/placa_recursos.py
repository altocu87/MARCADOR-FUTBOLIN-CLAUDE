"""Prepara la memoria FAT de la pantalla de 7" (19,9 MB): capas de la app, letras e imágenes.

  python scripts/placa_recursos.py            -> build/placa/fat/ (carpeta) y build/placa/ffat.bin (imagen para grabar)

Entradas:
  - build/placa/capas/*.png y build/placa/capas.json   (node scripts/placa-capas.mjs)
  - hardware/esp32s3_pantalla7/fuentes.txt               (letras Inter que usan las pantallas)
Salidas:
  - build/placa/fat/capas/<nombre>.jpg (opacas) o .bin (con transparencia, formato de imagen de LVGL RGB565A8)
  - build/placa/fat/fuentes/inter-<peso>[i]-<tamaño>.bin (lv_font_conv, 4 bits por píxel)
  - hardware/esp32s3_pantalla7/ui_capas.h  (posición y tamaño de cada capa, para el programa)
  - build/placa/ffat.bin  (FAT con nivelación de desgaste, como la monta FFat; se graba con `placa.py recursos`)
"""
import json
import os
import shutil
import struct
import subprocess
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, "build", "placa")
FAT = os.path.join(BUILD, "fat")
SKETCH = os.path.join(ROOT, "hardware", "esp32s3_pantalla7")
FAT_SIZE = 0x13E0000  # partitions.csv: ffat
INTER = os.path.join(ROOT, "node_modules", "@fontsource", "inter", "files")

# Letras: ASCII, Latin-1 (tildes, ñ, ¿, ¡, º, ·, ×) y comillas, guiones, puntos suspensivos, € y signo menos.
RANGO_TEXTO = "0x20-0x7E,0xA0-0xFF,0x2013-0x2014,0x2018-0x201D,0x2022,0x2026,0x20AC,0x2212"
JUEGOS = {"texto": RANGO_TEXTO, "cifras": "0x20,0x2B-0x3A,0x2013,0x2212,0x25"}  # cifras: + , - . / 0-9 : % y guiones


def rgb565_dither(im):
    """RGB888 -> RGB565 con tramado ordenado 4×4 (los degradados no hacen escalones)."""
    bayer = [0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5]
    w, h = im.size
    px = im.load()
    out = bytearray(w * h * 2)
    i = 0
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y][:3]
            t = bayer[(y & 3) * 4 + (x & 3)]
            r = min(255, r + (t >> 1))
            g = min(255, g + (t >> 2))
            b = min(255, b + (t >> 1))
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            out[i] = v & 0xFF
            out[i + 1] = v >> 8
            i += 2
    return out


def lvgl_bin_rgb565a8(im):
    """Imagen de LVGL 9 (cabecera de 12 bytes) en RGB565 + plano de transparencia A8."""
    im = im.convert("RGBA")
    w, h = im.size
    head = struct.pack("<BBHHHHH", 0x19, 0x14, 0, w, h, w * 2, 0)
    alpha = bytes(im.getchannel("A").tobytes())
    return head + bytes(rgb565_dither(im)) + alpha


def capas():
    manifest_path = os.path.join(BUILD, "capas.json")
    if not os.path.exists(manifest_path):
        print("Sin capas (node scripts/placa-capas.mjs)")
        return {}
    manifest = json.load(open(manifest_path, encoding="utf-8"))
    out = os.path.join(FAT, "capas")
    os.makedirs(out, exist_ok=True)
    for name, m in manifest.items():
        if m.get("medida"):  # solo una medida de la app, sin imagen
            m["archivo"] = None
            continue
        src = os.path.join(BUILD, "capas", f"{name}.png")
        im = Image.open(src)
        if m.get("alfa"):
            open(os.path.join(out, f"{name}.bin"), "wb").write(lvgl_bin_rgb565a8(im))
            m["archivo"] = f"capas/{name}.bin"
        else:
            im.convert("RGB").save(os.path.join(out, f"{name}.jpg"), quality=93, subsampling=0, optimize=True)
            m["archivo"] = f"capas/{name}.jpg"
    return manifest


def fuentes():
    spec = os.path.join(SKETCH, "fuentes.txt")
    out = os.path.join(FAT, "fuentes")
    os.makedirs(out, exist_ok=True)
    conv = os.path.join(ROOT, "node_modules", "lv_font_conv", "lv_font_conv.js")
    lista = []
    for raw in open(spec, encoding="utf-8"):
        line = raw.split("#")[0].strip()
        if not line:
            continue
        parts = line.split()
        peso, tam = int(parts[0]), int(parts[1])
        italica = "i" in parts[2:]
        juego = next((p for p in parts[2:] if p in JUEGOS), "texto")
        nombre = f"inter-{peso}{'i' if italica else ''}-{tam}"
        src = os.path.join(INTER, f"inter-latin-{peso}-{'italic' if italica else 'normal'}.woff")
        dst = os.path.join(out, f"{nombre}.bin")
        if not os.path.exists(dst) or os.path.getmtime(dst) < os.path.getmtime(spec):
            subprocess.run(
                ["node", conv, "--font", src, "-r", JUEGOS[juego], "--size", str(tam), "--bpp", "4",
                 "--format", "bin", "--no-compress", "-o", dst],
                check=True,
            )
        lista.append(nombre)
    return lista


def cabecera(manifest):
    lines = [
        "// Generado por scripts/placa_recursos.py a partir de las capas de la app (scripts/placa-capas.mjs).",
        "// No editar a mano: posición y tamaño de cada capa en la pantalla de 800×480.",
        "#ifndef MFV3_UI_CAPAS_H",
        "#define MFV3_UI_CAPAS_H",
        "#include <stdint.h>",
        "namespace capas {",
        "struct Capa {",
        "  const char* archivo;",
        "  int16_t x, y, w, h;",
        "};",
    ]
    for name, m in manifest.items():
        ident = name.replace("-", "_").upper()
        archivo = f'"{m["archivo"]}"' if m["archivo"] else "nullptr"
        x, y, w, h = (round(m[k]) for k in ("x", "y", "w", "h"))
        lines.append(f"const Capa {ident} = {{{archivo}, {x}, {y}, {w}, {h}}};")
    lines += ["}  // namespace capas", "#endif", ""]
    path = os.path.join(SKETCH, "ui_capas.h")
    new = "\n".join(lines)
    if not os.path.exists(path) or open(path, encoding="utf-8").read() != new:
        open(path, "w", encoding="utf-8", newline="\n").write(new)
        print("ui_capas.h actualizado")


def imagen_fat():
    dst = os.path.join(BUILD, "ffat.bin")
    subprocess.run(
        [sys.executable, os.path.join(ROOT, "scripts", "fatfsgen", "wl_fatfsgen.py"), FAT,
         "--output_file", dst, "--partition_size", str(FAT_SIZE), "--sector_size", "4096", "--long_name_support"],
        check=True,
    )
    return dst


def main():
    if os.path.exists(FAT):
        shutil.rmtree(os.path.join(FAT, "capas"), ignore_errors=True)
    os.makedirs(FAT, exist_ok=True)
    manifest = capas()
    letras = fuentes()
    cabecera(manifest)
    total = sum(os.path.getsize(os.path.join(d, f)) for d, _, fs in os.walk(FAT) for f in fs)
    img = imagen_fat()
    print(f"{len(manifest)} capas, {len(letras)} letras, {total / 1024 / 1024:.2f} MB de {FAT_SIZE / 1024 / 1024:.1f} MB -> {img}")


if __name__ == "__main__":
    main()
