"""Recorrido de la placa real por las pantallas del partido, con capturas (sin tocar la placa).

  python scripts/recorrido_placa.py            -> docs/capturas-placa/<escena>.png y docs/comparar/<escena>.png

Usa las órdenes USB TOQUE x y, GB / GA (gol), HORA y CAPTURA. Por tiempo usa partes de 1 minuto para no esperar
5 (la pantalla es la misma). Antes pone la hora de las capturas de la app (sábado 17/10/2026, 18:30).
"""
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
import placa  # noqa: E402


def orden(s, wait=0.15):
    with placa.open_serial() as ser:
        ser.write((s + "\n").encode())
        time.sleep(wait)
        ser.read(4096)


def toque(x, y, wait=0.35):
    orden(f"TOQUE {x} {y}", wait)


def captura(nombre):
    placa.captura(nombre)
    subprocess.run([sys.executable, os.path.join(ROOT, "scripts", "comparar.py"), nombre])


def a_config():
    toque(140, 280)
    toque(140, 280, 1.0)


def main(argv):
    solo = set(argv)
    want = lambda n: not solo or n in solo  # noqa: E731
    orden("HORA 2026-10-17 18:30:00")
    # Inicio
    if want("inicio"):
        captura("inicio")
    a_config()
    for _ in range(12):  # a 5 goles, como las capturas de la app (bajar al mínimo y subir)
        toque(204, 405, 0.12)
    for _ in range(4):
        toque(204, 138, 0.12)
    time.sleep(0.8)
    if want("configuracion"):
        captura("configuracion")
    # Partido por goles (5)
    toque(400, 443, 1.4)  # CONTINUAR → elegir jugadores (si la placa los tiene) o cuenta atrás
    if want("jugadores"):
        captura("jugadores")
    toque(267, 154, 0.6)  # primera ficha de la lista
    if want("jugadores-blanco-elegido"):
        captura("jugadores-blanco-elegido")
    toque(670, 438, 1.0)  # SIGUIENTE: AZUL
    if want("jugadores-azul"):
        captura("jugadores-azul")
    toque(267, 154, 0.6)  # primera libre
    toque(695, 438, 1.4)  # CONTINUAR → cuenta atrás
    if want("cuenta-atras"):
        captura("cuenta-atras")
    orden("SALTAR", 1.2)
    if want("partido"):
        captura("partido")
    orden("GB", 0.6)
    if want("partido-gol"):
        captura("partido-gol")
    time.sleep(3)
    for g in ["GA", "GB", "GA", "GB"]:
        orden(g, 3.4)
    if want("partido-marcador"):
        captura("partido-marcador")
    orden("GB", 3.4)
    if want("bola-de-partido"):
        captura("bola-de-partido")
    toque(400, 314, 0.8)  # PAUSA
    if want("pausa"):
        captura("pausa")
    toque(400, 260, 0.8)  # CONTINUAR
    orden("GB", 6.5)  # el cartel de gol y después la victoria
    if want("victoria"):
        captura("victoria")
    toque(44, 36, 1.0)  # inicio
    print("Hecho")


if __name__ == "__main__":
    main(sys.argv[1:])
