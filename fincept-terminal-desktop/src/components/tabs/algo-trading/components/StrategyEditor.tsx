import React, { useState, useEffect, useCallback } from 'react';
import {
  Save, Play, Loader, ArrowLeft, Shield, AlertTriangle, ChevronDown, ChevronRight,
  Target, TrendingUp, BarChart3, Clock, Percent, Activity,
  ArrowUpRight, ArrowDownRight, Hash, Crosshair, Gauge, Zap, Plus, Trash2, Copy,
} from 'lucide-react';
import type { ConditionItem } from '../types';
import { TIMEFRAMES, getIndicatorDef } from '../constants/indicators';
import { saveAlgoStrategy, getAlgoStrategy, runAlgoBacktest } from '../services/algoTradingService';
import ConditionRow from './ConditionRow';
import { F } from '../constants/theme';

interface StrategyEditorProps {
  editStrategyId: string | null;
  onSaved: () => void;
  onCancel: () => void;
}

interface BacktestMetrics {
  total_trades: number; winning_trades: number; losing_trades: number;
  win_rate: number; total_return: number; total_return_pct: number;
  max_drawdown: number; avg_pnl: number; avg_bars_held: number;
  profit_factor: number; sharpe: number;
}

interface BacktestResult {
  trades: Array<{ entry_price: number; exit_price: number; pnl: number; pnl_pct: number; reason: string; bars_held: number }>;
  metrics: BacktestMetrics;
}

