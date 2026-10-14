import { afterEach, describe, expect, it } from 'vitest';
import { DEFAULT_PROGRESSION } from '../src/services/persistence/defaults';
import {
  DEFAULT_RULES,
  RULE_FIELDS,
  applyGameRules,
  categoryFor,
  computeProgression,
  eloGain,
  eloSettings,
  fetchRemoteRules,
  readCachedRules,
  rulesDiff,
  rulesProblems,
  sanitizeRules,
  writeCachedRules,
  xpForLevel,
  xpForLevelWith,
} from '../src/services/progression';
import { makeMatch } from './helpers';

// Las reglas se aplican sobre tablas compartidas: cada prueba deja las recomendadas.
afterEach(() => applyGameRules(DEFAULT_RULES));

describe('Reglas del juego (panel de administración)', () => {
  it('los valores recomendados son los que usaba la app', () => {
    expect(eloSettings(DEFAULT_RULES).eloInitial).toBe(DEFAULT_PROGRESSION.eloInitial);
    expect(eloSettings(DEFAULT_RULES).kEstablished).toBe(DEFAULT_PROGRESSION.kEstablished);
    expect(xpForLevel(10)).toBe(xpForLevelWith(DEFAULT_RULES, 10));
    expect(categoryFor(1200).name).toBe('Chatarra');
    expect(categoryFor(1225).name).toBe('Madera');
  });

  it('cada dato tiene su explicación y lo recomendado está dentro de sus límites', () => {
    expect(RULE_FIELDS.map((f) => f.key).sort()).toEqual(Object.keys(DEFAULT_RULES).sort());
    for (const f of RULE_FIELDS) {
      expect(f.meaning.length).toBeGreaterThan(10);
      expect(f.why.length).toBeGreaterThan(10);
    }
    expect(rulesProblems(DEFAULT_RULES)).toEqual([]);
  });

  it('lo que llega roto se arregla: números fuera de límites, campos que faltan y rangos desordenados', () => {
    const r = sanitizeRules({ kEstablished: 999, xpWin: 'mucho', rankBronze: 1200, rankWood: 1300, desconocido: 1 });
    expect(r.kEstablished).toBe(50);
    expect(r.xpWin).toBe(DEFAULT_RULES.xpWin);
    expect(r.rankBronze).toBeGreaterThan(r.rankWood);
    expect(r.rankSilver).toBeGreaterThan(r.rankBronze);
    expect(sanitizeRules(null)).toEqual(DEFAULT_RULES);
  });

  it('avisa antes de publicar lo que no tiene sentido', () => {
    expect(rulesProblems({ ...DEFAULT_RULES, rankBronze: 1200 }).join(' ')).toMatch(/Bronce/);
    expect(rulesProblems({ ...DEFAULT_RULES, xpWin: 10 }).join(' ')).toMatch(/Ganar/);
    expect(rulesDiff(DEFAULT_RULES, { ...DEFAULT_RULES, xpWin: 120, goalDiffMultiplier: true })).toEqual([
      'Premiar las goleadas: no → sí',
      'Por ganar: 100 → 120',
    ]);
  });

  it('al aplicarlas cambian la XP, los rangos y los niveles de la progresión', () => {
    const m = makeMatch({ white: ['a'], blue: ['b'], goals: 'WWWWW', mode: 'ranked' });
    const base = (snap: ReturnType<typeof computeProgression>) =>
      snap.byMatch.get(m.id)!.get('a')!.xpBreakdown.filter((l) => !l.label.startsWith('Logro')).reduce((s, l) => s + l.xp, 0);
    const before = computeProgression(['a', 'b'], [m], DEFAULT_PROGRESSION, { challenges: false });
    expect(base(before)).toBe(50 + 100 + 50);

    const rules = sanitizeRules({
      ...DEFAULT_RULES,
      xpComplete: 10,
      xpWin: 20,
      xpRankedWinBonus: 0,
      xpAchievementCommon: 0,
      xpAchievementRare: 0,
      xpAchievementEpic: 0,
      rankWood: 1205,
      levelBase: 20,
      levelExponent: 1,
    });
    applyGameRules(rules);
    const after = computeProgression(['a', 'b'], [m], { ...DEFAULT_PROGRESSION, ...eloSettings(rules) }, { challenges: false });
    const a = after.players.get('a')!;
    expect(base(after)).toBe(30);
    expect(a.xp).toBe(30);
    expect(a.level).toBe(1);
    expect(a.elo).toBe(1200 + eloGain(DEFAULT_RULES.kProvisional, 0));
    expect(a.category.name).toBe('Madera');
  });

  it('se guardan en el aparato y se leen del panel', async () => {
    const mem = new Map<string, string>();
    const store = { getItem: (k: string) => mem.get(k) ?? null, setItem: (k: string, v: string) => void mem.set(k, v) };
    expect(readCachedRules(store)).toBeNull();
    writeCachedRules(store, { rules: { ...DEFAULT_RULES, xpWin: 120 }, updatedAt: 5 });
    expect(readCachedRules(store)).toEqual({ rules: { ...DEFAULT_RULES, xpWin: 120 }, updatedAt: 5 });

    const http = (async () => new Response(JSON.stringify([{ data: { xpLoss: 40 }, updated_at: 9 }]))) as typeof fetch;
    expect(await fetchRemoteRules('https://x.supabase.co/', 'k', http)).toEqual({ rules: { ...DEFAULT_RULES, xpLoss: 40 }, updatedAt: 9 });
    const none = (async () => new Response('[]')) as typeof fetch;
    expect(await fetchRemoteRules('https://x.supabase.co', 'k', none)).toBeNull();
  });
});
