import React, { useState, useEffect } from 'react';
import {
  Save, Play, Loader, ArrowLeft, Shield, AlertTriangle, ChevronDown, ChevronRight,
  Target, TrendingUp, TrendingDown, BarChart3, Clock, Percent, Activity,
  ArrowUpRight, ArrowDownRight, Hash,
} from 'lucide-react';
import type { ConditionItem } from '../types';
import { TIMEFRAMES } from '../constants/indicators';
import { saveAlgoStrategy, getAlgoStrategy, runAlgoBacktest } from '../services/algoTradingService';
import ConditionBuilder from './ConditionBuilder';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A', BORDER: '#2A2A2A', HOVER: '#1F1F1F',
  MUTED: '#4A4A4A', CYAN: '#00E5FF', YELLOW: '#FFD700', PURPLE: '#9D4EDD',
  BLUE: '#0088FF',
};

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

const inputStyle: React.CSSProperties = {
  width: '100%', padding: '8px 10px', backgroundColor: F.DARK_BG,
  color: F.WHITE, border: `1px solid ${F.BORDER}`, borderRadius: '2px',
  fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', outline: 'none',
};

const labelStyle: React.CSSProperties = {
  fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px',
  marginBottom: '4px', display: 'block',
};

const sectionStyle: React.CSSProperties = {
  backgroundColor: F.PANEL_BG,
  border: `1px solid ${F.BORDER}`,
  borderRadius: '2px',
};

const sectionHeaderStyle: React.CSSProperties = {
  padding: '10px 16px',
  backgroundColor: F.HEADER_BG,
  borderBottom: `1px solid ${F.BORDER}`,
  display: 'flex',
  alignItems: 'center',
  gap: '8px',
};

