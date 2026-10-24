"""Comprueba en el ELF real que las tablas de interfaz salieron de la RAM interna."""
import struct
import sys
from pathlib import Path

blob = Path(sys.argv[1]).read_bytes()
assert blob[:6] == b'\x7fELF\x01\x01', 'Se requiere ELF32 little endian'
h = struct.unpack_from('<16sHHIIIIIHHHHHH', blob)
off, size, count, names_idx = h[6], h[11], h[12], h[13]
sections = [struct.unpack_from('<IIIIIIIIII', blob, off+i*size) for i in range(count)]
def string(table, at):
    return table[at:table.index(b'\0', at)].decode('utf8', 'replace')
def payload(section):
    return blob[section[4]:section[4]+section[5]]
names = payload(sections[names_idx])
section_names = [string(names, s[0]) for s in sections]
objects = []
for s in sections:
    if s[1] != 2:
        continue
    symbols, strings = payload(s), payload(sections[s[6]])
    for at in range(0, len(symbols), s[9]):
        name, address, n, info, other, section = struct.unpack_from('<IIIBBH', symbols, at)
        if (info & 15) == 1 and n and section < count:
            objects.append((string(strings, name), address, n, section_names[section]))
players = [o for o in objects if o[0] in ('jugadores', '_ZL9jugadores')]
assert len(players) == 1 and players[0][2] >= 19000
assert 'ext_ram.bss' in players[0][3], 'Jugadores sigue en RAM interna'
def app_table(o):
    return o in players or any(t in o[0] for t in ('_ZN3tor', '_ZZN3tor', '_ZN9historial', '_ZZN9historial', '_ZN5fotos', '_ZZN5fotos'))
moved = [o for o in objects if app_table(o) and 'ext_ram.bss' in o[3]]
assert sum(o[2] for o in moved) >= 40000, 'No se trasladaron las tablas previstas'
assert all(0x3c000000 <= o[1] < 0x3e000000 for o in moved)
catalog = [o for o in objects if 'comentarios' in o[0] and 'catalog' in o[0] and o[2] >= 4000]
assert len(catalog) == 1 and 'ext_ram' not in catalog[0][3], 'El selector de voces debe conservar su RAM interna'
print('Tablas de interfaz en PSRAM:', sum(o[2] for o in moved), 'bytes; catálogo de audio en RAM interna')
for name, address, n, section in moved:
    print(f'  {n:6d} bytes {address:08x} {name}')
