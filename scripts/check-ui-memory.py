"""Comprueba en el ELF que las tablas grandes ya no reservan BSS interna."""
import struct
import sys
from pathlib import Path
blob = Path(sys.argv[1]).read_bytes()
assert blob[:6] == b'\x7fELF\x01\x01'
h = struct.unpack_from('<16sHHIIIIIHHHHHH', blob)
sections = [struct.unpack_from('<IIIIIIIIII', blob, h[6]+i*h[11]) for i in range(h[12])]
def string(table, at):
    return table[at:table.index(b'\0',at)].decode('utf8','replace')
def payload(s):
    return blob[s[4]:s[4]+s[5]]
names=payload(sections[h[13]])
sn=[string(names,s[0]) for s in sections]
assert not any('ext_ram.bss' in sn[i] and s[5] for i,s in enumerate(sections)), 'No usar BSS externa en esta prueba'
objects={}
for s in sections:
    if s[1]!=2:continue
    syms,strings=payload(s),payload(sections[s[6]])
    for at in range(0,len(syms),s[9]):
        name,address,n,info,other,section=struct.unpack_from('<IIIBBH',syms,at)
        if (info&15)==1 and n and section<len(sections):
            objects[string(strings,name)]=(n,sn[section])
expected={
 '_ZN3torL5editsE':864, '_ZN3torL4optsE':1920, '_ZN5fotosL5namesE':3840,
 '_ZN3torL6selIdsE':2560, '_ZN3torL8kindFitsE':3480, '_ZN3torL6manualE':640,
 '_ZN3torL10draftFixedE':2560, '_ZN3torL8leftoverE':640,
 '_ZZN9historialL11buildDigestEvE1w':3456, '_ZZN9historialL10buildPairsEvE2ps':8192}
saving=0
for name,old in expected.items():
    assert name in objects,name
    n,section=objects[name]
    assert n<=16 and 'ext_ram' not in section,(name,n,section)
    saving+=old-n
assert saving>=27000
for token in ('catalog','jugadores'):
    found=[v for k,v in objects.items() if token in k and v[0]>=4000]
    assert len(found)==1 and 'ext_ram' not in found[0][1],token
print('BSS interna recuperada:',saving,'bytes; reservas de interfaz diferidas con guardas; motor/audio internos')