/* ── Inline style helpers per UI_DESIGN_SYSTEM.md ── */
const font = '"IBM Plex Mono", "Consolas", monospace';
const lbl: React.CSSProperties = { fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', textTransform: 'uppercase' };
const inputStyle: React.CSSProperties = { padding: '6px 8px', backgroundColor: F.DARK_BG, color: F.WHITE, borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER, borderRadius: '2px', fontSize: '10px', fontFamily: font, outline: 'none' };
const selectStyle: React.CSSProperties = { ...inputStyle, cursor: 'pointer' };
const btnPrimary: React.CSSProperties = { padding: '6px 14px', backgroundColor: F.ORANGE, color: F.DARK_BG, borderWidth: 0, borderStyle: 'none', borderColor: 'transparent', borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '5px', letterSpacing: '0.5px', fontFamily: font };
const btnOutline: React.CSSProperties = { padding: '5px 10px', backgroundColor: 'transparent', borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER, color: F.GRAY, fontSize: '9px', fontWeight: 700, borderRadius: '2px', cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px', transition: 'all 0.15s', fontFamily: font };
const badge = (bg: string, fg: string): React.CSSProperties => ({ padding: '2px 6px', backgroundColor: bg, color: fg, fontSize: '8px', fontWeight: 700, borderRadius: '2px', letterSpacing: '0.5px', display: 'inline-flex', alignItems: 'center', gap: '3px' });

const DEFAULT_ENTRY: ConditionItem = { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: 'crosses_below', value: 30 };
const DEFAULT_EXIT: ConditionItem = { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: 'crosses_above', value: 70 };

const StrategyEditor: React.FC<StrategyEditorProps> = ({ editStrategyId, onSaved, onCancel }) => {
  const [name, setName] = useState('');
  const [description, setDescription] = useState('');
  const [timeframe, setTimeframe] = useState('5m');
  const [entryConditions, setEntryConditions] = useState<ConditionItem[]>([{ ...DEFAULT_ENTRY }]);
  const [exitConditions, setExitConditions] = useState<ConditionItem[]>([{ ...DEFAULT_EXIT }]);
  const [entryLogic, setEntryLogic] = useState<'AND' | 'OR'>('AND');
  const [exitLogic, setExitLogic] = useState<'AND' | 'OR'>('AND');
  const [stopLoss, setStopLoss] = useState<string>('');
  const [takeProfit, setTakeProfit] = useState<string>('');
  const [trailingStop, setTrailingStop] = useState<string>('');
  const [useExitConditions, setUseExitConditions] = useState(true);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState('');
  // Backtest
  const [btSymbol, setBtSymbol] = useState('');
  const [btPeriod, setBtPeriod] = useState('1y');
  const [btProvider, setBtProvider] = useState<'yfinance' | 'fyers'>('fyers');
  const [backtesting, setBacktesting] = useState(false);
  const [btResult, setBtResult] = useState<BacktestResult | null>(null);
  const [btError, setBtError] = useState('');
  const [btDebug, setBtDebug] = useState<string[]>([]);
  const [showDebug, setShowDebug] = useState(false);
  const [showBacktest, setShowBacktest] = useState(true);

  const loadStrategy = useCallback(async (id: string) => {
    const result = await getAlgoStrategy(id);
    if (result.success && result.data) {
      const s = result.data;
      setName(s.name); setDescription(s.description); setTimeframe(s.timeframe);
      setEntryLogic(s.entry_logic); setExitLogic(s.exit_logic);
      setStopLoss(s.stop_loss != null ? String(s.stop_loss) : '');
      setTakeProfit(s.take_profit != null ? String(s.take_profit) : '');
      setTrailingStop(s.trailing_stop != null ? String(s.trailing_stop) : '');
      try {
        setEntryConditions(JSON.parse(s.entry_conditions).filter((c: unknown) => typeof c === 'object'));
        setExitConditions(JSON.parse(s.exit_conditions).filter((c: unknown) => typeof c === 'object'));
      } catch { /* keep defaults */ }
    }
  }, []);

  useEffect(() => { if (editStrategyId) loadStrategy(editStrategyId); }, [editStrategyId, loadStrategy]);

  const buildConditionsJson = (conditions: ConditionItem[], logic: string): string => {
    if (conditions.length === 0) return '[]';
    const mixed: (ConditionItem | string)[] = [];
    conditions.forEach((c, i) => { if (i > 0) mixed.push(logic); mixed.push(c); });
    return JSON.stringify(mixed);
  };

  const handleSave = async () => {
    if (!name.trim()) { setError('Strategy name is required'); return; }
    if (entryConditions.length === 0) { setError('At least one entry condition is required'); return; }
    setSaving(true); setError('');
    const id = editStrategyId || `algo-${crypto.randomUUID()}`;
    const result = await saveAlgoStrategy({
      id, name: name.trim(), description: description.trim(),
      entry_conditions: buildConditionsJson(entryConditions, entryLogic),
      exit_conditions: buildConditionsJson(exitConditions, exitLogic),
      entry_logic: entryLogic, exit_logic: exitLogic,
      stop_loss: stopLoss ? parseFloat(stopLoss) : null,
      take_profit: takeProfit ? parseFloat(takeProfit) : null,
      trailing_stop: trailingStop ? parseFloat(trailingStop) : null,
      timeframe,
    });
    setSaving(false);
    if (result.success) onSaved(); else setError(result.error || 'Failed to save');
  };

  const handleBacktest = async () => {
    const symbolHint = btProvider === 'fyers'
      ? 'Enter symbol (e.g. RELIANCE, TCS). Run a scan first to fetch data.'
      : 'Enter symbol (e.g. RELIANCE.NS for NSE, AAPL for US)';
    if (!btSymbol.trim()) { setBtError(symbolHint); return; }
    if (entryConditions.length === 0) { setBtError('Add at least one entry condition'); return; }
    setBacktesting(true); setBtError(''); setBtResult(null); setBtDebug([]);
    const res = await runAlgoBacktest({
      symbol: btSymbol.trim(),
      entryConditions: buildConditionsJson(entryConditions, entryLogic),
      exitConditions: useExitConditions ? buildConditionsJson(exitConditions, exitLogic) : '[]',
      timeframe: ['1m', '3m', '5m'].includes(timeframe) ? '1d' : timeframe,
      period: btPeriod,
      stopLoss: stopLoss ? parseFloat(stopLoss) : undefined,
      takeProfit: takeProfit ? parseFloat(takeProfit) : undefined,
      provider: btProvider,
    });
    setBacktesting(false);
    if (res.debug) setBtDebug(res.debug); else if (res.data?.debug) setBtDebug(res.data.debug);
    if (res.success && res.data) { setBtResult(res.data); if (res.data.debug) setBtDebug(res.data.debug); setShowBacktest(true); }
    else setBtError(res.error || 'Backtest failed');
  };

  const handleConditionChange = (
    setter: React.Dispatch<React.SetStateAction<ConditionItem[]>>,
    index: number,
    updated: ConditionItem,
  ) => {
    setter(prev => { const next = [...prev]; next[index] = updated; return next; });
  };

  const handleRemoveCondition = (
    setter: React.Dispatch<React.SetStateAction<ConditionItem[]>>,
    index: number,
  ) => {
    setter(prev => prev.filter((_, i) => i !== index));
  };

  const handleDuplicateCondition = (
    setter: React.Dispatch<React.SetStateAction<ConditionItem[]>>,
    index: number,
  ) => {
    setter(prev => {
      const clone = JSON.parse(JSON.stringify(prev[index]));
      const next = [...prev];
      next.splice(index + 1, 0, clone);
      return next;
    });
  };

  const rrRatio = stopLoss && takeProfit && parseFloat(stopLoss) > 0
    ? (parseFloat(takeProfit) / parseFloat(stopLoss)).toFixed(2)
    : null;

  /* ──────────────────────── RENDER ──────────────────────── */
  return (
    <div style={{ width: '100%', height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden', fontFamily: font, color: F.WHITE, backgroundColor: F.DARK_BG }}>

      {/* ═══ TOOLBAR ═══ */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '0', padding: '0 12px', height: '36px', flexShrink: 0, backgroundColor: F.PANEL_BG, borderBottom: `1px solid ${F.BORDER}` }}>
        <button onClick={onCancel} style={{ background: 'none', border: 'none', cursor: 'pointer', color: F.GRAY, padding: '4px', display: 'flex', alignItems: 'center', marginRight: '8px', transition: 'color 0.15s' }}
          onMouseEnter={e => { e.currentTarget.style.color = F.WHITE; }}
          onMouseLeave={e => { e.currentTarget.style.color = F.GRAY; }}
        ><ArrowLeft size={14} /></button>
        <input value={name} onChange={e => setName(e.target.value)} placeholder="Strategy Name..."
          style={{ background: 'transparent', border: 'none', outline: 'none', color: F.ORANGE, fontSize: '12px', fontWeight: 700, fontFamily: font, width: '200px', letterSpacing: '0.5px' }}
        />
        <div style={{ width: '1px', height: '18px', backgroundColor: F.BORDER, margin: '0 10px' }} />
        <span style={{ ...lbl, marginRight: '4px' }}>TF</span>
        <select value={timeframe} onChange={e => setTimeframe(e.target.value)} style={{ ...selectStyle, width: 'auto', padding: '3px 6px', fontSize: '9px', color: F.CYAN }}>
          {TIMEFRAMES.map(tf => <option key={tf.value} value={tf.value}>{tf.label}</option>)}
        </select>
        <div style={{ width: '1px', height: '18px', backgroundColor: F.BORDER, margin: '0 10px' }} />
        <input value={description} onChange={e => setDescription(e.target.value)} placeholder="Description (optional)"
          style={{ background: 'transparent', border: 'none', outline: 'none', color: F.GRAY, fontSize: '10px', fontFamily: font, flex: 1, minWidth: '100px' }}
        />
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', marginLeft: '8px' }}>
          <button onClick={onCancel} style={btnOutline}>CANCEL</button>
          <button onClick={handleSave} disabled={saving} style={{ ...btnPrimary, opacity: saving ? 0.5 : 1 }}>
            <Save size={10} />{saving ? 'SAVING...' : 'SAVE'}
          </button>
        </div>
      </div>

      {/* Error bar */}
      {error && (
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', padding: '5px 16px', backgroundColor: `${F.RED}15`, borderBottom: `1px solid ${F.RED}40`, flexShrink: 0 }}>
          <AlertTriangle size={11} color={F.RED} />
          <span style={{ fontSize: '10px', fontWeight: 700, color: F.RED }}>{error}</span>
        </div>
      )}

      {/* ═══ MAIN CONTENT — horizontal split: left=rules, right=backtest ═══ */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden', minHeight: 0 }}>

        {/* ──── LEFT: RULE SHEET (scrollable) ──── */}
        <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden', minWidth: 0 }}>

          {/* ─── ENTRY CONDITIONS ─── */}
          <div style={{ borderBottom: `1px solid ${F.BORDER}` }}>
            {/* Section header */}
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', padding: '8px 16px', backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}` }}>
              <ArrowUpRight size={12} style={{ color: F.GREEN }} />
              <span style={{ fontSize: '10px', fontWeight: 700, color: F.GREEN, letterSpacing: '0.5px' }}>ENTRY CONDITIONS</span>
              {entryConditions.length > 1 && (
                <div style={{ display: 'flex', alignItems: 'center', gap: '2px', marginLeft: '8px' }}>
                  {(['AND', 'OR'] as const).map(l => (
                    <button key={l} onClick={() => setEntryLogic(l)} style={{
                      padding: '2px 8px', border: 'none', borderRadius: '2px', cursor: 'pointer', fontFamily: font,
                      backgroundColor: entryLogic === l ? F.ORANGE : `${F.BORDER}`, color: entryLogic === l ? F.DARK_BG : F.GRAY,
                      fontSize: '8px', fontWeight: 700, transition: 'all 0.15s',
                    }}>{l}</button>
                  ))}
                </div>
              )}
              <div style={{ flex: 1 }} />
              <span style={badge(`${F.GREEN}20`, F.GREEN)}>{entryConditions.length} RULE{entryConditions.length !== 1 ? 'S' : ''}</span>
              <button
                onClick={() => setEntryConditions(prev => [...prev, { ...DEFAULT_ENTRY }])}
                style={{ ...btnOutline, padding: '3px 8px', fontSize: '8px', color: F.GREEN, borderColor: `${F.GREEN}40` }}
                onMouseEnter={e => { e.currentTarget.style.borderColor = F.GREEN; e.currentTarget.style.color = F.WHITE; }}
                onMouseLeave={e => { e.currentTarget.style.borderColor = `${F.GREEN}40`; e.currentTarget.style.color = F.GREEN; }}
              ><Plus size={10} /> ADD</button>
            </div>

            {/* Condition rows */}
            {entryConditions.map((cond, i) => (
              <div key={i} style={{ borderBottom: `1px solid ${F.BORDER}30` }}>
                {/* Row number + logic connector */}
                <div style={{ display: 'flex', alignItems: 'stretch' }}>
                  <div style={{ width: '40px', flexShrink: 0, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', backgroundColor: `${F.GREEN}08`, borderRight: `1px solid ${F.BORDER}30` }}>
                    {i === 0 ? (
                      <span style={{ fontSize: '9px', fontWeight: 700, color: F.GREEN }}>#{i + 1}</span>
                    ) : (
                      <>
                        <span style={{ fontSize: '7px', fontWeight: 700, color: F.ORANGE, marginBottom: '2px' }}>{entryLogic}</span>
                        <span style={{ fontSize: '9px', fontWeight: 700, color: F.GREEN }}>#{i + 1}</span>
                      </>
                    )}
                  </div>
                  <div style={{ flex: 1, minWidth: 0 }}>
                    <ConditionRow condition={cond} index={i}
                      onChange={(idx, updated) => handleConditionChange(setEntryConditions, idx, updated)}
                      onRemove={() => handleRemoveCondition(setEntryConditions, i)}
                    />
                  </div>
                  <div style={{ width: '36px', flexShrink: 0, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', gap: '4px', borderLeft: `1px solid ${F.BORDER}30` }}>
                    <button onClick={() => handleDuplicateCondition(setEntryConditions, i)}
                      style={{ background: 'none', border: 'none', cursor: 'pointer', color: F.MUTED, padding: '2px', transition: 'color 0.15s' }}
                      onMouseEnter={e => { e.currentTarget.style.color = F.CYAN; }}
                      onMouseLeave={e => { e.currentTarget.style.color = F.MUTED; }}
                      title="Duplicate"
                    ><Copy size={10} /></button>
                    {entryConditions.length > 1 && (
                      <button onClick={() => handleRemoveCondition(setEntryConditions, i)}
                        style={{ background: 'none', border: 'none', cursor: 'pointer', color: F.MUTED, padding: '2px', transition: 'color 0.15s' }}
                        onMouseEnter={e => { e.currentTarget.style.color = F.RED; }}
                        onMouseLeave={e => { e.currentTarget.style.color = F.MUTED; }}
                        title="Remove"
                      ><Trash2 size={10} /></button>
                    )}
                  </div>
                </div>
              </div>
            ))}
            {entryConditions.length === 0 && (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '20px', gap: '8px' }}>
                <Zap size={14} style={{ color: F.MUTED, opacity: 0.5 }} />
                <span style={{ fontSize: '10px', color: F.MUTED }}>No entry rules — click ADD above</span>
              </div>
            )}
          </div>

          {/* ─── EXIT CONDITIONS ─── */}
          <div style={{ borderBottom: `1px solid ${F.BORDER}` }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', padding: '8px 16px', backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}` }}>
              <ArrowDownRight size={12} style={{ color: useExitConditions ? F.RED : F.MUTED }} />
              <span style={{ fontSize: '10px', fontWeight: 700, color: useExitConditions ? F.RED : F.MUTED, letterSpacing: '0.5px' }}>EXIT CONDITIONS</span>
              {exitConditions.length > 1 && useExitConditions && (
                <div style={{ display: 'flex', alignItems: 'center', gap: '2px', marginLeft: '8px' }}>
                  {(['AND', 'OR'] as const).map(l => (
                    <button key={l} onClick={() => setExitLogic(l)} style={{
                      padding: '2px 8px', border: 'none', borderRadius: '2px', cursor: 'pointer', fontFamily: font,
                      backgroundColor: exitLogic === l ? F.ORANGE : `${F.BORDER}`, color: exitLogic === l ? F.DARK_BG : F.GRAY,
                      fontSize: '8px', fontWeight: 700, transition: 'all 0.15s',
                    }}>{l}</button>
                  ))}
                </div>
              )}
              <div style={{ flex: 1 }} />
              <label style={{ display: 'flex', alignItems: 'center', gap: '5px', cursor: 'pointer' }}>
                <span style={{ fontSize: '8px', fontWeight: 700, color: useExitConditions ? F.GREEN : F.MUTED }}>{useExitConditions ? 'ON' : 'OFF'}</span>
                <input type="checkbox" checked={useExitConditions} onChange={e => setUseExitConditions(e.target.checked)} style={{ accentColor: F.ORANGE, cursor: 'pointer' }} />
              </label>
              {useExitConditions && (
                <>
                  <span style={badge(`${F.RED}20`, F.RED)}>{exitConditions.length} RULE{exitConditions.length !== 1 ? 'S' : ''}</span>
                  <button
                    onClick={() => setExitConditions(prev => [...prev, { ...DEFAULT_EXIT }])}
                    style={{ ...btnOutline, padding: '3px 8px', fontSize: '8px', color: F.RED, borderColor: `${F.RED}40` }}
                    onMouseEnter={e => { e.currentTarget.style.borderColor = F.RED; e.currentTarget.style.color = F.WHITE; }}
                    onMouseLeave={e => { e.currentTarget.style.borderColor = `${F.RED}40`; e.currentTarget.style.color = F.RED; }}
                  ><Plus size={10} /> ADD</button>
                </>
              )}
            </div>

            {useExitConditions ? (
              <>
                {exitConditions.map((cond, i) => (
                  <div key={i} style={{ borderBottom: `1px solid ${F.BORDER}30` }}>
                    <div style={{ display: 'flex', alignItems: 'stretch' }}>
                      <div style={{ width: '40px', flexShrink: 0, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', backgroundColor: `${F.RED}08`, borderRight: `1px solid ${F.BORDER}30` }}>
                        {i === 0 ? (
                          <span style={{ fontSize: '9px', fontWeight: 700, color: F.RED }}>#{i + 1}</span>
                        ) : (
                          <>
                            <span style={{ fontSize: '7px', fontWeight: 700, color: F.ORANGE, marginBottom: '2px' }}>{exitLogic}</span>
                            <span style={{ fontSize: '9px', fontWeight: 700, color: F.RED }}>#{i + 1}</span>
                          </>
                        )}
                      </div>
                      <div style={{ flex: 1, minWidth: 0 }}>
                        <ConditionRow condition={cond} index={i}
                          onChange={(idx, updated) => handleConditionChange(setExitConditions, idx, updated)}
                          onRemove={() => handleRemoveCondition(setExitConditions, i)}
                        />
                      </div>
                      <div style={{ width: '36px', flexShrink: 0, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', gap: '4px', borderLeft: `1px solid ${F.BORDER}30` }}>
                        <button onClick={() => handleDuplicateCondition(setExitConditions, i)}
                          style={{ background: 'none', border: 'none', cursor: 'pointer', color: F.MUTED, padding: '2px', transition: 'color 0.15s' }}
                          onMouseEnter={e => { e.currentTarget.style.color = F.CYAN; }}
                          onMouseLeave={e => { e.currentTarget.style.color = F.MUTED; }}
                          title="Duplicate"
                        ><Copy size={10} /></button>
                        {exitConditions.length > 1 && (
                          <button onClick={() => handleRemoveCondition(setExitConditions, i)}
                            style={{ background: 'none', border: 'none', cursor: 'pointer', color: F.MUTED, padding: '2px', transition: 'color 0.15s' }}
                            onMouseEnter={e => { e.currentTarget.style.color = F.RED; }}
                            onMouseLeave={e => { e.currentTarget.style.color = F.MUTED; }}
                            title="Remove"
                          ><Trash2 size={10} /></button>
                        )}
                      </div>
                    </div>
                  </div>
                ))}
                {exitConditions.length === 0 && (
                  <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '20px', gap: '8px' }}>
                    <Zap size={14} style={{ color: F.MUTED, opacity: 0.5 }} />
                    <span style={{ fontSize: '10px', color: F.MUTED }}>No exit rules — click ADD above</span>
                  </div>
                )}
              </>
            ) : (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', padding: '16px', gap: '8px' }}>
                <Shield size={12} style={{ color: F.MUTED }} />
                <span style={{ fontSize: '10px', color: F.MUTED }}>Exit conditions disabled — using SL/TP only</span>
              </div>
            )}
          </div>

          {/* ─── RISK MANAGEMENT ─── */}
          <div style={{ borderBottom: `1px solid ${F.BORDER}` }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px', padding: '8px 16px', backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}` }}>
              <Shield size={12} style={{ color: F.YELLOW }} />
              <span style={{ fontSize: '10px', fontWeight: 700, color: F.YELLOW, letterSpacing: '0.5px' }}>RISK MANAGEMENT</span>
              <div style={{ flex: 1 }} />
              {rrRatio && (
                <span style={{ fontSize: '9px', color: parseFloat(rrRatio) >= 1 ? F.GREEN : F.RED }}>
                  R:R <span style={{ fontWeight: 700 }}>1:{rrRatio}</span>
                </span>
              )}
              {stopLoss && takeProfit && parseFloat(stopLoss) >= parseFloat(takeProfit) && (
                <span style={{ ...badge(`${F.YELLOW}20`, F.YELLOW) }}><AlertTriangle size={8} /> SL ≥ TP</span>
              )}
            </div>
            <div style={{ display: 'flex', gap: '0', alignItems: 'stretch' }}>
              {/* Stop Loss */}
              <div style={{ flex: 1, padding: '10px 16px', borderRight: `1px solid ${F.BORDER}` }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '5px', marginBottom: '6px' }}>
                  <Target size={10} style={{ color: F.RED }} />
                  <span style={{ ...lbl, color: F.RED }}>STOP LOSS %</span>
                </div>
                <input type="number" value={stopLoss} onChange={e => setStopLoss(e.target.value)} placeholder="0.0" step="0.1" min="0"
                  style={{ ...inputStyle, width: '100%', borderColor: stopLoss ? `${F.RED}40` : F.BORDER }}
                  onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
                  onBlur={e => { e.currentTarget.style.borderColor = stopLoss ? `${F.RED}40` : F.BORDER; }}
                />
                <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '3px' }}>Exit if loss ≥ this %</div>
              </div>
              {/* Take Profit */}
              <div style={{ flex: 1, padding: '10px 16px', borderRight: `1px solid ${F.BORDER}` }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '5px', marginBottom: '6px' }}>
                  <TrendingUp size={10} style={{ color: F.GREEN }} />
                  <span style={{ ...lbl, color: F.GREEN }}>TAKE PROFIT %</span>
                </div>
                <input type="number" value={takeProfit} onChange={e => setTakeProfit(e.target.value)} placeholder="0.0" step="0.1" min="0"
                  style={{ ...inputStyle, width: '100%', borderColor: takeProfit ? `${F.GREEN}40` : F.BORDER }}
                  onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
                  onBlur={e => { e.currentTarget.style.borderColor = takeProfit ? `${F.GREEN}40` : F.BORDER; }}
                />
                <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '3px' }}>Exit if profit ≥ this %</div>
              </div>
              {/* Trailing Stop */}
              <div style={{ flex: 1, padding: '10px 16px' }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: '5px', marginBottom: '6px' }}>
                  <Gauge size={10} style={{ color: F.YELLOW }} />
                  <span style={{ ...lbl, color: F.YELLOW }}>TRAILING STOP %</span>
                </div>
                <input type="number" value={trailingStop} onChange={e => setTrailingStop(e.target.value)} placeholder="0.0" step="0.1" min="0"
                  style={{ ...inputStyle, width: '100%', borderColor: trailingStop ? `${F.YELLOW}40` : F.BORDER }}
                  onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
                  onBlur={e => { e.currentTarget.style.borderColor = trailingStop ? `${F.YELLOW}40` : F.BORDER; }}
                />
                <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '3px' }}>Pullback from peak ≥ this %</div>
              </div>
            </div>
          </div>
        </div>

        {/* ──── RIGHT: BACKTEST PANEL (320px, collapsible) ──── */}
        <div style={{ width: showBacktest ? '340px' : '44px', flexShrink: 0, display: 'flex', flexDirection: 'column', overflow: 'hidden', borderLeft: `1px solid ${F.BORDER}`, transition: 'width 0.2s ease' }}>
          {/* Backtest toggle header */}
          <div
            onClick={() => setShowBacktest(!showBacktest)}
            style={{ display: 'flex', alignItems: 'center', gap: '6px', padding: '8px 12px', backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}`, cursor: 'pointer', flexShrink: 0 }}
          >
            <Crosshair size={12} style={{ color: F.PURPLE }} />
            {showBacktest && (
              <>
                <span style={{ fontSize: '9px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px', flex: 1 }}>BACKTEST</span>
                {btResult && (
                  <span style={{ fontSize: '13px', fontWeight: 700, fontFamily: font, color: btResult.metrics.total_return_pct >= 0 ? F.GREEN : F.RED }}>
                    {btResult.metrics.total_return_pct >= 0 ? '+' : ''}{btResult.metrics.total_return_pct}%
                  </span>
                )}
              </>
            )}
          </div>

          {showBacktest && (
            <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden' }}>
              {/* Config */}
              <div style={{ padding: '10px 12px', borderBottom: `1px solid ${F.BORDER}` }}>
                <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px', marginBottom: '6px' }}>
                  <div>
                    <span style={{ ...lbl, display: 'block', marginBottom: '3px', fontSize: '8px' }}>SOURCE</span>
                    <select value={btProvider} onChange={e => setBtProvider(e.target.value as 'yfinance' | 'fyers')} style={{ ...selectStyle, width: '100%', padding: '5px 6px' }}>
                      <option value="fyers">Fyers</option><option value="yfinance">Yahoo</option>
                    </select>
                  </div>
                  <div>
                    <span style={{ ...lbl, display: 'block', marginBottom: '3px', fontSize: '8px' }}>PERIOD</span>
                    <select value={btPeriod} onChange={e => setBtPeriod(e.target.value)} style={{ ...selectStyle, width: '100%', padding: '5px 6px' }}>
                      {[['1mo', '1M'], ['3mo', '3M'], ['6mo', '6M'], ['1y', '1Y'], ['2y', '2Y'], ['5y', '5Y']].map(([v, l]) => (
                        <option key={v} value={v}>{l}</option>
                      ))}
                    </select>
                  </div>
                </div>
                <div style={{ display: 'flex', gap: '6px' }}>
                  <input value={btSymbol} onChange={e => setBtSymbol(e.target.value)}
                    placeholder={btProvider === 'fyers' ? 'RELIANCE' : 'AAPL'}
                    style={{ ...inputStyle, flex: 1, padding: '5px 8px' }}
                    onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
                    onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
                  />
                  <button onClick={handleBacktest} disabled={backtesting}
                    style={{ ...btnPrimary, backgroundColor: F.PURPLE, color: F.WHITE, padding: '5px 12px', opacity: backtesting ? 0.5 : 1, whiteSpace: 'nowrap' }}
                  >
                    {backtesting ? <Loader size={11} className="animate-spin" /> : <Play size={11} />}
                    {backtesting ? 'RUN...' : 'RUN'}
                  </button>
                </div>
              </div>

              {/* BT Error */}
              {btError && (
                <div style={{ padding: '6px 12px', backgroundColor: `${F.RED}15`, borderBottom: `1px solid ${F.RED}30` }}>
                  <span style={{ fontSize: '9px', color: F.RED }}>{btError}</span>
                </div>
              )}

              {/* Debug */}
              {btDebug.length > 0 && (
                <div style={{ padding: '5px 12px', borderBottom: `1px solid ${F.BORDER}` }}>
                  <button onClick={() => setShowDebug(!showDebug)} style={{ background: 'none', border: 'none', cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'space-between', width: '100%', fontFamily: font, color: F.MUTED, fontSize: '8px', fontWeight: 700 }}>
                    <span>DEBUG ({btDebug.length})</span>
                    {showDebug ? <ChevronDown size={9} /> : <ChevronRight size={9} />}
                  </button>
                  {showDebug && (
                    <div style={{ marginTop: '4px', padding: '4px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', maxHeight: '100px', overflowY: 'auto', fontSize: '8px', lineHeight: 1.4 }}>
                      {btDebug.map((line, i) => (
                        <div key={i} style={{ wordBreak: 'break-all', color: line.includes('ERROR') ? F.RED : line.includes('WARNING') ? F.YELLOW : F.MUTED }}>{line}</div>
                      ))}
                    </div>
                  )}
                </div>
              )}

              {/* Results */}
              {btResult && (
                <>
                  <div style={{ padding: '10px 12px', borderBottom: `1px solid ${F.BORDER}` }}>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '5px' }}>
                      {[
                        { l: 'WIN RATE', v: `${btResult.metrics.win_rate}%`, c: btResult.metrics.win_rate >= 50 ? F.GREEN : F.YELLOW, ic: Target },
                        { l: 'DRAWDOWN', v: `${btResult.metrics.max_drawdown}%`, c: F.RED, ic: ArrowDownRight },
                        { l: 'SHARPE', v: btResult.metrics.sharpe.toFixed(2), c: btResult.metrics.sharpe >= 1 ? F.GREEN : F.YELLOW, ic: Percent },
                        { l: 'PF', v: btResult.metrics.profit_factor.toFixed(2), c: btResult.metrics.profit_factor >= 1.5 ? F.GREEN : F.YELLOW, ic: Activity },
                        { l: 'TRADES', v: String(btResult.metrics.total_trades), c: F.CYAN, ic: Hash },
                        { l: 'AVG P&L', v: btResult.metrics.avg_pnl.toFixed(2), c: btResult.metrics.avg_pnl >= 0 ? F.GREEN : F.RED, ic: TrendingUp },
                        { l: 'AVG BARS', v: btResult.metrics.avg_bars_held.toFixed(1), c: F.CYAN, ic: Clock },
                        { l: 'W / L', v: `${btResult.metrics.winning_trades}/${btResult.metrics.losing_trades}`, c: F.WHITE, ic: BarChart3 },
                      ].map((m, i) => (
                        <div key={i} style={{ padding: '6px', backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px' }}>
                          <div style={{ display: 'flex', alignItems: 'center', gap: '3px', marginBottom: '3px' }}>
                            <m.ic size={8} style={{ color: F.GRAY }} />
                            <span style={{ fontSize: '7px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>{m.l}</span>
                          </div>
                          <div style={{ fontSize: '13px', fontWeight: 700, fontFamily: font, color: m.c }}>{m.v}</div>
                        </div>
                      ))}
                    </div>
                  </div>

                  {btResult.trades.length > 0 && (
                    <div>
                      <div style={{ padding: '5px 12px', backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}` }}>
                        <span style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>TRADES ({btResult.trades.length})</span>
                      </div>
                      <div style={{ maxHeight: '200px', overflowY: 'auto' }}>
                        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                          <thead>
                            <tr>
                              {['#', 'ENTRY', 'EXIT', 'P&L%', 'BARS'].map(h => (
                                <th key={h} style={{ padding: '4px 6px', fontSize: '7px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', textAlign: h === '#' ? 'left' : 'right', backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}`, position: 'sticky', top: 0 }}>{h}</th>
                              ))}
                            </tr>
                          </thead>
                          <tbody>
                            {btResult.trades.map((t, i) => (
                              <tr key={i} style={{ borderBottom: `1px solid ${F.BORDER}30` }}
                                onMouseEnter={e => { e.currentTarget.style.backgroundColor = F.HOVER; }}
                                onMouseLeave={e => { e.currentTarget.style.backgroundColor = 'transparent'; }}
                              >
                                <td style={{ padding: '3px 6px', fontSize: '9px', fontFamily: font, color: F.MUTED }}>{i + 1}</td>
                                <td style={{ padding: '3px 6px', fontSize: '9px', fontFamily: font, color: F.WHITE, textAlign: 'right' }}>{t.entry_price}</td>
                                <td style={{ padding: '3px 6px', fontSize: '9px', fontFamily: font, color: F.WHITE, textAlign: 'right' }}>{t.exit_price}</td>
                                <td style={{ padding: '3px 6px', fontSize: '9px', fontFamily: font, fontWeight: 700, color: t.pnl_pct >= 0 ? F.GREEN : F.RED, textAlign: 'right' }}>{t.pnl_pct >= 0 ? '+' : ''}{t.pnl_pct}%</td>
                                <td style={{ padding: '3px 6px', fontSize: '9px', fontFamily: font, color: F.CYAN, textAlign: 'right' }}>{t.bars_held}</td>
                              </tr>
                            ))}
                          </tbody>
                        </table>
                      </div>
                    </div>
                  )}
                </>
              )}

              {!btResult && !backtesting && !btError && (
                <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', padding: '32px 12px', color: F.MUTED, fontSize: '10px', textAlign: 'center' }}>
                  <Crosshair size={20} style={{ marginBottom: '6px', opacity: 0.5 }} />
                  <span>Enter symbol & click RUN</span>
                </div>
              )}
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default StrategyEditor;
