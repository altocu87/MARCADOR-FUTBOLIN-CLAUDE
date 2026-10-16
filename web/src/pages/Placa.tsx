import { useEffect, useState } from 'react';
import {
  BOARDS,
  LINK_LABEL,
  PIN_FUNCTIONS,
  PIN_ORDER,
  ROLE_LABEL,
  installSteps,
  webInstallable,
  type BoardInfo,
} from '../../../src/inputs/hardware/catalog';
import { WiringDiagram } from '../../../src/ui/components/WiringDiagram';
import { APP_URL, href } from '../router';

declare module 'react' {
  // eslint-disable-next-line @typescript-eslint/no-namespace
  namespace JSX {
    interface IntrinsicElements {
      'esp-web-install-button': React.DetailedHTMLProps<React.HTMLAttributes<HTMLElement>, HTMLElement> & { manifest: string };
    }
  }
}

function shoppingList(b: BoardInfo): { item: string; optional?: boolean; link?: string }[] {
  const list: { item: string; optional?: boolean; link?: string }[] = [{ item: `${b.name} (${b.price})` }];
  if (b.role === 'pantalla') {
    list.push({ item: 'Mando inalámbrico: ESP32-C3 Super Mini + 2 pulsadores arcade', link: href({ page: 'placa', id: 'mando-c3' }) });
    list.push({ item: 'Cable USB-C de datos (para instalar el programa)' });
    list.push({
      item: b.id === 'waveshare-7c' ? 'Alimentación: cargador USB-C de 5 V / 2 A, o fuente de 7–36 V para un montaje fijo' : 'Cargador USB de 5 V / 2 A',
    });
    if (b.pins.sensorBlanco) list.push({ item: '2 sensores de gol', optional: true, link: href({ page: 'goles' }) });
    if (b.pins.ledBlanco) list.push({ item: 'Luces o relé para las salidas DO0/DO1', optional: true });
    return list;
  }
  list.push({ item: '2 pulsadores arcade (uno blanco y uno azul)' });
  list.push({ item: 'Cables dupont y, si quieres, una caja para los pulsadores' });
  if (b.role === 'mando') {
    list.push({ item: 'Cable USB-C de datos (para instalar el programa)' });
    list.push({ item: 'Batería externa USB pequeña, o LiPo de 3,7 V con módulo cargador' });
    return list;
  }
  list.push({ item: 'Cable USB de datos (no solo de carga)' });
  if (b.links.includes('wifi')) list.push({ item: 'Cargador USB o batería externa si va sin cable al PC', optional: true });
  list.push({ item: '1 pulsador más para PAUSA', optional: true });
  list.push({ item: '2 sensores de gol', optional: true, link: href({ page: 'goles' }) });
  list.push({ item: 'LED o zumbador para celebrar los goles', optional: true });
  return list;
}

function firstBoot(b: BoardInfo): string[] {
  if (b.role === 'pantalla')
    return [
      'La primera vez arranca en la «PRUEBA DE LA PLACA».',
      'Toca las 4 esquinas: se ponen en verde si el táctil funciona bien.',
      'Pulsa el botón BLANCO y el AZUL del mando: deben marcarse como OK.',
      b.pins.sensorBlanco ? 'Si tienes sensores, pasa la bola por cada uno (es opcional).' : 'Si algo no responde, revisa la conexión de ese elemento.',
      'Pulsa TERMINAR. Puedes repetir la prueba cuando quieras en AJUSTES → PRUEBA DE LA PLACA.',
    ];
  if (b.role === 'mando')
    return [
      'Al encender, el LED de la placa parpadea 2 veces.',
      'En la pantalla de mesa aparece «MANDO CONECTADO» (puede tardar hasta 20 segundos).',
      'Toque corto: 1 destello y gol. Toque largo (0,8 s): 3 destellos y se anula el último gol.',
      'Si tienes dos mesas cerca, cada pareja mando-pantalla necesita su propio número de grupo (ver Ayuda).',
    ];
  return [
    `Abre la app en Chrome o Edge y ve a Ajustes → Conexiones → ${b.links.includes('wifi') ? 'USB, Wi-Fi o Bluetooth' : 'USB'} → Conectar.`,
    `La app reconoce tu placa: verás «${b.name}» y lo que sabe hacer.`,
    'Pulsa los botones: en el Registro de Conexiones aparecen GOL_BLANCO, GOL_AZUL o ANULAR al mantenerlos.',
    'Pulsa «Ver esquema de conexión» si necesitas repasar los cables.',
  ];
}

