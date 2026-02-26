import React, { useState } from 'react';
import {
  Search, Loader, ChevronDown, ChevronRight, AlertTriangle, Zap,
  RotateCcw, Download, Plus, Trash2, Copy as CopyIcon, Terminal,
} from 'lucide-react';
import type { ConditionItem, ScanResult, ConditionDetail } from '../types';
import { TIMEFRAMES, getIndicatorDef } from '../constants/indicators';
import { runAlgoScan } from '../services/algoTradingService';
import { useStockBrokerContextOptional } from '@/contexts/StockBrokerContext';
import { useBrokerContext } from '@/contexts/BrokerContext';
import ConditionBuilder from './ConditionBuilder';
import { F } from '../constants/theme';

const font = '"IBM Plex Mono", "Consolas", monospace';

interface SymbolError { symbol: string; error: string; }

// ── Scan Presets ────────────────────────────────────────────────────────
interface ScanPreset { name: string; desc: string; conditions: ConditionItem[]; logic: 'AND' | 'OR'; }

const SCAN_PRESETS: ScanPreset[] = [
  { name: 'RSI Oversold', desc: 'RSI(14) < 30', conditions: [{ indicator: 'RSI', params: { period: 14 }, field: 'value', operator: '<', value: 30 }], logic: 'AND' },
  { name: 'RSI Overbought', desc: 'RSI(14) > 70', conditions: [{ indicator: 'RSI', params: { period: 14 }, field: 'value', operator: '>', value: 70 }], logic: 'AND' },
  { name: 'Golden Cross', desc: 'EMA(50) X↑ SMA(200)', conditions: [{ indicator: 'EMA', params: { period: 50 }, field: 'value', operator: 'crosses_above', value: 0, compareMode: 'indicator', compareIndicator: 'SMA', compareParams: { period: 200 }, compareField: 'value' }], logic: 'AND' },
  { name: 'Death Cross', desc: 'EMA(50) X↓ SMA(200)', conditions: [{ indicator: 'EMA', params: { period: 50 }, field: 'value', operator: 'crosses_below', value: 0, compareMode: 'indicator', compareIndicator: 'SMA', compareParams: { period: 200 }, compareField: 'value' }], logic: 'AND' },
  { name: 'MACD Bullish', desc: 'MACD X↑ Signal', conditions: [{ indicator: 'MACD', params: { fast: 12, slow: 26, signal: 9 }, field: 'line', operator: 'crosses_above', value: 0, compareMode: 'indicator', compareIndicator: 'MACD', compareParams: { fast: 12, slow: 26, signal: 9 }, compareField: 'signal_line' }], logic: 'AND' },
  { name: 'Bollinger Squeeze', desc: 'Close < BB Lower + RSI<35', conditions: [{ indicator: 'CLOSE', params: {}, field: 'value', operator: '<', value: 0, compareMode: 'indicator', compareIndicator: 'BOLLINGER', compareParams: { period: 20, std_dev: 2 }, compareField: 'lower' }, { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: '<', value: 35 }], logic: 'AND' },
  { name: 'Volume Breakout', desc: 'Rising 3 bars + MFI>60', conditions: [{ indicator: 'CLOSE', params: {}, field: 'value', operator: 'rising', value: 0, lookback: 3 }, { indicator: 'MFI', params: { period: 14 }, field: 'value', operator: '>', value: 60 }], logic: 'AND' },
  { name: 'Supertrend Buy', desc: 'Direction = Bullish', conditions: [{ indicator: 'SUPERTREND', params: { period: 10, multiplier: 3 }, field: 'direction', operator: '==', value: 1 }], logic: 'AND' },
  { name: 'RSI+MACD Combo', desc: 'RSI rising + MACD hist>0', conditions: [{ indicator: 'RSI', params: { period: 14 }, field: 'value', operator: 'rising', value: 0, lookback: 3 }, { indicator: 'MACD', params: { fast: 12, slow: 26, signal: 9 }, field: 'histogram', operator: '>', value: 0 }], logic: 'AND' },
  { name: 'Price > VWAP', desc: 'Close X↑ VWAP', conditions: [{ indicator: 'CLOSE', params: {}, field: 'value', operator: 'crosses_above', value: 0, compareMode: 'indicator', compareIndicator: 'VWAP', compareParams: {}, compareField: 'value' }], logic: 'AND' },
];

