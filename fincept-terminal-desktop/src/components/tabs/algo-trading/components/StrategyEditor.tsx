import React, { useState, useEffect } from 'react';
import { Save, Play, Loader, ArrowLeft } from 'lucide-react';
import type { ConditionItem } from '../types';
import { TIMEFRAMES } from '../constants/indicators';
import { saveAlgoStrategy, getAlgoStrategy, runAlgoBacktest } from '../services/algoTradingService';
import ConditionBuilder from './ConditionBuilder';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A', BORDER: '#2A2A2A', HOVER: '#1F1F1F',
  MUTED: '#4A4A4A', CYAN: '#00E5FF', YELLOW: '#FFD700', PURPLE: '#9D4EDD',
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
  const [saving, setSaving] = useState(false);
  const [error, setError] = useState('');
  const [btSymbol, setBtSymbol] = useState('');
  const [btPeriod, setBtPeriod] = useState('1y');
  const [backtesting, setBacktesting] = useState(false);
  const [btResult, setBtResult] = useState<BacktestResult | null>(null);
  const [btError, setBtError] = useState('');

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
    if (!btSymbol.trim()) { setBtError('Enter a symbol (e.g. RELIANCE.NS, AAPL)'); return; }
    if (entryConditions.length === 0) { setBtError('Add at least one entry condition'); return; }
    setBacktesting(true); setBtError(''); setBtResult(null);
    const res = await runAlgoBacktest({
      symbol: btSymbol.trim(),
      entryConditions: buildConditionsJson(entryConditions, entryLogic),
      exitConditions: buildConditionsJson(exitConditions, exitLogic),
      timeframe: ['1m','3m','5m'].includes(timeframe) ? '1d' : timeframe,
      period: btPeriod,
      stopLoss: stopLoss ? parseFloat(stopLoss) : undefined,
      takeProfit: takeProfit ? parseFloat(takeProfit) : undefined,
    });
    setBacktesting(false);
    if (res.success && res.data) setBtResult(res.data);
    else setBtError(res.error || 'Backtest failed');
  };

  return (
    <div style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Header */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <button
            onClick={onCancel}
            style={{ padding: '6px', backgroundColor: 'transparent', border: 'none', color: F.GRAY, cursor: 'pointer', borderRadius: '2px' }}
          >
            <ArrowLeft size={14} />
          </button>
          <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE }}>
            {editStrategyId ? 'EDIT STRATEGY' : 'CREATE STRATEGY'}
          </span>
        </div>
        <div style={{ display: 'flex', gap: '6px' }}>
          <button onClick={onCancel} style={{
            padding: '6px 10px', backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
            color: F.GRAY, fontSize: '9px', fontWeight: 700, borderRadius: '2px', cursor: 'pointer',
          }}>CANCEL</button>
          <button onClick={handleSave} disabled={saving} style={{
            padding: '8px 16px', backgroundColor: F.ORANGE, color: F.DARK_BG, border: 'none',
            borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
            opacity: saving ? 0.5 : 1, display: 'flex', alignItems: 'center', gap: '4px',
          }}>
            <Save size={10} />
            {saving ? 'SAVING...' : 'SAVE STRATEGY'}
          </button>
        </div>
      </div>

      {error && (
        <div style={{ padding: '6px 10px', backgroundColor: `${F.RED}20`, color: F.RED, fontSize: '9px', fontWeight: 700, borderRadius: '2px' }}>
          {error}
        </div>
      )}

      {/* Basic Info */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
        <div>
          <label style={labelStyle}>STRATEGY NAME</label>
          <input value={name} onChange={e => setName(e.target.value)} placeholder="My RSI Strategy" style={inputStyle} />
        </div>
        <div>
          <label style={labelStyle}>TIMEFRAME</label>
          <select value={timeframe} onChange={e => setTimeframe(e.target.value)} style={inputStyle}>
            {TIMEFRAMES.map(tf => <option key={tf.value} value={tf.value}>{tf.label}</option>)}
          </select>
        </div>
      </div>

      <div>
        <label style={labelStyle}>DESCRIPTION (OPTIONAL)</label>
        <input value={description} onChange={e => setDescription(e.target.value)} placeholder="Short description" style={inputStyle} />
      </div>

      {/* Entry & Exit Conditions */}
      <ConditionBuilder label="Entry Conditions (BUY when)" conditions={entryConditions} logic={entryLogic}
        onConditionsChange={setEntryConditions} onLogicChange={setEntryLogic} />
      <ConditionBuilder label="Exit Conditions (SELL when)" conditions={exitConditions} logic={exitLogic}
        onConditionsChange={setExitConditions} onLogicChange={setExitLogic} />

      {/* Risk Management */}
      <div style={{ backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px' }}>
        <div style={{ padding: '8px 12px', backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}` }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>RISK MANAGEMENT</span>
        </div>
        <div style={{ padding: '12px', display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
          <div>
            <label style={labelStyle}>STOP LOSS (%)</label>
            <input type="number" value={stopLoss} onChange={e => setStopLoss(e.target.value)} placeholder="e.g. 2" step="0.1" style={inputStyle} />
          </div>
          <div>
            <label style={labelStyle}>TAKE PROFIT (%)</label>
            <input type="number" value={takeProfit} onChange={e => setTakeProfit(e.target.value)} placeholder="e.g. 5" step="0.1" style={inputStyle} />
          </div>
          <div>
            <label style={labelStyle}>TRAILING STOP (%)</label>
            <input type="number" value={trailingStop} onChange={e => setTrailingStop(e.target.value)} placeholder="e.g. 1.5" step="0.1" style={inputStyle} />
          </div>
        </div>
      </div>

      {/* Quick Backtest */}
      <div style={{ backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px' }}>
        <div style={{ padding: '8px 12px', backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}` }}>
          <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>QUICK BACKTEST</span>
        </div>
        <div style={{ padding: '12px', display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '12px' }}>
          <div>
            <label style={labelStyle}>SYMBOL (YFINANCE)</label>
            <input value={btSymbol} onChange={e => setBtSymbol(e.target.value)} placeholder="RELIANCE.NS, AAPL" style={inputStyle} />
          </div>
          <div>
            <label style={labelStyle}>PERIOD</label>
            <select value={btPeriod} onChange={e => setBtPeriod(e.target.value)} style={inputStyle}>
              {[['1mo','1 Month'],['3mo','3 Months'],['6mo','6 Months'],['1y','1 Year'],['2y','2 Years'],['5y','5 Years']].map(([v,l]) => (
                <option key={v} value={v}>{l}</option>
              ))}
            </select>
          </div>
          <div style={{ display: 'flex', alignItems: 'flex-end' }}>
            <button onClick={handleBacktest} disabled={backtesting} style={{
              width: '100%', padding: '8px 16px', backgroundColor: F.PURPLE, color: F.WHITE,
              border: 'none', borderRadius: '2px', fontSize: '9px', fontWeight: 700, cursor: 'pointer',
              opacity: backtesting ? 0.5 : 1, display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
            }}>
              {backtesting ? <Loader size={10} className="animate-spin" /> : <Play size={10} />}
              {backtesting ? 'RUNNING...' : 'RUN BACKTEST'}
            </button>
          </div>
        </div>

        {btError && (
          <div style={{ margin: '0 12px 12px', padding: '6px 10px', backgroundColor: `${F.RED}20`, color: F.RED, fontSize: '9px', borderRadius: '2px' }}>
            {btError}
          </div>
        )}

        {btResult && (
          <div style={{ padding: '0 12px 12px', display: 'flex', flexDirection: 'column', gap: '8px' }}>
            {/* Metrics Grid */}
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '6px' }}>
              {[
                { label: 'TOTAL RETURN', value: `${btResult.metrics.total_return_pct >= 0 ? '+' : ''}${btResult.metrics.total_return_pct}%`, color: btResult.metrics.total_return_pct >= 0 ? F.GREEN : F.RED },
                { label: 'WIN RATE', value: `${btResult.metrics.win_rate}%`, color: btResult.metrics.win_rate >= 50 ? F.GREEN : F.YELLOW },
                { label: 'MAX DRAWDOWN', value: `${btResult.metrics.max_drawdown}%`, color: F.RED },
                { label: 'SHARPE RATIO', value: btResult.metrics.sharpe.toFixed(2), color: btResult.metrics.sharpe >= 1 ? F.GREEN : F.YELLOW },
                { label: 'TOTAL TRADES', value: String(btResult.metrics.total_trades), color: F.CYAN },
                { label: 'PROFIT FACTOR', value: btResult.metrics.profit_factor.toFixed(2), color: btResult.metrics.profit_factor >= 1.5 ? F.GREEN : F.YELLOW },
                { label: 'AVG P&L', value: btResult.metrics.avg_pnl.toFixed(2), color: btResult.metrics.avg_pnl >= 0 ? F.GREEN : F.RED },
                { label: 'AVG BARS HELD', value: btResult.metrics.avg_bars_held.toFixed(1), color: F.CYAN },
              ].map((m, i) => (
                <div key={i} style={{ backgroundColor: F.HEADER_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px', padding: '8px' }}>
                  <div style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>{m.label}</div>
                  <div style={{ fontSize: '11px', fontWeight: 700, color: m.color, marginTop: '2px', fontFamily: '"IBM Plex Mono", monospace' }}>{m.value}</div>
                </div>
              ))}
            </div>

            {/* Trades Table */}
            {btResult.trades.length > 0 && (
              <div style={{ maxHeight: '180px', overflow: 'auto' }}>
                <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace' }}>
                  <thead>
                    <tr style={{ borderBottom: `1px solid ${F.BORDER}` }}>
                      {['#', 'ENTRY', 'EXIT', 'P&L', 'P&L%', 'REASON', 'BARS'].map(h => (
                        <th key={h} style={{ padding: '6px 8px', textAlign: h === '#' || h === 'REASON' ? 'left' : 'right', fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', position: 'sticky', top: 0, backgroundColor: F.PANEL_BG }}>{h}</th>
                      ))}
                    </tr>
                  </thead>
                  <tbody>
                    {btResult.trades.map((t, i) => (
                      <tr key={i} style={{ borderBottom: `1px solid ${F.BORDER}15` }}>
                        <td style={{ padding: '4px 8px', color: F.MUTED }}>{i + 1}</td>
                        <td style={{ padding: '4px 8px', textAlign: 'right', color: F.WHITE }}>{t.entry_price}</td>
                        <td style={{ padding: '4px 8px', textAlign: 'right', color: F.WHITE }}>{t.exit_price}</td>
                        <td style={{ padding: '4px 8px', textAlign: 'right', fontWeight: 700, color: t.pnl >= 0 ? F.GREEN : F.RED }}>
                          {t.pnl >= 0 ? '+' : ''}{t.pnl}
                        </td>
                        <td style={{ padding: '4px 8px', textAlign: 'right', color: t.pnl_pct >= 0 ? F.GREEN : F.RED }}>
                          {t.pnl_pct >= 0 ? '+' : ''}{t.pnl_pct}%
                        </td>
                        <td style={{ padding: '4px 8px', color: F.MUTED }}>{t.reason}</td>
                        <td style={{ padding: '4px 8px', textAlign: 'right', color: F.CYAN }}>{t.bars_held}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
};

export default StrategyEditor;
