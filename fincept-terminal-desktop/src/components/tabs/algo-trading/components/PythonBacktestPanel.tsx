import React, { useState, useEffect } from 'react';
import {
  Play,
  Loader2,
  AlertCircle,
  TrendingUp,
  TrendingDown,
  BarChart3,
  DollarSign,
  Target,
  Percent,
  ArrowDownRight,
  Clock,
  X,
  Settings,
  ChevronDown,
  ChevronUp,
  Bug,
  Info,
} from 'lucide-react';
import type { PythonStrategy, PythonBacktestResult, BacktestTrade, EquityPoint } from '../types';
import { runPythonBacktest, getPythonStrategyCode } from '../services/algoTradingService';
import ParameterEditor from './ParameterEditor';
import { F } from '../constants/theme';

const font = '"IBM Plex Mono", "Consolas", monospace';

interface PythonBacktestPanelProps {
  strategy: PythonStrategy;
  onClose: () => void;
}

const PythonBacktestPanel: React.FC<PythonBacktestPanelProps> = ({ strategy, onClose }) => {
  // Config state
  const [symbols, setSymbols] = useState('SPY');
  const [startDate, setStartDate] = useState(() => {
    const d = new Date();
    d.setFullYear(d.getFullYear() - 1);
    return d.toISOString().split('T')[0];
  });
  const [endDate, setEndDate] = useState(() => new Date().toISOString().split('T')[0]);
  const [initialCapital, setInitialCapital] = useState(100000);
  const [parameters, setParameters] = useState<Record<string, string>>({});
  const [showParams, setShowParams] = useState(false);
  const [strategyCode, setStrategyCode] = useState<string | undefined>(undefined);

  // Result state
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<PythonBacktestResult | null>(null);
  const [debugLog, setDebugLog] = useState<string[]>([]);
  const [showDebug, setShowDebug] = useState(false);

  // Load strategy code for parameter extraction
  useEffect(() => {
    const loadCode = async () => {
      try {
        const response = await getPythonStrategyCode(strategy.id);
        if (response.success && response.data) {
          setStrategyCode(response.data.code);
        }
      } catch (err) {
        console.error('[PythonBacktest] Failed to load strategy code:', err);
      }
    };
    loadCode();
  }, [strategy.id]);

  const handleRunBacktest = async () => {
    setLoading(true);
    setError(null);
    setResult(null);
    setDebugLog([]);

    try {
      const request = {
        strategy_id: strategy.id,
        symbol: symbols.split(',').map(s => s.trim().toUpperCase()).filter(Boolean).join(','),
        start_date: startDate,
        end_date: endDate,
        initial_capital: initialCapital,
        params: Object.keys(parameters).length > 0 ? JSON.stringify(parameters) : undefined,
      };
      console.log('[PythonBacktest] Request:', request);

      const response = await runPythonBacktest(request);
      console.log('[PythonBacktest] Response:', response);

      if (!response.success) {
        setError(response.error || 'Backtest failed');
        if (response.debug) setDebugLog(response.debug as string[]);
        return;
      }

      if (response.data) {
        setResult(response.data);
        if (response.data.debug) setDebugLog(response.data.debug);
      }
    } catch (err) {
      console.error('[PythonBacktest] Exception:', err);
      setError(err instanceof Error ? err.message : 'Unknown error');
    } finally {
      setLoading(false);
    }
  };

  const m = result?.metrics;
  const safe = (val: number | undefined, decimals = 2) =>
    val != null && isFinite(val) ? val.toFixed(decimals) : '—';

  return (
    <div
      style={{
        position: 'fixed', inset: 0, display: 'flex', alignItems: 'center', justifyContent: 'center',
        zIndex: 1000, backgroundColor: 'rgba(0,0,0,0.85)',
      }}
      onClick={onClose}
    >
      <div
        style={{
          width: '980px', maxHeight: '92vh', display: 'flex', flexDirection: 'column',
          backgroundColor: F.PANEL_BG, borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER,
          borderRadius: '3px', overflow: 'hidden', fontFamily: font,
        }}
        onClick={e => e.stopPropagation()}
      >
        {/* ── Header ── */}
        <div style={{
          display: 'flex', alignItems: 'center', justifyContent: 'space-between',
          padding: '14px 20px', backgroundColor: F.HEADER_BG,
          borderBottomWidth: '2px', borderBottomStyle: 'solid', borderBottomColor: F.ORANGE,
        }}>
          <div>
            <div style={{ fontSize: '14px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>
              BACKTEST — {strategy.name}
            </div>
            <div style={{ fontSize: '10px', color: F.GRAY, marginTop: '3px' }}>
              {strategy.id} · {strategy.category}
            </div>
          </div>
          <button
            onClick={onClose}
            style={{
              background: 'none', borderWidth: 0, color: F.GRAY, cursor: 'pointer',
              padding: '4px', borderRadius: '2px',
            }}
            onMouseEnter={e => { e.currentTarget.style.color = F.WHITE; }}
            onMouseLeave={e => { e.currentTarget.style.color = F.GRAY; }}
          >
            <X size={18} />
          </button>
        </div>

        {/* ── Body ── */}
        <div style={{ flex: 1, overflowY: 'auto', padding: '20px' }}>

          {/* ── Configuration Grid ── */}
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '14px', marginBottom: '16px' }}>
            <InputField label="SYMBOLS" value={symbols} onChange={setSymbols} placeholder="SPY, AAPL, MSFT" />
            <InputField label="START DATE" value={startDate} onChange={setStartDate} type="date" />
            <InputField label="END DATE" value={endDate} onChange={setEndDate} type="date" />
            <InputField label="INITIAL CAPITAL" value={String(initialCapital)} onChange={v => setInitialCapital(Number(v))} type="number" />

            {/* Empty spacer */}
            <div />

            {/* Run button */}
            <div style={{ display: 'flex', alignItems: 'flex-end' }}>
              <button
                onClick={handleRunBacktest}
                disabled={loading || !symbols.trim()}
                style={{
                  width: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px',
                  padding: '10px 16px', borderRadius: '2px', borderWidth: 0,
                  fontSize: '11px', fontWeight: 700, letterSpacing: '0.5px', fontFamily: font,
                  backgroundColor: loading ? F.GRAY : F.ORANGE,
                  color: F.DARK_BG, cursor: loading || !symbols.trim() ? 'not-allowed' : 'pointer',
                  opacity: !symbols.trim() ? 0.5 : 1,
                  transition: 'all 0.2s',
                }}
              >
                {loading ? <><Loader2 size={14} className="animate-spin" /> RUNNING...</> : <><Play size={14} /> RUN BACKTEST</>}
              </button>
            </div>
          </div>

          {/* ── Parameters Toggle ── */}
          <button
            onClick={() => setShowParams(!showParams)}
            style={{
              width: '100%', display: 'flex', alignItems: 'center', justifyContent: 'space-between',
              padding: '10px 14px', marginBottom: '16px', borderRadius: '2px',
              backgroundColor: F.DARK_BG, borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER,
              color: F.GRAY, cursor: 'pointer', fontFamily: font, fontSize: '10px', fontWeight: 700,
              letterSpacing: '0.5px',
            }}
          >
            <span style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Settings size={13} /> STRATEGY PARAMETERS
            </span>
            {showParams ? <ChevronUp size={13} /> : <ChevronDown size={13} />}
          </button>

          {showParams && strategyCode && (
            <div style={{ marginBottom: '16px' }}>
              <ParameterEditor code={strategyCode} values={parameters} onChange={setParameters} disabled={loading} />
            </div>
          )}

          {/* ── Error ── */}
          {error && (
            <div style={{
              display: 'flex', alignItems: 'flex-start', gap: '10px', padding: '12px 14px', marginBottom: '16px',
              borderRadius: '2px', backgroundColor: `${F.RED}12`, borderWidth: '1px', borderStyle: 'solid',
              borderColor: `${F.RED}60`, fontSize: '11px', color: F.RED, lineHeight: '1.5',
            }}>
              <AlertCircle size={16} style={{ flexShrink: 0, marginTop: '1px' }} />
              <div style={{ flex: 1, whiteSpace: 'pre-wrap', wordBreak: 'break-word' }}>{error}</div>
            </div>
          )}

          {/* ── Debug Log Toggle ── */}
          {debugLog.length > 0 && (
            <div style={{ marginBottom: '16px' }}>
              <button
                onClick={() => setShowDebug(!showDebug)}
                style={{
                  display: 'flex', alignItems: 'center', gap: '6px', padding: '6px 10px',
                  backgroundColor: 'transparent', borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER,
                  color: F.GRAY, fontSize: '9px', fontWeight: 700, fontFamily: font, borderRadius: '2px',
                  cursor: 'pointer', letterSpacing: '0.5px',
                }}
              >
                <Bug size={11} />
                {showDebug ? 'HIDE' : 'SHOW'} DEBUG LOG ({debugLog.length})
              </button>
              {showDebug && (
                <div style={{
                  marginTop: '8px', padding: '10px', backgroundColor: F.DARK_BG,
                  borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER,
                  borderRadius: '2px', maxHeight: '200px', overflowY: 'auto',
                  fontSize: '9px', color: F.MUTED, lineHeight: '1.7', fontFamily: font,
                }}>
                  {debugLog.map((line, i) => <div key={i}>{line}</div>)}
                </div>
              )}
            </div>
          )}

          {/* ── Results ── */}
          {result && m && (
            <div>
              {/* Info banner if no trades */}
              {m.total_trades === 0 && (
                <div style={{
                  display: 'flex', alignItems: 'center', gap: '10px', padding: '12px 14px', marginBottom: '16px',
                  borderRadius: '2px', backgroundColor: `${F.YELLOW}12`, borderWidth: '1px', borderStyle: 'solid',
                  borderColor: `${F.YELLOW}40`, fontSize: '10px', color: F.YELLOW,
                }}>
                  <Info size={14} />
                  Strategy produced 0 trades. This may be expected for universe-selection or framework strategies that require specific market conditions.
                </div>
              )}

              {/* ── Metrics Grid ── */}
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '10px', marginBottom: '16px' }}>
                <MetricCard
                  label="Total Return" icon={m.total_return_pct >= 0 ? TrendingUp : TrendingDown}
                  value={`${m.total_return_pct >= 0 ? '+' : ''}${safe(m.total_return_pct)}%`}
                  valueColor={m.total_return_pct >= 0 ? F.GREEN : F.RED}
                />
                <MetricCard label="Total Trades" icon={BarChart3} value={String(m.total_trades)} />
                <MetricCard
                  label="Win Rate" icon={Target}
                  value={`${safe(m.win_rate, 1)}%`}
                  valueColor={m.win_rate >= 50 ? F.GREEN : F.YELLOW}
                />
                <MetricCard
                  label="Max Drawdown" icon={ArrowDownRight}
                  value={`-${safe(m.max_drawdown_pct)}%`}
                  valueColor={F.RED}
                />
                <MetricCard
                  label="Sharpe Ratio" icon={Percent}
                  value={safe(m.sharpe_ratio)}
                  valueColor={m.sharpe_ratio >= 1 ? F.GREEN : F.YELLOW}
                />
                <MetricCard
                  label="Profit Factor"
                  value={safe(m.profit_factor)}
                  valueColor={m.profit_factor >= 1.5 ? F.GREEN : F.YELLOW}
                />
                <MetricCard
                  label="Avg P&L" icon={DollarSign}
                  value={`$${safe(m.avg_pnl)}`}
                  valueColor={m.avg_pnl >= 0 ? F.GREEN : F.RED}
                />
                <MetricCard
                  label="Avg Hold" icon={Clock}
                  value={`${safe(m.avg_bars_held, 0)} bars`}
                />
              </div>

              {/* ── Equity Curve ── */}
              {result.equity_curve.length > 0 && (
                <div style={{
                  marginBottom: '16px', padding: '14px', backgroundColor: F.DARK_BG,
                  borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER, borderRadius: '2px',
                }}>
                  <div style={{ fontSize: '10px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '10px' }}>
                    EQUITY CURVE
                  </div>
                  <EquityChart curve={result.equity_curve} />
                </div>
              )}

              {/* ── Trades Table ── */}
              {result.trades.length > 0 && (
                <div style={{
                  borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER,
                  borderRadius: '2px', overflow: 'hidden', backgroundColor: F.DARK_BG,
                }}>
                  <div style={{
                    display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                    padding: '10px 14px', borderBottomWidth: '1px', borderBottomStyle: 'solid', borderBottomColor: F.BORDER,
                  }}>
                    <span style={{ fontSize: '10px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
                      TRADES ({result.trades.length})
                    </span>
                    <span style={{ fontSize: '9px', color: F.MUTED }}>
                      Total P&L: <span style={{ color: m.total_return >= 0 ? F.GREEN : F.RED, fontWeight: 700 }}>
                        {m.total_return >= 0 ? '+' : ''}${safe(m.total_return)}
                      </span>
                    </span>
                  </div>
                  <div style={{ maxHeight: '240px', overflowY: 'auto' }}>
                    <table style={{ width: '100%', borderCollapse: 'collapse', fontFamily: font }}>
                      <thead>
                        <tr>
                          {['Symbol', 'Qty', 'Entry', 'Exit', 'P&L', 'Bars'].map(h => (
                            <th key={h} style={{
                              padding: '8px 12px', fontSize: '9px', fontWeight: 700, color: F.GRAY,
                              letterSpacing: '0.5px', textAlign: h === 'Symbol' ? 'left' : 'right',
                              borderBottomWidth: '1px', borderBottomStyle: 'solid', borderBottomColor: F.BORDER,
                            }}>
                              {h.toUpperCase()}
                            </th>
                          ))}
                        </tr>
                      </thead>
                      <tbody>
                        {result.trades.slice(0, 50).map((trade: BacktestTrade, idx: number) => (
                          <tr key={trade.id || idx} style={{
                            borderTopWidth: idx > 0 ? '1px' : 0, borderTopStyle: 'solid', borderTopColor: F.BORDER,
                          }}>
                            <td style={{ padding: '7px 12px', fontSize: '11px', color: F.WHITE }}>{trade.symbol}</td>
                            <td style={{ padding: '7px 12px', fontSize: '11px', color: F.GRAY, textAlign: 'right' }}>{trade.quantity}</td>
                            <td style={{ padding: '7px 12px', fontSize: '11px', color: F.WHITE, textAlign: 'right' }}>
                              ${trade.entry_price.toFixed(2)}
                            </td>
                            <td style={{ padding: '7px 12px', fontSize: '11px', color: F.WHITE, textAlign: 'right' }}>
                              {trade.exit_price != null ? `$${trade.exit_price.toFixed(2)}` : '—'}
                            </td>
                            <td style={{
                              padding: '7px 12px', fontSize: '11px', textAlign: 'right', fontWeight: 700,
                              color: trade.pnl >= 0 ? F.GREEN : F.RED,
                            }}>
                              {trade.pnl >= 0 ? '+' : ''}${trade.pnl.toFixed(2)}
                            </td>
                            <td style={{ padding: '7px 12px', fontSize: '11px', color: F.GRAY, textAlign: 'right' }}>
                              {trade.bars_held}
                            </td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                    {result.trades.length > 50 && (
                      <div style={{ textAlign: 'center', padding: '10px', fontSize: '9px', color: F.GRAY }}>
                        Showing first 50 of {result.trades.length} trades
                      </div>
                    )}
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


// ── Input Field ──
const InputField: React.FC<{
  label: string; value: string; onChange: (v: string) => void;
  type?: string; placeholder?: string;
}> = ({ label, value, onChange, type = 'text', placeholder }) => (
  <div>
    <div style={{ fontSize: '10px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '5px' }}>
      {label}
    </div>
    <input
      type={type}
      value={value}
      onChange={e => onChange(e.target.value)}
      placeholder={placeholder}
      style={{
        width: '100%', padding: '8px 10px', backgroundColor: F.DARK_BG,
        borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER,
        borderRadius: '2px', color: F.WHITE, fontSize: '11px', fontFamily: font,
        outline: 'none', boxSizing: 'border-box',
      }}
      onFocus={e => { e.currentTarget.style.borderColor = F.ORANGE; }}
      onBlur={e => { e.currentTarget.style.borderColor = F.BORDER; }}
    />
  </div>
);


// ── Metric Card ──
const MetricCard: React.FC<{
  label: string; value: string; valueColor?: string; icon?: React.ElementType;
}> = ({ label, value, valueColor = F.WHITE, icon: Icon }) => (
  <div style={{
    padding: '12px 14px', backgroundColor: F.DARK_BG,
    borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER, borderRadius: '2px',
  }}>
    <div style={{
      fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px',
      display: 'flex', alignItems: 'center', gap: '5px', marginBottom: '6px',
    }}>
      {Icon && <Icon size={12} />}
      {label.toUpperCase()}
    </div>
    <div style={{ fontSize: '16px', fontWeight: 700, color: valueColor, fontFamily: font }}>
      {value}
    </div>
  </div>
);


// ── Equity Chart (SVG) ──
const EquityChart: React.FC<{ curve: EquityPoint[] }> = ({ curve }) => {
  if (curve.length < 2) return null;

  const values = curve.map(p => p.value);
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min || 1;

  const W = 100;
  const H = 50;

  const pts = curve.map((p, i) => {
    const x = (i / (curve.length - 1)) * W;
    const y = H - ((p.value - min) / range) * H;
    return `${x},${y}`;
  });

  const pathD = `M ${pts.join(' L ')}`;
  const isPositive = values[values.length - 1] >= values[0];

  return (
    <svg viewBox={`0 0 ${W} ${H}`} style={{ width: '100%', height: '90px' }} preserveAspectRatio="none">
      <defs>
        <linearGradient id="eqGrad" x1="0%" y1="0%" x2="0%" y2="100%">
          <stop offset="0%" stopColor={isPositive ? F.GREEN : F.RED} stopOpacity="0.2" />
          <stop offset="100%" stopColor={isPositive ? F.GREEN : F.RED} stopOpacity="0" />
        </linearGradient>
      </defs>
      <path d={`${pathD} L ${W},${H} L 0,${H} Z`} fill="url(#eqGrad)" />
      <path d={pathD} fill="none" stroke={isPositive ? F.GREEN : F.RED} strokeWidth="0.8" />
    </svg>
  );
};


export default PythonBacktestPanel;