// ── Symbol Watchlists ──────────────────────────────────────────────────
const SYMBOL_WATCHLISTS: Record<string, string[]> = {
  'NIFTY 50': ['RELIANCE','TCS','HDFCBANK','INFY','ICICIBANK','HINDUNILVR','ITC','SBIN','BHARTIARTL','KOTAKBANK','LT','AXISBANK','WIPRO','ASIANPAINT','MARUTI','BAJFINANCE','HCLTECH','SUNPHARMA','TITAN','TATASTEEL','ULTRACEMCO','ONGC','NTPC','POWERGRID','M&M','NESTLEIND','ADANIENT','JSWSTEEL','BAJAJFINSV','TECHM','INDUSINDBK','HINDALCO','TATAMTRDVR','COALINDIA','GRASIM','SBILIFE','CIPLA','DIVISLAB','DRREDDY','BPCL','APOLLOHOSP','EICHERMOT','TATACONSUM','BRITANNIA','HDFCLIFE','HEROMOTOCO','BAJAJ-AUTO','LTIM','ADANIPORTS','UPL'],
  'BANK NIFTY': ['HDFCBANK','ICICIBANK','KOTAKBANK','AXISBANK','SBIN','INDUSINDBK','BANKBARODA','AUBANK','FEDERALBNK','BANDHANBNK','PNB','IDFCFIRSTB'],
  'IT STOCKS': ['TCS','INFY','WIPRO','HCLTECH','TECHM','LTIM','MPHASIS','COFORGE','PERSISTENT','TATAELXSI','LTTS','MINDTREE'],
  'PHARMA': ['SUNPHARMA','DRREDDY','CIPLA','DIVISLAB','AUROPHARMA','LUPIN','BIOCON','TORNTPHARM','ALKEM','GLENMARK'],
};

const STOCK_BROKERS = ['fyers','zerodha','upstox','dhan','angelone','kotak','groww','aliceblue','fivepaisa','iifl','motilal','shoonya'];

