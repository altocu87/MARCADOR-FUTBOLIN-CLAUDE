#!/usr/bin/env python3
"""Prepara los WAV exportados por el dueño. No toca originales ni conecta con la placa.
Uso: python scripts/preparar-comentarios.py <carpeta-originales> <paquete-salida>
Requiere FFmpeg ya instalado. Catálogo agrupado CMT1 compatible con pantalla >=0.5.
"""
import argparse
import array
import hashlib
import json
import math
import re
import shutil
import struct
import subprocess
import wave
import zlib
from pathlib import Path

GROUPS = {
    1: (0, 'comodin/after_inviting_pressure', 'gol_comodin'),
    2: (1, 'encajados3', 'tres_o_mas_encajados'),
    3: (2, 'empate', 'gol_empate'),
    4: (3, 'respuesta2', 'respuesta_tras_dos_o_mas_seguidos'),
    5: (0, 'comodin2', 'gol_comodin'),
    6: (0, 'comodin3', 'gol_comodin'),
    7: (5, 'empate_movido_60s', 'empate_con_dos_previos_en_60s'),
}

def code_for(identifier):
    code = 2166136261
    for byte in identifier.encode('utf-8'):
        code = ((code ^ byte) * 16777619) & 0xffffffff
    return code


def prepare(source, output, ffmpeg):
    files = []
    used = set()
    for path in sorted(source.rglob('GOL_*.wav')):
        match = re.match(r'^GOL_([1-6])_.*_([0-9]+)_([0-9]+)[.]wav$', path.name)
        if not match:
            match = re.match(r'^GOL_EMPATE TRAS ULTIMO MINUTO CON MAS DE 2 GOLES_([0-9]+)_([0-9]+)[.]wav$', path.name)
            if not match:
                continue
            group = 7
            variant, repeated = map(int, match.groups())
        else:
            group, variant, repeated = map(int, match.groups())
        if variant != repeated:
            raise ValueError('Índice de variante ambiguo: ' + path.name)
        pool, suffix, label = GROUPS[group]
        identifier = f'fc26/es/gol/{suffix}/{variant:02d}'
        code = f'{code_for(identifier):08x}'
        if code in used:
            raise ValueError('Código o variante duplicados: ' + path.name)
        used.add(code)
        with wave.open(str(path), 'rb') as wav:
            if (wav.getnchannels(), wav.getsampwidth(), wav.getcomptype()) != (1, 2, 'NONE'):
                raise ValueError('Se esperaba PCM mono de 16 bits: ' + path.name)
            rate = wav.getframerate()
            samples = array.array('h', wav.readframes(wav.getnframes()))
        window = max(1, rate // 200)  # bloques de 5 ms; conservar 20 ms antes del sonido
        threshold = 32768 * 10 ** (-55 / 20)
        first = None
        for at in range(0, len(samples), window):
            chunk = samples[at:at + window]
            if math.sqrt(sum(v * v for v in chunk) / len(chunk)) > threshold:
                first = at
                break
        if first is None:
            raise ValueError('Archivo sin señal audible detectada: ' + path.name)
        trim = max(0, first / rate - 0.020)
        relative = Path('sonidos') / code[:2] / (code + '.wav')
        dest = output / 'sd' / relative
        dest.parent.mkdir(parents=True, exist_ok=True)
        temporary = dest.with_suffix('.preparando.wav')
        if path.resolve() in (dest.resolve(), temporary.resolve()):
            raise ValueError('La salida no puede ser el original')
        gain = -1
        while True:
            subprocess.run([
                ffmpeg, '-hide_banner', '-loglevel', 'error', '-nostdin', '-y', '-i', str(path),
                '-af', f'atrim=start={trim:.9f},asetpts=PTS-STARTPTS,volume={gain}dB',
                '-ar', '16000', '-ac', '1', '-c:a', 'pcm_s16le', '-map_metadata', '-1',
                '-fflags', '+bitexact', '-flags:a', '+bitexact', str(temporary)
            ], check=True)
            with wave.open(str(temporary), 'rb') as wav:
                assert (wav.getnchannels(), wav.getsampwidth(), wav.getframerate(), wav.getcomptype()) == (1, 2, 16000, 'NONE')
                frames = wav.getnframes()
                pcm = array.array('h', wav.readframes(frames))
            clipped = sum(abs(v) >= 32767 for v in pcm)
            if not clipped:
                break
            gain -= 1
            if gain < -6:
                raise ValueError('Saturación persistente: ' + path.name)
        data = temporary.read_bytes()
        if len(data) > 1048576 or data[36:40] != b'data':
            raise ValueError('Tamaño o cabecera incompatibles: ' + path.name)
        temporary.replace(dest)
        files.append(dict(
            id=identifier, code=code, source=path.name, source_group=group, group=label, pool=pool,
            source_bytes=path.stat().st_size, source_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
            prepared_bytes=len(data), prepared_seconds=round(frames / 16000, 6),
            leading_trim_ms=round(trim * 1000, 3), gain_db=gain, rate=16000, channels=1, bits=16,
            peak=max(abs(v) for v in pcm), clipped_samples=clipped,
            sha256=hashlib.sha256(data).hexdigest(), crc32=f'{zlib.crc32(data):08x}',
            package_path=(Path('sd') / relative).as_posix(),
            sd_path='/' + relative.as_posix(),
            automatic_selection_active=False, upload_verified=False,
        ))
        print(group, variant, code, len(data), 'bytes', flush=True)
    if not files or len(files) > 1024:
        raise ValueError('El catálogo debe contener de 1 a 1024 archivos')
    files.sort(key=lambda f: (f['pool'], f['id']))
    binary_rows = b''.join(struct.pack('<BI', f['pool'], int(f['code'], 16)) for f in files)
    crc = zlib.crc32(binary_rows)
    (output / 'sd' / 'sonidos' / 'catalogo.cmt').write_bytes(b'CMT1' + struct.pack('<HI', len(files), crc) + binary_rows)
    commands = [f'COMENTARIOS_INICIO {len(files)}']
    commands += [f"COMENTARIOS_ITEM {f['pool']} {f['code']}" for f in files]
    commands += [f'COMENTARIOS_FIN {crc:08x}']
    (output / 'catalogo-comandos.txt').write_text('\n'.join(commands) + '\n', encoding='ascii')
    manifest = dict(schema=2, minimum_firmware='0.5', files=files, catalog_crc32=f'{crc:08x}',
                    total_bytes=sum(f['prepared_bytes'] for f in files),
                    rules=dict(generic_with_specific=True, group4_consecutive=True,
                               group4_requires_leading=False, group2_threshold=3,
                               group2_interpretation_pending=False, group2_scoring_team_previously_conceded=True,
                               equalizer_recent_window_ms=60000, equalizer_recent_other_goals=2,
                               equalizer_recent_uses_play_time=True, annul_audio_received=False))
    (output / 'catalogo.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('TOTAL', len(files), manifest['total_bytes'], 'bytes')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    ffmpeg = shutil.which('ffmpeg')
    if not ffmpeg:
        parser.error('FFmpeg no está instalado/en PATH; no se instalará automáticamente')
    prepare(args.source, args.output, ffmpeg)
