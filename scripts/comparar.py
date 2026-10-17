"""Junta la captura de la app y la de la placa, lado a lado, en un solo PNG.

Uso:
  python scripts/comparar.py <escena>            -> docs/comparar/<escena>.png
  python scripts/comparar.py app.png placa.png salida.png

La de la app sale de docs/capturas-app/<escena>.png (scripts/capturas-app.mjs) y la de la placa de
docs/capturas-placa/<escena>.png (scripts/captura_placa.py, por USB). Si falta una, se deja el hueco.
Abajo se añade una tira con la diferencia (zonas que cambian en rojo) para ver de un vistazo qué no coincide.
"""
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
W, H = 800, 480
GAP = 16
BAR = 34


def font(size):
    for name in ("arialbd.ttf", "DejaVuSans-Bold.ttf", "Arial Bold.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def load(path):
    if path and os.path.exists(path):
        im = Image.open(path).convert("RGB")
        return im if im.size == (W, H) else im.resize((W, H))
    hueco = Image.new("RGB", (W, H), (40, 40, 40))
    ImageDraw.Draw(hueco).text((W // 2, H // 2), "sin captura", fill=(160, 160, 160), font=font(28), anchor="mm")
    return hueco


def diff_strip(a, b):
    """Diferencia a mitad de tamaño: gris = igual, rojo = distinto."""
    d = ImageChops.difference(a, b).convert("L").point(lambda v: 255 if v > 40 else 0)
    base = ImageChops.blend(a, b, 0.5).convert("L").convert("RGB")
    red = Image.new("RGB", a.size, (255, 40, 40))
    out = Image.composite(red, base, d)
    return out.resize((W // 2, H // 2)), sum(d.histogram()[255:]) * 100.0 / (W * H)


def compose(app_path, board_path, out_path, title=""):
    a, b = load(app_path), load(board_path)
    strip, pct = diff_strip(a, b)
    sheet = Image.new("RGB", (W * 2 + GAP, BAR + H + GAP + H // 2), (18, 18, 22))
    dr = ImageDraw.Draw(sheet)
    f = font(20)
    dr.text((W // 2, BAR // 2), "APP", fill=(98, 214, 255), font=f, anchor="mm")
    dr.text((W + GAP + W // 2, BAR // 2), "PLACA", fill=(242, 201, 76), font=f, anchor="mm")
    sheet.paste(a, (0, BAR))
    sheet.paste(b, (W + GAP, BAR))
    y = BAR + H + GAP
    sheet.paste(strip, (0, y))
    dr.text((W // 2 + 24, y + 20), title, fill=(230, 230, 230), font=font(26))
    dr.text((W // 2 + 24, y + 64), f"Pixeles distintos: {pct:.1f} %", fill=(200, 200, 200), font=f)
    dr.text((W // 2 + 24, y + 96), "Rojo = no coincide", fill=(255, 90, 90), font=f)
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    sheet.save(out_path)
    return pct


def main(argv):
    if len(argv) == 1:
        name = argv[0]
        out = os.path.join(ROOT, "docs", "comparar", f"{name}.png")
        pct = compose(
            os.path.join(ROOT, "docs", "capturas-app", f"{name}.png"),
            os.path.join(ROOT, "docs", "capturas-placa", f"{name}.png"),
            out,
            name,
        )
    elif len(argv) == 3:
        out = argv[2]
        pct = compose(argv[0], argv[1], out, os.path.splitext(os.path.basename(out))[0])
    else:
        print(__doc__)
        return 1
    print(f"{out}  ({pct:.1f} % distinto)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