const StrategyEditor: React.FC<StrategyEditorProps> = ({ editStrategyId, onSaved, onCancel }) => {
  const [name, setName] = useState('');
  const [description, setDescription] = useState('');
  const [timeframe, setTimeframe] = useState('5m');
  const [entryConditions, setEntryConditions] = useState<ConditionItem[]>([
    { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: 'crosses_below', value: 30 },
  ]);
  const [exitConditions, setExitConditions] = useState<ConditionItem[]>([
    { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: 'crosses_above', value: 70 },
  ]);
  const [entryLogic, setEntryLogic] = useState<'AND' | 'OR'>('AND');
  const [exitLogic, setExitLogic] = useState<'AND' | 'OR'>('AND');
  const [stopLoss, setStopLoss] = useState<string>('');
  const [takeProfit, setTakeProfit] = useState<string>('');
  const [trailingStop, setTrailingStop] = useState<string>('');
  const [useExitConditions, setUseExitConditions] = useState(true);
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState('');
  const [btSymbol, setBtSymbol] = useState('');
  const [btPeriod, setBtPeriod] = useState('1y');
  const [btProvider, setBtProvider] = useState<'yfinance' | 'fyers'>('fyers');
  const [backtesting, setBacktesting] = useState(false);
  const [btResult, setBtResult] = useState<BacktestResult | null>(null);
  const [btError, setBtError] = useState('');
  const [btDebug, setBtDebug] = useState<string[]>([]);
  const [showDebug, setShowDebug] = useState(false);
  const [showBacktest, setShowBacktest] = useState(true);

  useEffect(() => {
    if (editStrategyId) loadStrategy(editStrategyId);
  }, [editStrategyId]);

  const loadStrategy = async (id: string) => {
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
  };

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
    const id = editStrategyId || `algo-${Date.now()}`;
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
    if (result.success) onSaved();
    else setError(result.error || 'Failed to save');
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
      timeframe: ['1m','3m','5m'].includes(timeframe) ? '1d' : timeframe,
      period: btPeriod,
      stopLoss: stopLoss ? parseFloat(stopLoss) : undefined,
      takeProfit: takeProfit ? parseFloat(takeProfit) : undefined,
      provider: btProvider,
    });
    setBacktesting(false);

    if (res.debug) {
      setBtDebug(res.debug);
    } else if (res.data?.debug) {
      setBtDebug(res.data.debug);
    }

    if (res.success && res.data) {
      setBtResult(res.data);
      if (res.data.debug) setBtDebug(res.data.debug);
    } else {
      setBtError(res.error || 'Backtest failed');
    }
  };

  return (
    <div style={{
      width: '100%', height: '100%', display: 'flex', flexDirection: 'column',
      overflow: 'hidden', fontFamily: '"IBM Plex Mono", "Consolas", monospace',
    }}>

      {/* ═══════ TOOLBAR ═══════ */}
      <div style={{
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
        padding: '10px 20px', backgroundColor: F.HEADER_BG,
        borderBottom: `1px solid ${F.BORDER}`, flexShrink: 0,
      }}>
        {/* Left */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <button
            onClick={onCancel}
            style={{
              padding: '6px 8px', backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
              color: F.GRAY, cursor: 'pointer', borderRadius: '2px', display: 'flex', alignItems: 'center',
              transition: 'all 0.2s',
            }}
            onMouseEnter={e => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.color = F.WHITE; }}
            onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
          >
            <ArrowLeft size={12} />
          </button>
          <div>
            <div style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
              {editStrategyId ? 'EDIT STRATEGY' : 'CREATE NEW STRATEGY'}
            </div>
            <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>
              Visual condition-based strategy builder
            </div>
          </div>
        </div>

        {/* Center: Name + Timeframe */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <input
            value={name}
            onChange={e => setName(e.target.value)}
            placeholder="Strategy Name..."
            style={{
              width: '240px', padding: '7px 10px', backgroundColor: F.DARK_BG,
              color: F.WHITE, border: `1px solid ${F.BORDER}`, borderRadius: '2px',
              fontSize: '11px', fontWeight: 600, outline: 'none',
            }}
            onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
            onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
          />
          <select
            value={timeframe}
            onChange={e => setTimeframe(e.target.value)}
            style={{
              padding: '7px 8px', backgroundColor: F.DARK_BG, color: F.CYAN,
              border: `1px solid ${F.BORDER}`, borderRadius: '2px', fontSize: '10px',
              fontWeight: 700, cursor: 'pointer', outline: 'none',
            }}
          >
            {TIMEFRAMES.map(tf => <option key={tf.value} value={tf.value}>{tf.label}</option>)}
          </select>
        </div>

        {/* Right: Actions */}
        <div style={{ display: 'flex', gap: '6px' }}>
          <button
            onClick={onCancel}
            style={{
              padding: '7px 14px', backgroundColor: 'transparent', color: F.GRAY,
              border: `1px solid ${F.BORDER}`, borderRadius: '2px', fontSize: '9px',
              fontWeight: 700, cursor: 'pointer', transition: 'all 0.2s',
            }}
            onMouseEnter={e => { e.currentTarget.style.color = F.WHITE; e.currentTarget.style.borderColor = F.GRAY; }}
            onMouseLeave={e => { e.currentTarget.style.color = F.GRAY; e.currentTarget.style.borderColor = F.BORDER; }}
          >
            CANCEL
          </button>
          <button
            onClick={handleSave}
            disabled={saving}
            style={{
              padding: '7px 16px', backgroundColor: F.ORANGE, color: F.DARK_BG,
              border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
              cursor: saving ? 'not-allowed' : 'pointer', display: 'flex', alignItems: 'center',
              gap: '4px', opacity: saving ? 0.5 : 1, letterSpacing: '0.5px',
            }}
          >
            <Save size={10} />
            {saving ? 'SAVING...' : 'SAVE'}
          </button>
        </div>
      </div>

      {/* Error Banner */}
      {error && (
        <div style={{
          padding: '8px 20px', backgroundColor: `${F.RED}15`, borderBottom: `1px solid ${F.RED}40`,
          display: 'flex', alignItems: 'center', gap: '6px', flexShrink: 0,
        }}>
          <AlertTriangle size={10} color={F.RED} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: F.RED }}>{error}</span>
        </div>
      )}

      {/* ═══════ TWO-COLUMN BODY ═══════ */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden', minHeight: 0 }}>

        {/* ──── LEFT COLUMN: Strategy Builder ──── */}
        <div style={{
          flex: 1, overflow: 'auto', padding: '16px 20px',
          display: 'flex', flexDirection: 'column', gap: '16px', minWidth: 0,
        }}>
          {/* Description */}
          <div>
            <label style={labelStyle}>DESCRIPTION (OPTIONAL)</label>
            <input
              value={description}
              onChange={e => setDescription(e.target.value)}
              placeholder="Describe your strategy logic..."
              style={inputStyle}
            />
          </div>

          {/* ── ENTRY CONDITIONS SECTION ── */}
          <div style={sectionStyle}>
            <div style={{
              ...sectionHeaderStyle,
              borderLeft: `3px solid ${F.GREEN}`,
            }}>
              <ArrowUpRight size={14} color={F.GREEN} />
              <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
                ENTRY CONDITIONS
              </span>
              <span style={{ fontSize: '9px', color: F.GREEN, marginLeft: '4px' }}>BUY WHEN</span>
              <div style={{ flex: 1 }} />
              <div style={{
                padding: '3px 8px', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
                backgroundColor: `${F.GREEN}20`, color: F.GREEN,
              }}>
                {entryConditions.length} CONDITION{entryConditions.length !== 1 ? 'S' : ''}
              </div>
            </div>
            <div style={{ padding: '8px 0' }}>
              <ConditionBuilder
                label=""
                conditions={entryConditions}
                logic={entryLogic}
                onConditionsChange={setEntryConditions}
                onLogicChange={setEntryLogic}
              />
            </div>
          </div>

          {/* ── EXIT CONDITIONS SECTION ── */}
          <div style={sectionStyle}>
            <div style={{
              ...sectionHeaderStyle,
              borderLeft: `3px solid ${F.RED}`,
              justifyContent: 'space-between',
            }}>
              <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
                <ArrowDownRight size={14} color={F.RED} />
                <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
                  EXIT CONDITIONS
                </span>
                <span style={{ fontSize: '9px', color: F.RED, marginLeft: '4px' }}>SELL WHEN</span>
                {!useExitConditions && (
                  <span style={{
                    padding: '2px 6px', borderRadius: '2px', fontSize: '8px',
                    fontWeight: 700, backgroundColor: `${F.YELLOW}20`, color: F.YELLOW,
                  }}>
                    DISABLED
                  </span>
                )}
              </div>
              <label style={{ display: 'flex', alignItems: 'center', gap: '6px', cursor: 'pointer' }}>
                <span style={{ fontSize: '8px', color: useExitConditions ? F.GREEN : F.MUTED }}>
                  {useExitConditions ? 'ENABLED' : 'SL/TP ONLY'}
                </span>
                <input
                  type="checkbox"
                  checked={useExitConditions}
                  onChange={e => setUseExitConditions(e.target.checked)}
                  style={{ accentColor: F.ORANGE, cursor: 'pointer' }}
                />
              </label>
            </div>
            {useExitConditions ? (
              <div style={{ padding: '8px 0' }}>
                <ConditionBuilder
                  label=""
                  conditions={exitConditions}
                  logic={exitLogic}
                  onConditionsChange={setExitConditions}
                  onLogicChange={setExitLogic}
                />
              </div>
            ) : (
              <div style={{ padding: '24px 16px', textAlign: 'center' }}>
                <Shield size={20} color={F.MUTED} style={{ marginBottom: '6px' }} />
                <div style={{ fontSize: '9px', color: F.MUTED }}>
                  Exit conditions disabled. Trades exit via Stop Loss or Take Profit only.
                </div>
                <div style={{ fontSize: '8px', color: F.YELLOW, marginTop: '4px' }}>
                  Ensure SL and/or TP are configured in the Risk Management panel.
                </div>
              </div>
            )}
          </div>

          {/* ── RISK MANAGEMENT SECTION ── */}
          <div style={sectionStyle}>
            <div style={{
              ...sectionHeaderStyle,
              borderLeft: `3px solid ${F.YELLOW}`,
            }}>
              <Shield size={14} color={F.YELLOW} />
              <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
                RISK MANAGEMENT
              </span>
              <span style={{ fontSize: '8px', color: F.MUTED, marginLeft: '8px' }}>
                Priority: SL → TP → Exit Condition
              </span>
            </div>
            <div style={{ padding: '12px 16px' }}>
              <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
                <div>
                  <label style={labelStyle}>STOP LOSS (%)</label>
                  <input type="number" value={stopLoss} onChange={e => setStopLoss(e.target.value)} placeholder="e.g. 2" step="0.1" min="0" style={inputStyle} />
                  <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>Exit if loss &ge; this %</div>
                </div>
                <div>
                  <label style={labelStyle}>TAKE PROFIT (%)</label>
                  <input type="number" value={takeProfit} onChange={e => setTakeProfit(e.target.value)} placeholder="e.g. 5" step="0.1" min="0" style={inputStyle} />
                  <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>Exit if profit &ge; this %</div>
                </div>
                <div>
                  <label style={labelStyle}>TRAILING STOP (%)</label>
                  <input type="number" value={trailingStop} onChange={e => setTrailingStop(e.target.value)} placeholder="e.g. 1.5" step="0.1" min="0" style={inputStyle} />
                  <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>Not yet implemented</div>
                </div>
              </div>

              {/* Validation Warnings */}
              {stopLoss && takeProfit && parseFloat(stopLoss) >= parseFloat(takeProfit) && (
                <div style={{
                  marginTop: '12px', padding: '8px 10px',
                  backgroundColor: `${F.YELLOW}15`, border: `1px solid ${F.YELLOW}40`, borderRadius: '2px',
                  display: 'flex', alignItems: 'center', gap: '6px',
                }}>
                  <AlertTriangle size={10} color={F.YELLOW} />
                  <span style={{ fontSize: '9px', color: F.YELLOW }}>
                    Stop Loss ({stopLoss}%) &ge; Take Profit ({takeProfit}%). Adjust for better risk/reward.
                  </span>
                </div>
              )}
              {useExitConditions && exitConditions.length > 0 && takeProfit && (
                <div style={{
                  marginTop: '8px', padding: '8px 10px',
                  backgroundColor: `${F.CYAN}10`, border: `1px solid ${F.CYAN}30`, borderRadius: '2px',
                }}>
                  <span style={{ fontSize: '9px', color: F.CYAN }}>
                    Exit conditions trigger AFTER SL/TP checks. May exit before {takeProfit}% target.
                  </span>
                </div>
              )}
            </div>
          </div>
        </div>

        {/* ──── RIGHT COLUMN: Backtest Panel ──── */}
        <div style={{
          width: showBacktest ? '360px' : '40px',
          backgroundColor: F.PANEL_BG,
          borderLeft: `1px solid ${F.BORDER}`,
          display: 'flex',
          flexDirection: 'column',
          overflow: 'hidden',
          flexShrink: 0,
          transition: 'width 0.3s ease',
        }}>
          {/* Toggle Header */}
          <div
            onClick={() => setShowBacktest(!showBacktest)}
            style={{
              ...sectionHeaderStyle,
              borderLeft: `3px solid ${F.PURPLE}`,
              cursor: 'pointer',
              justifyContent: 'space-between',
            }}
          >
            <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
              <BarChart3 size={14} color={F.PURPLE} />
              {showBacktest && (
                <span style={{ fontSize: '10px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
                  QUICK BACKTEST
                </span>
              )}
            </div>
            {showBacktest ? <ChevronRight size={12} color={F.MUTED} /> : <ChevronDown size={12} color={F.MUTED} />}
          </div>

          {showBacktest && (
            <div style={{ flex: 1, overflow: 'auto' }}>
              {/* Config */}
              <div style={{ padding: '12px 16px', display: 'flex', flexDirection: 'column', gap: '10px' }}>
                <div>
                  <label style={labelStyle}>DATA SOURCE</label>
                  <select value={btProvider} onChange={e => setBtProvider(e.target.value as 'yfinance' | 'fyers')} style={inputStyle}>
                    <option value="fyers">Fyers (Local Cache)</option>
                    <option value="yfinance">Yahoo Finance</option>
                  </select>
                </div>
                <div>
                  <label style={labelStyle}>SYMBOL</label>
                  <input
                    value={btSymbol}
                    onChange={e => setBtSymbol(e.target.value)}
                    placeholder={btProvider === 'fyers' ? 'RELIANCE, TCS' : 'RELIANCE.NS, AAPL'}
                    style={inputStyle}
                  />
                </div>
                <div>
                  <label style={labelStyle}>PERIOD</label>
                  <select value={btPeriod} onChange={e => setBtPeriod(e.target.value)} style={inputStyle}>
                    {[['1mo','1 Month'],['3mo','3 Months'],['6mo','6 Months'],['1y','1 Year'],['2y','2 Years'],['5y','5 Years']].map(([v,l]) => (
                      <option key={v} value={v}>{l}</option>
                    ))}
                  </select>
                </div>
                <button
                  onClick={handleBacktest}
                  disabled={backtesting}
                  style={{
                    width: '100%', padding: '10px', backgroundColor: F.PURPLE, color: F.WHITE,
                    border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700,
                    cursor: backtesting ? 'not-allowed' : 'pointer',
                    opacity: backtesting ? 0.5 : 1, display: 'flex', alignItems: 'center',
                    justifyContent: 'center', gap: '6px', letterSpacing: '0.5px',
                    transition: 'all 0.2s',
                  }}
                >
                  {backtesting ? <Loader size={10} className="animate-spin" /> : <Play size={10} />}
                  {backtesting ? 'RUNNING BACKTEST...' : 'RUN BACKTEST'}
                </button>
              </div>

              {/* Backtest Error */}
              {btError && (
                <div style={{
                  margin: '0 16px 8px', padding: '8px 10px',
                  backgroundColor: `${F.RED}15`, border: `1px solid ${F.RED}30`,
                  borderRadius: '2px',
                }}>
                  <span style={{ fontSize: '9px', color: F.RED, lineHeight: '1.4' }}>{btError}</span>
                </div>
              )}

              {/* Debug Log */}
              {btDebug.length > 0 && (
                <div style={{ margin: '0 16px 8px' }}>
                  <button
                    onClick={() => setShowDebug(!showDebug)}
                    style={{
                      width: '100%', padding: '6px 8px', backgroundColor: F.HEADER_BG,
                      border: `1px solid ${F.BORDER}`, color: F.MUTED, fontSize: '8px',
                      fontWeight: 700, cursor: 'pointer', textAlign: 'left', borderRadius: '2px',
                      display: 'flex', justifyContent: 'space-between', alignItems: 'center',
                    }}
                  >
                    <span>DEBUG LOG ({btDebug.length})</span>
                    {showDebug ? <ChevronDown size={10} /> : <ChevronRight size={10} />}
                  </button>
                  {showDebug && (
                    <div style={{
                      marginTop: '2px', padding: '6px 8px', backgroundColor: '#0a0a0a',
                      border: `1px solid ${F.BORDER}`, borderRadius: '2px', maxHeight: '150px',
                      overflow: 'auto', fontSize: '8px', lineHeight: '1.4',
                    }}>
                      {btDebug.map((line, i) => (
                        <div key={i} style={{
                          color: line.includes('ERROR') ? F.RED : line.includes('WARNING') ? F.YELLOW : F.MUTED,
                          marginBottom: '1px', wordBreak: 'break-all',
                        }}>
                          {line}
                        </div>
                      ))}
                    </div>
                  )}
                </div>
              )}

              {/* ─── BACKTEST RESULTS ─── */}
              {btResult && (
                <div style={{ borderTop: `1px solid ${F.BORDER}` }}>
                  {/* Summary Bar */}
                  <div style={{
                    padding: '12px 16px', backgroundColor: F.HEADER_BG,
                    display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                    borderBottom: `1px solid ${F.BORDER}`,
                  }}>
                    <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
                      {btResult.metrics.total_return_pct >= 0
                        ? <TrendingUp size={14} color={F.GREEN} />
                        : <TrendingDown size={14} color={F.RED} />
                      }
                      <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
                        RESULTS
                      </span>
                    </div>
                    <span style={{
                      fontSize: '14px', fontWeight: 700,
                      color: btResult.metrics.total_return_pct >= 0 ? F.GREEN : F.RED,
                    }}>
                      {btResult.metrics.total_return_pct >= 0 ? '+' : ''}{btResult.metrics.total_return_pct}%
                    </span>
                  </div>

                  {/* Metrics Grid */}
                  <div style={{ padding: '12px 16px' }}>
                    <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
                      {[
                        { label: 'WIN RATE', value: `${btResult.metrics.win_rate}%`, color: btResult.metrics.win_rate >= 50 ? F.GREEN : F.YELLOW, icon: Target },
                        { label: 'MAX DRAWDOWN', value: `${btResult.metrics.max_drawdown}%`, color: F.RED, icon: ArrowDownRight },
                        { label: 'SHARPE', value: btResult.metrics.sharpe.toFixed(2), color: btResult.metrics.sharpe >= 1 ? F.GREEN : F.YELLOW, icon: Percent },
                        { label: 'PROFIT FACTOR', value: btResult.metrics.profit_factor.toFixed(2), color: btResult.metrics.profit_factor >= 1.5 ? F.GREEN : F.YELLOW, icon: Activity },
                        { label: 'TRADES', value: String(btResult.metrics.total_trades), color: F.CYAN, icon: Hash },
                        { label: 'AVG P&L', value: btResult.metrics.avg_pnl.toFixed(2), color: btResult.metrics.avg_pnl >= 0 ? F.GREEN : F.RED, icon: TrendingUp },
                        { label: 'AVG BARS', value: btResult.metrics.avg_bars_held.toFixed(1), color: F.CYAN, icon: Clock },
                        { label: 'W / L', value: `${btResult.metrics.winning_trades} / ${btResult.metrics.losing_trades}`, color: F.WHITE, icon: BarChart3 },
                      ].map((m, i) => (
                        <div key={i} style={{
                          backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`,
                          borderRadius: '2px', padding: '10px',
                        }}>
                          <div style={{
                            display: 'flex', alignItems: 'center', gap: '4px',
                            fontSize: '8px', fontWeight: 700, color: F.MUTED,
                            letterSpacing: '0.5px', marginBottom: '4px',
                          }}>
                            <m.icon size={9} />
                            {m.label}
                          </div>
                          <div style={{
                            fontSize: '13px', fontWeight: 700, color: m.color,
                          }}>{m.value}</div>
                        </div>
                      ))}
                    </div>
                  </div>

                  {/* Trade Log */}
                  {btResult.trades.length > 0 && (
                    <div>
                      <div style={{
                        padding: '8px 16px', backgroundColor: F.HEADER_BG,
                        borderTop: `1px solid ${F.BORDER}`,
                        borderBottom: `1px solid ${F.BORDER}`,
                      }}>
                        <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
                          TRADE LOG ({btResult.trades.length})
                        </span>
                      </div>
                      <div style={{ maxHeight: '200px', overflow: 'auto' }}>
                        <table style={{
                          width: '100%', borderCollapse: 'collapse', fontSize: '9px',
                        }}>
                          <thead>
                            <tr>
                              {['#', 'ENTRY', 'EXIT', 'P&L%', 'BARS'].map(h => (
                                <th key={h} style={{
                                  padding: '6px 8px',
                                  textAlign: h === '#' ? 'left' : 'right',
                                  fontSize: '8px', fontWeight: 700, color: F.GRAY,
                                  letterSpacing: '0.5px', position: 'sticky', top: 0,
                                  backgroundColor: F.HEADER_BG,
                                  borderBottom: `1px solid ${F.BORDER}`,
                                }}>{h}</th>
                              ))}
                            </tr>
                          </thead>
                          <tbody>
                            {btResult.trades.map((t, i) => (
                              <tr key={i} style={{ borderBottom: `1px solid ${F.BORDER}30` }}>
                                <td style={{ padding: '5px 8px', color: F.MUTED }}>{i + 1}</td>
                                <td style={{ padding: '5px 8px', textAlign: 'right', color: F.WHITE }}>{t.entry_price}</td>
                                <td style={{ padding: '5px 8px', textAlign: 'right', color: F.WHITE }}>{t.exit_price}</td>
                                <td style={{
                                  padding: '5px 8px', textAlign: 'right', fontWeight: 700,
                                  color: t.pnl_pct >= 0 ? F.GREEN : F.RED,
                                }}>
                                  {t.pnl_pct >= 0 ? '+' : ''}{t.pnl_pct}%
                                </td>
                                <td style={{ padding: '5px 8px', textAlign: 'right', color: F.CYAN }}>{t.bars_held}</td>
                              </tr>
                            ))}
                          </tbody>
                        </table>
                      </div>
                    </div>
                  )}
                </div>
              )}

              {/* Empty state */}
              {!btResult && !backtesting && !btError && (
                <div style={{
                  flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center',
                  justifyContent: 'center', padding: '32px 16px', opacity: 0.5,
                }}>
                  <BarChart3 size={28} color={F.MUTED} style={{ marginBottom: '8px' }} />
                  <div style={{ fontSize: '9px', color: F.MUTED, textAlign: 'center', lineHeight: '1.5' }}>
                    Configure symbol and period, then click RUN BACKTEST to see results.
                  </div>
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
