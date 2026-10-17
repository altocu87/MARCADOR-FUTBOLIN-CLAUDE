"""Rectángulo que cambia entre dos PNG de 800×480 (sin contar píxeles sueltos): imprime «x y w h» o «nada»."""
import sys

from PIL import Image, ImageChops, ImageFilter

a = Image.open(sys.argv[1]).convert("RGB")
b = Image.open(sys.argv[2]).convert("RGB")
d = ImageChops.difference(a, b).convert("L").point(lambda v: 255 if v > 6 else 0)
# Se borran los puntos sueltos (ruido del difuminado) y luego se recupera el borde de lo que sí cambia.
d = d.filter(ImageFilter.MinFilter(3)).filter(ImageFilter.MaxFilter(7))
box = d.getbbox()
if not box:
    print("nada")
else:
    x0, y0, x1, y1 = box
    print(x0, y0, x1 - x0, y1 - y0)
