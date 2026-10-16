import { describe, expect, it } from 'vitest';
import { applyPanelPrefs, markOverrides, resetToPanel } from '../src/services/panelPrefs';
import { DEFAULT_PREFERENCES, normalizePreferences } from '../src/services/persistence';
import { DEFAULT_RULES } from '../src/services/progression';

const rules = { ...DEFAULT_RULES, matchGoals: 7, volume: 40, keepAwake: false, sleepMinutes: 12, autoConnect: false };

describe('valores del panel y cambios de cada mesa', () => {
  it('sin cambios en la mesa manda el panel', () => {
    const p = applyPanelPrefs(normalizePreferences({ defaultGoalsPerPeriod: 3 }), rules);
    expect(p.defaultGoalsPerPeriod).toBe(7);
    expect(p.volume).toBeCloseTo(0.4);
  });

  it('lo que cambia la mesa se queda suyo y se puede devolver al panel', () => {
    const shown = applyPanelPrefs(DEFAULT_PREFERENCES, rules);
    const stored = markOverrides(shown, { ...shown, defaultGoalsPerPeriod: 9 });
    expect(stored.overrides).toEqual(['defaultGoalsPerPeriod']);
    expect(applyPanelPrefs(stored, rules).defaultGoalsPerPeriod).toBe(9);
    expect(applyPanelPrefs(stored, rules).volume).toBeCloseTo(0.4);
    const back = resetToPanel(stored, ['defaultGoalsPerPeriod']);
    expect(applyPanelPrefs(back, rules).defaultGoalsPerPeriod).toBe(7);
  });

  it('pantalla y placa las fija siempre el panel', () => {
    const p = applyPanelPrefs({ ...DEFAULT_PREFERENCES, keepAwake: true, sleepMinutes: 1, hardware: { wsUrl: 'x', autoConnect: true } }, rules);
    expect(p.keepAwake).toBe(false);
    expect(p.sleepMinutes).toBe(12);
    expect(p.hardware).toEqual({ wsUrl: 'x', autoConnect: false });
  });
});
