/**
 * Lo que manda en cada mesa: el panel de administración da los valores por defecto de los partidos sueltos y del
 * sonido, y cada mesa puede cambiarlos (queda apuntado en `overrides`). La pantalla y la reconexión de la placa las
 * fija siempre el panel.
 */
import type { GameRules } from './progression/gameRules';
import type { OverridableKey, Preferences } from './persistence/types';

export const OVERRIDABLE_KEYS: OverridableKey[] = [
  'defaultEndCondition',
  'defaultGoalsPerPeriod',
  'defaultMinutesPerPeriod',
  'penaltyFirstTeam',
  'volume',
  'goalSound',
  'voice',
  'effects',
];

/** Valor del panel para cada campo que la mesa puede cambiar. */
export function panelValues(rules: GameRules): Pick<Preferences, OverridableKey> {
  return {
    defaultEndCondition: rules.matchEndCondition,
    defaultGoalsPerPeriod: rules.matchGoals,
    defaultMinutesPerPeriod: rules.matchMinutes,
    penaltyFirstTeam: rules.penaltyFirstTeam,
    volume: Math.max(0, Math.min(1, rules.volume / 100)),
    goalSound: rules.goalSound,
    voice: rules.voice,
    effects: rules.effects,
  };
}

/** Preferencias en uso: lo de la mesa donde lo ha cambiado, lo del panel en lo demás, y lo forzado por el panel. */
export function applyPanelPrefs(prefs: Preferences, rules: GameRules): Preferences {
  const panel = panelValues(rules);
  const own = new Set(prefs.overrides ?? []);
  const out: Preferences = { ...prefs };
  for (const k of OVERRIDABLE_KEYS) if (!own.has(k)) (out as unknown as Record<string, unknown>)[k] = panel[k];
  out.keepAwake = rules.keepAwake;
  out.sleepMinutes = rules.sleepMinutes;
  out.hardware = { ...prefs.hardware, autoConnect: rules.autoConnect };
  return out;
}

/** Al guardar: los campos cambiados respecto a lo que se veía pasan a ser de la mesa. */
export function markOverrides(shown: Preferences, next: Preferences): Preferences {
  const own = new Set(next.overrides ?? shown.overrides ?? []);
  for (const k of OVERRIDABLE_KEYS) if (next[k] !== shown[k]) own.add(k);
  return { ...next, overrides: OVERRIDABLE_KEYS.filter((k) => own.has(k)) };
}

/** Quita los cambios de la mesa en esos campos: vuelven a lo del panel. */
export function resetToPanel(prefs: Preferences, keys: OverridableKey[]): Preferences {
  return { ...prefs, overrides: (prefs.overrides ?? []).filter((k) => !keys.includes(k)) };
}