// ── Format condition detail ────────────────────────────────────────────
function fmtDetail(d: ConditionDetail): React.ReactNode {
  const ops: Record<string, string> = { '>': '>', '<': '<', '>=': '≥', '<=': '≤', '==': '=', crosses_above: 'X↑', crosses_below: 'X↓', between: 'BTW', crossed_above_within: 'X↑N', crossed_below_within: 'X↓N', rising: '↑N', falling: '↓N' };
  const op = ops[d.operator] || d.operator;
  const val = d.computed_value != null ? d.computed_value.toFixed(2) : '—';
  if (d.target_indicator) {
    const tv = d.target_computed_value != null ? d.target_computed_value.toFixed(2) : '—';
    return <><span style={{ color: F.WHITE, fontWeight: 700 }}>{d.indicator}</span>{d.field !== 'value' && <span style={{ color: F.MUTED }}>.{d.field}</span>}<span style={{ color: F.CYAN }}>={val}</span><span style={{ color: F.ORANGE, margin: '0 4px' }}>{op}</span><span style={{ color: F.YELLOW, fontWeight: 700 }}>{d.target_indicator}</span><span style={{ color: F.CYAN }}>={tv}</span></>;
  }
  return <><span style={{ color: F.WHITE, fontWeight: 700 }}>{d.indicator}</span>{d.field !== 'value' && <span style={{ color: F.MUTED }}>.{d.field}</span>}<span style={{ color: F.CYAN }}>={val}</span><span style={{ color: F.ORANGE, margin: '0 4px' }}>{op}</span><span style={{ color: F.MUTED }}>{d.target}</span></>;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

const ScannerPanel: React.FC = () => {
  const stockBroker = useStockBrokerContextOptional();
  const { activeBroker } = useBrokerContext();
  const provider = stockBroker?.activeBroker || (STOCK_BROKERS.includes(activeBroker) ? activeBroker : null) || 'fyers';

  const [conditions, setConditions] = useState<ConditionItem[]>([
    { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: '<', value: 30 },
  ]);
  const [logic, setLogic] = useState<'AND' | 'OR'>('AND');
  const [symbolsInput, setSymbolsInput] = useState('');
  const [timeframe, setTimeframe] = useState('1d');
  const [lookbackDays, setLookbackDays] = useState<number>(365);
  const [scanning, setScanning] = useState(false);
  const [results, setResults] = useState<ScanResult[] | null>(null);
  const [scanned, setScanned] = useState(0);
  const [error, setError] = useState('');
  const [debugLog, setDebugLog] = useState<string[]>([]);
  const [showDebug, setShowDebug] = useState(false);
  const [prefetchWarning, setPrefetchWarning] = useState('');
  const [symbolErrors, setSymbolErrors] = useState<SymbolError[]>([]);
  const [showPresets, setShowPresets] = useState(false);
  const [expandedResult, setExpandedResult] = useState<number | null>(null);

  const buildConditionsJson = (): string => {
    if (conditions.length === 0) return '[]';
    const mixed: (ConditionItem | string)[] = [];
    conditions.forEach((c, i) => { if (i > 0) mixed.push(logic); mixed.push(c); });
    return JSON.stringify(mixed);
  };

  const handleScan = async () => {
    const symbols = symbolsInput.split(/[,\n]/).map(s => s.trim().toUpperCase()).filter(Boolean);
    if (symbols.length === 0) { setError('Enter at least one symbol'); return; }
    if (conditions.length === 0) { setError('Add at least one condition'); return; }
    setScanning(true); setError(''); setResults(null); setDebugLog([]); setPrefetchWarning(''); setSymbolErrors([]); setExpandedResult(null);
    try {
      const result = await runAlgoScan(buildConditionsJson(), JSON.stringify(symbols), timeframe, provider, lookbackDays);
      setScanning(false);
      if (result.debug) setDebugLog(result.debug);
      if (result.prefetch_warning) setPrefetchWarning(result.prefetch_warning);
      if (result.success && result.data) {
        setResults(result.data.matches); setScanned(result.data.scanned);
        if (result.data.errors?.length) setSymbolErrors(result.data.errors);
        if (result.data.debug) setDebugLog(result.data.debug);
        if (result.data.prefetch_warning) setPrefetchWarning(result.data.prefetch_warning);
      } else { setError(result.error || 'Scan failed — check debug log'); }
    } catch (e) { setScanning(false); setError(`Unexpected error: ${e instanceof Error ? e.message : String(e)}`); }
  };

  const applyPreset = (preset: ScanPreset) => {
    setConditions(JSON.parse(JSON.stringify(preset.conditions)));
    setLogic(preset.logic);
    setShowPresets(false);
  };

  const appendWatchlist = (name: string) => {
    const symbols = SYMBOL_WATCHLISTS[name];
    if (!symbols) return;
    const existing = symbolsInput.split(/[,\n]/).map(s => s.trim().toUpperCase()).filter(Boolean);
    const newSymbols = symbols.filter(s => !existing.includes(s));
    setSymbolsInput([...existing, ...newSymbols].join(', '));
  };

  const conditionSummary = conditions.map(c => {
    const def = getIndicatorDef(c.indicator);
    const label = def?.label || c.indicator;
    const pv = Object.values(c.params);
    const ps = pv.length > 0 ? `(${pv.join(',')})` : '';
    const fs = c.field !== 'value' ? `.${c.field}` : '';
    if (c.compareMode === 'indicator' && c.compareIndicator) {
      const cd = getIndicatorDef(c.compareIndicator);
      const cl = cd?.label || c.compareIndicator;
      const cpv = Object.values(c.compareParams || {});
      const cps = cpv.length > 0 ? `(${cpv.join(',')})` : '';
      return `${label}${ps}${fs} ${c.operator} ${cl}${cps}`;
    }
    return `${label}${ps}${fs} ${c.operator} ${c.value}`;
  }).join(` ${logic} `);

  const exportToCSV = () => {
    if (!results || results.length === 0) return;
    const headers = ['#', 'Symbol', 'Price'];
    const indicatorCols = new Set<string>();
    results.forEach(r => r.details.forEach(d => { indicatorCols.add(d.field !== 'value' ? `${d.indicator}.${d.field}` : d.indicator); }));
    const indHeaders = Array.from(indicatorCols);
    headers.push(...indHeaders.map(h => `${h}_Value`), ...indHeaders.map(h => `${h}_Met`));
    const rows = results.map((r, i) => {
      const row = [String(i + 1), r.symbol, r.price.toFixed(2)];
      indHeaders.forEach(h => { const d = r.details.find(d => (d.field !== 'value' ? `${d.indicator}.${d.field}` : d.indicator) === h); row.push(d?.computed_value != null ? d.computed_value.toFixed(4) : ''); });
      indHeaders.forEach(h => { const d = r.details.find(d => (d.field !== 'value' ? `${d.indicator}.${d.field}` : d.indicator) === h); row.push(d ? (d.met ? 'TRUE' : 'FALSE') : ''); });
      return row;
    });
    const csv = [`# Scan Results - ${new Date().toISOString()}`, `# ${conditionSummary}`, `# TF: ${timeframe} | Scanned: ${scanned} | Matches: ${results.length}`, '', headers.join(','), ...rows.map(r => r.map(c => `"${c}"`).join(','))].join('\n');
    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a'); a.href = url; a.download = `scan_${timeframe}_${new Date().toISOString().slice(0, 10)}.csv`;
    document.body.appendChild(a); a.click(); document.body.removeChild(a); URL.revokeObjectURL(url);
  };

  const symbolCount = symbolsInput.split(/[,\n]/).map(s => s.trim()).filter(Boolean).length;

  // ── Inline styles ──
  const inputSt: React.CSSProperties = { padding: '6px 8px', backgroundColor: F.DARK_BG, color: F.WHITE, borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER, borderRadius: '2px', fontSize: '10px', fontFamily: font, outline: 'none', width: '100%' };
  const selectSt: React.CSSProperties = { ...inputSt, cursor: 'pointer' };
  const smallBtn: React.CSSProperties = { padding: '3px 8px', backgroundColor: 'transparent', borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER, borderRadius: '2px', fontSize: '9px', fontWeight: 700, fontFamily: font, color: F.GRAY, cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px', transition: 'all 0.15s', whiteSpace: 'nowrap' };
  const label7: React.CSSProperties = { fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.8px', textTransform: 'uppercase', marginBottom: '4px', display: 'block' };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%', fontFamily: font, backgroundColor: F.DARK_BG }}>

      {/* ━━━ HEADER BAR ━━━ */}
      <div style={{
        display: 'flex', alignItems: 'center', gap: '10px', padding: '8px 14px',
        backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}`, flexShrink: 0,
      }}>
        <Terminal size={14} style={{ color: F.ORANGE }} />
        <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>SCANNER</span>

        {/* Condition summary pill */}
        {conditions.length > 0 && (
          <div style={{
            flex: 1, minWidth: 0, padding: '3px 10px', borderRadius: '2px',
            backgroundColor: `${F.ORANGE}0A`, borderWidth: '1px', borderStyle: 'solid', borderColor: `${F.ORANGE}25`,
            fontSize: '9px', color: F.ORANGE, fontFamily: font, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap',
          }} title={conditionSummary}>
            {conditionSummary}
          </div>
        )}

        <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flexShrink: 0 }}>
          <button onClick={() => setShowPresets(!showPresets)} style={{
            ...smallBtn,
            borderColor: showPresets ? `${F.ORANGE}60` : F.BORDER,
            color: showPresets ? F.ORANGE : F.GRAY,
          }}>
            <Zap size={10} /> PRESETS
          </button>
          <button onClick={() => { setConditions([{ indicator: 'RSI', params: { period: 14 }, field: 'value', operator: '<', value: 30 }]); setLogic('AND'); setResults(null); setError(''); setDebugLog([]); setPrefetchWarning(''); setSymbolErrors([]); }} style={smallBtn}>
            <RotateCcw size={10} /> RESET
          </button>
        </div>
      </div>

      {/* ━━━ PRESETS DROPDOWN ━━━ */}
      {showPresets && (
        <div style={{ padding: '8px 14px', borderBottom: `1px solid ${F.BORDER}`, backgroundColor: F.PANEL_BG, flexShrink: 0 }}>
          <div style={{ fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.8px', marginBottom: '6px' }}>SCAN TEMPLATES — CLICK TO APPLY</div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(180px, 1fr))', gap: '4px' }}>
            {SCAN_PRESETS.map((p, i) => (
              <button key={i} onClick={() => applyPreset(p)} style={{
                padding: '6px 8px', backgroundColor: F.DARK_BG, borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER,
                borderRadius: '2px', cursor: 'pointer', textAlign: 'left', fontFamily: font, transition: 'all 0.15s',
              }}
                onMouseEnter={e => { e.currentTarget.style.borderColor = F.ORANGE; e.currentTarget.style.backgroundColor = F.HEADER_BG; }}
                onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.backgroundColor = F.DARK_BG; }}
              >
                <div style={{ fontSize: '10px', fontWeight: 700, color: F.ORANGE }}>{p.name}</div>
                <div style={{ fontSize: '8px', color: F.MUTED, marginTop: '2px' }}>{p.desc}</div>
              </button>
            ))}
          </div>
        </div>
      )}

      {/* ━━━ MAIN CONTENT: Two columns ━━━ */}
      <div style={{ display: 'flex', flex: 1, overflow: 'hidden', minHeight: 0 }}>

        {/* ── LEFT COLUMN: Conditions + Symbols + Controls ── */}
        <div style={{
          width: '380px', flexShrink: 0, display: 'flex', flexDirection: 'column',
          borderRight: `1px solid ${F.BORDER}`, overflow: 'hidden',
        }}>
          {/* Condition Builder */}
          <div style={{ flexShrink: 0, borderBottom: `1px solid ${F.BORDER}` }}>
            <ConditionBuilder label="Scan Conditions" conditions={conditions} logic={logic}
              onConditionsChange={setConditions} onLogicChange={setLogic} />
          </div>

          {/* Symbols section */}
          <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'auto', padding: '10px 12px', gap: '10px' }}>
            {/* Symbol input */}
            <div>
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '4px' }}>
                <span style={label7}>SYMBOLS</span>
                {symbolCount > 0 && <span style={{ fontSize: '9px', color: F.CYAN, fontWeight: 600 }}>{symbolCount} tickers</span>}
              </div>
              <textarea
                value={symbolsInput}
                onChange={e => setSymbolsInput(e.target.value)}
                placeholder="RELIANCE, TCS, INFY, HDFCBANK..."
                rows={4}
                style={{ ...inputSt, resize: 'none', lineHeight: '1.5' }}
              />
            </div>

            {/* Watchlist quick-add */}
            <div>
              <span style={label7}>QUICK ADD</span>
              <div style={{ display: 'flex', gap: '4px', flexWrap: 'wrap' }}>
                {Object.keys(SYMBOL_WATCHLISTS).map(name => (
                  <button key={name} onClick={() => appendWatchlist(name)} style={smallBtn}
                    onMouseEnter={e => { e.currentTarget.style.borderColor = F.CYAN; e.currentTarget.style.color = F.CYAN; }}
                    onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
                    title={`Add ${SYMBOL_WATCHLISTS[name].length} symbols`}
                  >
                    <Plus size={8} /> {name} ({SYMBOL_WATCHLISTS[name].length})
                  </button>
                ))}
              </div>
            </div>

            {/* Timeframe + Lookback */}
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
              <div>
                <span style={label7}>TIMEFRAME</span>
                <select value={timeframe} onChange={e => setTimeframe(e.target.value)} style={selectSt}>
                  {TIMEFRAMES.map(tf => <option key={tf.value} value={tf.value}>{tf.label}</option>)}
                </select>
              </div>
              <div>
                <span style={label7}>LOOKBACK DAYS</span>
                <input type="number" value={lookbackDays} onChange={e => setLookbackDays(Math.max(1, parseInt(e.target.value) || 30))} min={1} max={1000} style={inputSt} />
              </div>
            </div>

            {/* Provider info */}
            <div style={{ fontSize: '8px', color: F.MUTED, display: 'flex', alignItems: 'center', gap: '4px' }}>
              DATA: <span style={{ color: F.CYAN, fontWeight: 700 }}>{provider.toUpperCase()}</span>
            </div>
          </div>

          {/* SCAN BUTTON */}
          <div style={{ padding: '10px 12px', borderTop: `1px solid ${F.BORDER}`, flexShrink: 0 }}>
            <button onClick={handleScan} disabled={scanning} style={{
              width: '100%', padding: '8px', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '6px',
              backgroundColor: scanning ? F.MUTED : F.ORANGE, color: F.DARK_BG, border: 'none', borderRadius: '2px',
              fontSize: '11px', fontWeight: 700, fontFamily: font, letterSpacing: '0.5px',
              cursor: scanning ? 'not-allowed' : 'pointer', transition: 'all 0.15s',
            }}>
              {scanning ? <Loader size={13} className="animate-spin" /> : <Search size={13} />}
              {scanning ? 'SCANNING...' : 'RUN SCAN'}
            </button>
          </div>
        </div>

        {/* ── RIGHT COLUMN: Results ── */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', minWidth: 0 }}>

          {/* Results header */}
          <div style={{
            display: 'flex', alignItems: 'center', gap: '10px', padding: '7px 14px',
            backgroundColor: F.PANEL_BG, borderBottom: `1px solid ${F.BORDER}`, flexShrink: 0,
          }}>
            <span style={{ fontSize: '10px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>RESULTS</span>
            {results !== null && (
              <>
                <span style={{ fontSize: '10px', fontWeight: 700, color: F.CYAN }}>{results.length}</span>
                <span style={{ fontSize: '9px', color: F.MUTED }}>/ {scanned} scanned</span>
                {symbolErrors.length > 0 && (
                  <span style={{ fontSize: '9px', color: F.YELLOW, display: 'flex', alignItems: 'center', gap: '3px' }}>
                    <AlertTriangle size={10} /> {symbolErrors.length} warnings
                  </span>
                )}
              </>
            )}
            <div style={{ flex: 1 }} />
            {results && results.length > 0 && (
              <button onClick={exportToCSV} style={smallBtn}
                onMouseEnter={e => { e.currentTarget.style.borderColor = F.CYAN; e.currentTarget.style.color = F.CYAN; }}
                onMouseLeave={e => { e.currentTarget.style.borderColor = F.BORDER; e.currentTarget.style.color = F.GRAY; }}
              >
                <Download size={10} /> CSV
              </button>
            )}
            {debugLog.length > 0 && (
              <button onClick={() => setShowDebug(!showDebug)} style={{
                ...smallBtn,
                borderColor: showDebug ? `${F.YELLOW}50` : F.BORDER,
                color: showDebug ? F.YELLOW : F.GRAY,
              }}>
                {showDebug ? <ChevronDown size={10} /> : <ChevronRight size={10} />}
                LOG ({debugLog.length})
              </button>
            )}
          </div>

          {/* Error / Warning banners */}
          {error && (
            <div style={{ padding: '8px 14px', backgroundColor: `${F.RED}10`, borderBottom: `1px solid ${F.RED}30`, flexShrink: 0 }}>
              <div style={{ fontSize: '10px', fontWeight: 700, color: F.RED, marginBottom: '2px' }}>SCAN ERROR</div>
              <div style={{ fontSize: '10px', color: F.RED, whiteSpace: 'pre-wrap' }}>{error}</div>
            </div>
          )}
          {prefetchWarning && !error && (
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px', padding: '6px 14px', backgroundColor: `${F.YELLOW}08`, borderBottom: `1px solid ${F.YELLOW}20`, flexShrink: 0 }}>
              <AlertTriangle size={12} style={{ color: F.YELLOW, flexShrink: 0 }} />
              <span style={{ fontSize: '10px', color: F.YELLOW }}>{prefetchWarning}</span>
            </div>
          )}

          {/* Debug log (collapsible) */}
          {showDebug && debugLog.length > 0 && (
            <div style={{ maxHeight: '140px', overflow: 'auto', padding: '8px 14px', borderBottom: `1px solid ${F.BORDER}`, backgroundColor: F.PANEL_BG, flexShrink: 0 }}>
              {debugLog.map((line, i) => (
                <div key={i} style={{
                  fontSize: '9px', lineHeight: '1.6', fontFamily: font,
                  color: line.includes('ERROR') ? F.RED : line.includes('WARNING') ? F.YELLOW : line.includes('summary') ? F.CYAN : F.MUTED,
                }}>{line}</div>
              ))}
            </div>
          )}

          {/* Symbol errors */}
          {symbolErrors.length > 0 && !showDebug && (
            <div style={{ padding: '6px 14px', borderBottom: `1px solid ${F.BORDER}`, backgroundColor: `${F.YELLOW}06`, flexShrink: 0, maxHeight: '80px', overflow: 'auto' }}>
              {symbolErrors.map((se, i) => (
                <div key={i} style={{ fontSize: '9px', color: F.GRAY, lineHeight: '1.5' }}>
                  <span style={{ color: F.WHITE, fontWeight: 700 }}>{se.symbol}</span>: <span style={{ color: F.YELLOW }}>{se.error}</span>
                </div>
              ))}
            </div>
          )}

          {/* Results table */}
          <div style={{ flex: 1, overflow: 'auto' }}>
            {results === null && !scanning ? (
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', gap: '8px' }}>
                <Search size={28} style={{ color: F.MUTED, opacity: 0.3 }} />
                <span style={{ fontSize: '10px', color: F.MUTED }}>Configure conditions and run a scan</span>
              </div>
            ) : scanning ? (
              <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', gap: '8px' }}>
                <Loader size={16} className="animate-spin" style={{ color: F.ORANGE }} />
                <span style={{ fontSize: '10px', fontWeight: 600, color: F.GRAY }}>Scanning {symbolCount} symbols...</span>
              </div>
            ) : results && results.length === 0 ? (
              <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', gap: '6px' }}>
                <span style={{ fontSize: '10px', fontWeight: 600, color: F.GRAY }}>No matches found</span>
                <span style={{ fontSize: '9px', color: F.MUTED }}>Try different conditions or more symbols</span>
              </div>
            ) : results && (
              <table style={{ width: '100%', borderCollapse: 'collapse', fontFamily: font }}>
                <thead>
                  <tr style={{ borderBottom: `1px solid ${F.BORDER}`, position: 'sticky', top: 0, backgroundColor: F.PANEL_BG, zIndex: 1 }}>
                    <th style={{ padding: '6px 14px', fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.6px', textAlign: 'left', width: '36px' }}>#</th>
                    <th style={{ padding: '6px 8px', fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.6px', textAlign: 'left' }}>SYMBOL</th>
                    <th style={{ padding: '6px 8px', fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.6px', textAlign: 'right' }}>PRICE</th>
                    <th style={{ padding: '6px 8px', fontSize: '8px', fontWeight: 700, color: F.MUTED, letterSpacing: '0.6px', textAlign: 'left' }}>CONDITIONS</th>
                  </tr>
                </thead>
                <tbody>
                  {results.map((r, i) => (
                    <React.Fragment key={i}>
                      <tr
                        onClick={() => setExpandedResult(expandedResult === i ? null : i)}
                        style={{
                          cursor: 'pointer', borderBottom: `1px solid ${F.BORDER}15`,
                          backgroundColor: expandedResult === i ? `${F.ORANGE}08` : 'transparent',
                          transition: 'background-color 0.1s',
                        }}
                        onMouseEnter={e => { if (expandedResult !== i) e.currentTarget.style.backgroundColor = F.HOVER; }}
                        onMouseLeave={e => { if (expandedResult !== i) e.currentTarget.style.backgroundColor = expandedResult === i ? `${F.ORANGE}08` : 'transparent'; }}
                      >
                        <td style={{ padding: '7px 14px', fontSize: '9px', color: F.MUTED }}>{i + 1}</td>
                        <td style={{ padding: '7px 8px', fontSize: '10px', fontWeight: 700, color: F.WHITE }}>{r.symbol}</td>
                        <td style={{ padding: '7px 8px', fontSize: '10px', fontWeight: 700, color: F.CYAN, textAlign: 'right' }}>{r.price.toFixed(2)}</td>
                        <td style={{ padding: '7px 8px' }}>
                          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                            {r.details.map((d, j) => (
                              <span key={j} style={{
                                display: 'inline-flex', alignItems: 'center', gap: '3px', padding: '2px 6px', borderRadius: '2px',
                                fontSize: '9px', fontWeight: 700, fontFamily: font,
                                backgroundColor: d.met ? `${F.GREEN}12` : `${F.RED}12`,
                                borderWidth: '1px', borderStyle: 'solid', borderColor: d.met ? `${F.GREEN}30` : `${F.RED}30`,
                                color: d.met ? F.GREEN : F.RED,
                              }}>
                                {d.met ? '✓' : '✗'} {d.indicator}{d.field !== 'value' ? `.${d.field}` : ''}
                              </span>
                            ))}
                          </div>
                        </td>
                      </tr>
                      {/* Expanded detail */}
                      {expandedResult === i && (
                        <tr>
                          <td colSpan={4} style={{ padding: 0 }}>
                            <div style={{ padding: '10px 14px', backgroundColor: F.DARK_BG, borderBottom: `1px solid ${F.BORDER}` }}>
                              <div style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '6px' }}>
                                CONDITION DETAILS — {r.symbol}
                              </div>
                              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                                {r.details.map((d, j) => (
                                  <div key={j} style={{
                                    display: 'flex', alignItems: 'center', gap: '8px', padding: '5px 8px', borderRadius: '2px',
                                    backgroundColor: F.PANEL_BG, borderWidth: '1px', borderStyle: 'solid', borderColor: F.BORDER,
                                    fontSize: '10px', fontFamily: font,
                                  }}>
                                    <span style={{
                                      width: '18px', height: '18px', borderRadius: '2px', display: 'flex', alignItems: 'center', justifyContent: 'center',
                                      fontSize: '9px', fontWeight: 700, flexShrink: 0,
                                      backgroundColor: d.met ? `${F.GREEN}18` : `${F.RED}18`, color: d.met ? F.GREEN : F.RED,
                                    }}>{d.met ? '✓' : '✗'}</span>
                                    <div style={{ flex: 1 }}>{fmtDetail(d)}</div>
                                    {d.error && <span style={{ fontSize: '9px', color: F.RED, fontStyle: 'italic' }}>{d.error}</span>}
                                  </div>
                                ))}
                              </div>
                            </div>
                          </td>
                        </tr>
                      )}
                    </React.Fragment>
                  ))}
                </tbody>
              </table>
            )}
          </div>
        </div>
      </div>
    </div>
  );
};

export default ScannerPanel;