type FwState = 'checking' | 'ready' | 'missing' | 'no-serial';

function Installer({ b }: { b: BoardInfo }) {
  const [state, setState] = useState<FwState>('checking');
  useEffect(() => {
    let alive = true;
    (async () => {
      if (!('serial' in navigator)) {
        setState('no-serial');
        return;
      }
      try {
        const r = await fetch(`firmware/${b.id}/${b.id}.bin`, { method: 'HEAD' });
        const type = r.headers.get('content-type') ?? '';
        const ok = r.ok && !type.includes('text/html');
        if (ok) await import('esp-web-tools');
        if (alive) setState(ok ? 'ready' : 'missing');
      } catch {
        if (alive) setState('missing');
      }
    })();
    return () => {
      alive = false;
    };
  }, [b.id]);

  return (
    <div className="card installer">
      <h3>Instalar desde el navegador</h3>
      <ol>
        <li>Usa Chrome o Edge en un ordenador.</li>
        <li>Conecta la placa por USB con un cable de datos.</li>
        <li>Pulsa «Instalar», elige el puerto de la placa (suele llamarse «USB JTAG/serial» o «USB Serial») y espera a que termine (alrededor de 1 minuto).</li>
      </ol>
      <p className="small muted">
        ¿No aparece la placa en la lista? Prueba con otro cable (muchos solo cargan) y, si sigue sin salir, mantén pulsado el
        botón <strong>BOOT</strong> de la placa mientras la conectas.
      </p>
      {state === 'checking' && <p className="muted">Comprobando…</p>}
      {state === 'no-serial' && (
        <p className="notice warn">Este navegador no puede hablar con placas por USB. Ábrelo en Chrome o Edge en un ordenador (no en iPhone).</p>
      )}
      {state === 'missing' && (
        <p className="notice warn">
          El programa de esta placa todavía no está publicado en esta web. Mientras tanto, instálalo con el IDE de Arduino
          (abajo). Se publica automáticamente cuando se compila en GitHub.
        </p>
      )}
      {state === 'ready' && (
        <esp-web-install-button manifest={`firmware/${b.id}/manifest.json`}>
          <button slot="activate" className="btn btn-primary btn-lg">
            Instalar en mi placa
          </button>
          <span slot="unsupported" className="notice warn">
            Tu navegador no lo permite: usa Chrome o Edge en un ordenador.
          </span>
          <span slot="not-allowed" className="notice warn">
            Abre esta página con https para poder instalar.
          </span>
        </esp-web-install-button>
      )}
      {b.sketch === 'esp32_marcador' && (
        <p className="small muted">
          Para que la placa además sirva la app en su propia red Wi-Fi hay que subirle también los archivos de la app con el
          IDE de Arduino (ver «Instalar con el IDE»). Por USB y Bluetooth funciona sin eso.
        </p>
      )}
    </div>
  );
}

