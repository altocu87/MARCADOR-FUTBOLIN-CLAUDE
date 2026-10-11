import { describe, expect, it } from 'vitest';
import {
  KINDS,
  KIND_ORDER,
  avisosImagen,
  disponibilidadTexto,
  disponibleAhora,
  leerColores,
  leerTitulos,
  nombreDesdeArchivo,
  obtencionDe,
  obtencionErrores,
  obtencionTexto,
  revisarArchivo,
  type CatalogCollection,
  type CatalogItem,
} from '../web/admin/src/catalog';

const at = (iso: string) => new Date(`${iso}T12:00:00`).getTime();

describe('Catálogo · formatos', () => {
  it('cada tipo con archivo de imagen tiene tamaño, peso y consejo', () => {
    for (const k of KIND_ORDER) {
      const s = KINDS[k];
      expect(s.consejo.length, k).toBeGreaterThan(10);
      if (s.archivo === 'imagen') {
        expect(s.ancho! > 0 && s.alto! > 0 && s.maxKB > 0, k).toBe(true);
        expect(s.ajuste, k).toBeTruthy();
      }
    }
  });

  it('revisa el formato y el peso antes de subir', () => {
    expect(revisarArchivo('copa', { name: 'a.png', type: 'image/png', size: 2_000_000 })).toBeNull();
    expect(revisarArchivo('copa', { name: 'a.gif', type: 'image/gif', size: 10 })).toMatch(/formato/);
    expect(revisarArchivo('copa', { name: 'a.png', type: 'image/png', size: 20_000_000 })).toMatch(/pesa/);
    expect(revisarArchivo('sonido', { name: 'a.mp3', type: 'audio/mpeg', size: 900_000 })).toMatch(/pesa/);
    expect(revisarArchivo('titulo', { name: 'a.png', type: 'image/png', size: 10 })).toMatch(/no lleva/);
  });

  it('avisa de imágenes pequeñas, mal proporcionadas o sin transparencia', () => {
    expect(avisosImagen('copa', { name: 'a.png', type: 'image/png' }, 512, 512)).toEqual([]);
    const av = avisosImagen('copa', { name: 'a.jpg', type: 'image/jpeg' }, 100, 300);
    expect(av.join(' ')).toMatch(/pequeña/);
    expect(av.join(' ')).toMatch(/proporción/);
    expect(av.join(' ')).toMatch(/transparente/);
  });

  it('el nombre de la pieza sale del archivo', () => {
    expect(nombreDesdeArchivo('copa-dorada_02.png')).toBe('Copa dorada 02');
    expect(nombreDesdeArchivo('.png')).toBe('Sin nombre');
  });

  it('lee títulos y colores escritos uno por línea', () => {
    expect(leerTitulos('El Muro\n\nEl Muro\nUn título larguísimo de más de 24 letras')).toEqual({
      valores: ['El Muro'],
      errores: ['«Un título larguísimo de más de 24 letras» tiene más de 24 caracteres.'],
    });
    const c = leerColores('Fuego #ff5a1f #ffc400\n#00ff00\nMal');
    expect(c.colores).toEqual([
      { nombre: 'Fuego', valor: '#FF5A1F #FFC400' },
      { nombre: '#00FF00', valor: '#00FF00' },
    ]);
    expect(c.errores).toHaveLength(1);
  });
});

describe('Catálogo · cómo y cuándo se consigue', () => {
  it('temporada que cruza el año (Navidad)', () => {
    const navidad = { modo: 'anual' as const, desdeDia: '12-20', hastaDia: '01-06' };
    expect(disponibleAhora(navidad, at('2026-12-24'))).toBe(true);
    expect(disponibleAhora(navidad, at('2027-01-03'))).toBe(true);
    expect(disponibleAhora(navidad, at('2026-07-01'))).toBe(false);
    const verano = { modo: 'anual' as const, desdeDia: '06-21', hastaDia: '08-31' };
    expect(disponibleAhora(verano, at('2026-07-01'))).toBe(true);
    expect(disponibleAhora(verano, at('2026-12-24'))).toBe(false);
    expect(disponibilidadTexto({ ...navidad, soloUsoEnFechas: true })).toBe('Cada año, 20 dic – 6 ene · solo se usa en esas fechas');
  });

  it('entre dos fechas y siempre', () => {
    const d = { modo: 'fechas' as const, desde: at('2026-10-01'), hasta: at('2026-10-31') };
    expect(disponibleAhora(d, at('2026-10-15'))).toBe(true);
    expect(disponibleAhora(d, at('2026-11-15'))).toBe(false);
    expect(disponibleAhora({ modo: 'siempre' }, 0)).toBe(true);
  });

  it('un evento sin fechas, un precio a cero o un código raro no se pueden publicar', () => {
    expect(obtencionErrores({ tipo: 'evento', partidos: 3 }, { modo: 'siempre' })).toHaveLength(1);
    expect(obtencionErrores({ tipo: 'evento', partidos: 3 }, { modo: 'anual', desdeDia: '12-20', hastaDia: '01-06' })).toEqual([]);
    expect(obtencionErrores({ tipo: 'monedas', precio: 0 }, { modo: 'siempre' })).toHaveLength(1);
    expect(obtencionErrores({ tipo: 'codigo', codigo: 'a b' }, { modo: 'siempre' })).toHaveLength(1);
    expect(obtencionErrores({ tipo: 'logro', logro: '' }, { modo: 'siempre' })).toHaveLength(1);
  });

  it('pieza a pieza: cada una puede tener su forma; si no, la de la colección', () => {
    const col = { ambito: 'elemento', obtencion: { tipo: 'nivel', nivel: 5 } } as CatalogCollection;
    const propia = { obtencion: { tipo: 'torneos', cantidad: 3 } } as CatalogItem;
    expect(obtencionDe(propia, col)).toEqual({ tipo: 'torneos', cantidad: 3 });
    expect(obtencionDe({} as CatalogItem, col)).toEqual({ tipo: 'nivel', nivel: 5 });
    // Colección entera: manda la colección aunque la pieza tenga otra guardada.
    expect(obtencionDe(propia, { ...col, ambito: 'coleccion' })).toEqual({ tipo: 'nivel', nivel: 5 });
    expect(obtencionTexto({ tipo: 'pago', precio: 199 })).toBe('1,99 €');
  });
});