export function Placa({ id }: { id: string }) {
  const b = BOARDS.find((x) => x.id === id);
  if (!b)
    return (
      <div className="wrap page">
        <h1>Placa no encontrada</h1>
        <a className="btn" href={href({ page: 'monta' })}>
          Ver placas compatibles
        </a>
      </div>
    );
  const used = PIN_ORDER.filter((f) => b.pins[f]);
  const installable = webInstallable(b);
  return (
    <div className="wrap page">
      <nav className="crumbs" aria-label="Pasos">
        <a href={href({ page: 'monta' })}>1 · Qué quieres montar</a>
        <a href={href({ page: 'monta', level: b.level })}>2 · Elige tu placa</a>
        <span aria-current="step">3 · Conecta e instala</span>
      </nav>
      <h1>{b.name}</h1>
      <p className="muted">{b.aka}</p>
      <div className="tags">
        <span className="tag accent">{ROLE_LABEL[b.role]}</span>
        <span className="tag">{b.chip}</span>
        <span className="tag">{b.voltage}</span>
        {b.links.map((l) => (
          <span key={l} className="tag">{LINK_LABEL[l]}</span>
        ))}
        <span className={`tag ${b.status === 'probada' ? 'ok' : 'warn'}`}>
          {b.status === 'probada' ? 'Probada en placa' : 'Probada en simulador'}
        </span>
      </div>
      {b.notes.map((n) => (
        <p key={n}>{n}</p>
      ))}

      <section className="block">
        <h2>
          <span className="num">1</span> Qué necesitas
        </h2>
        <ul className="checklist">
          {shoppingList(b).map((s) => (
            <li key={s.item} className={s.optional ? 'optional' : ''}>
              {s.link ? <a href={s.link}>{s.item}</a> : s.item}
              {s.optional && <span className="tag">opcional</span>}
            </li>
          ))}
        </ul>
      </section>

      <section className="block">
        <h2>
          <span className="num">2</span> Conecta los cables
        </h2>
        {used.length === 0 ? (
          <p>
            Esta pantalla no lleva pulsadores conectados: los goles llegan por el{' '}
            <a href={href({ page: 'placa', id: 'mando-c3' })}>mando inalámbrico</a> o tocando la pantalla.
          </p>
        ) : (
          <>
            <div className="table-scroll">
              <table className="pins">
                <thead>
                  <tr>
                    <th>Qué</th>
                    <th>Pin de la placa</th>
                    <th>El otro cable</th>
                  </tr>
                </thead>
                <tbody>
                  {used.map((f) => (
                    <tr key={f}>
                      <td>
                        <strong>{PIN_FUNCTIONS[f].label}</strong>
                        <div className="small muted">{PIN_FUNCTIONS[f].detail}</div>
                      </td>
                      <td className="pin">{b.pins[f]}</td>
                      <td className="muted">
                        {b.role === 'pantalla' && PIN_FUNCTIONS[f].kind === 'sensor' ? 'Común de las entradas (COM)' : PIN_FUNCTIONS[f].other}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
            <WiringDiagram board={b} used={used} />
            <p className="small muted">
              Línea continua: del pin al componente · Línea discontinua: el otro cable de cada componente, todos unidos a{' '}
              {b.role === 'pantalla' ? 'COM' : 'GND'}. Los pulsadores no llevan resistencias.
            </p>
          </>
        )}
      </section>

      <section className="block">
        <h2>
          <span className="num">3</span> Instala el programa
        </h2>
        {installable && <Installer b={b} />}
        <details className="card" open={!installable}>
          <summary>{installable ? 'Prefiero instalarlo con el IDE de Arduino' : 'Instalar con el IDE de Arduino'}</summary>
          <ol>
            {installSteps(b).map((s) => (
              <li key={s}>{s}</li>
            ))}
          </ol>
        </details>
      </section>

      <section className="block">
        <h2>
          <span className="num">4</span> Primer arranque
        </h2>
        <ol className="big-steps">
          {firstBoot(b).map((s) => (
            <li key={s}>{s}</li>
          ))}
        </ol>
      </section>

      <div className="card next">
        <div>
          <h3>¿Algo no va?</h3>
          <p className="muted">Mira los problemas frecuentes o vuelve a la lista de placas.</p>
        </div>
        <div className="cta-row">
          <a className="btn" href={href({ page: 'ayuda' })}>
            Ayuda
          </a>
          <a className="btn btn-primary" href={APP_URL}>
            Abrir la app
          </a>
        </div>
      </div>
    </div>
  );
}
