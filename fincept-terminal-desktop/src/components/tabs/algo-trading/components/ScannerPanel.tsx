import React, { useState } from 'react';
import { Search, Loader, ChevronDown, ChevronRight, AlertTriangle, Zap, List, RotateCcw } from 'lucide-react';
import type { ConditionItem, ScanResult, ConditionDetail } from '../types';
import { TIMEFRAMES, getIndicatorDef } from '../constants/indicators';
import { runAlgoScan } from '../services/algoTradingService';
import { useStockBrokerContextOptional } from '@/contexts/StockBrokerContext';
import { useBrokerContext } from '@/contexts/BrokerContext';
import ConditionBuilder from './ConditionBuilder';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A', BORDER: '#2A2A2A', MUTED: '#4A4A4A', CYAN: '#00E5FF',
  YELLOW: '#FFD600', PURPLE: '#9D4EDD', BLUE: '#0088FF',
};

const inputStyle: React.CSSProperties = {
  width: '100%', padding: '8px 10px', backgroundColor: F.DARK_BG,
  color: F.WHITE, border: `1px solid ${F.BORDER}`, borderRadius: '2px',
  fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace', outline: 'none',
};

const labelStyle: React.CSSProperties = {
  fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px',
  marginBottom: '4px', display: 'block',
};

const chipStyle: React.CSSProperties = {
  padding: '3px 8px', backgroundColor: 'transparent', border: `1px solid ${F.BORDER}`,
  borderRadius: '2px', color: F.GRAY, fontSize: '8px', fontWeight: 700,
  letterSpacing: '0.5px', cursor: 'pointer', transition: 'all 0.15s',
  fontFamily: '"IBM Plex Mono", monospace',
};

interface SymbolError {
  symbol: string;
  error: string;
}

// ── Scan Presets (Chartink-style templates) ──────────────────────────────
interface ScanPreset {
  name: string;
  description: string;
  conditions: ConditionItem[];
  logic: 'AND' | 'OR';
}

const SCAN_PRESETS: ScanPreset[] = [
  {
    name: 'RSI Oversold',
    description: 'RSI(14) < 30 — oversold stocks',
    conditions: [
      { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: '<', value: 30 },
    ],
    logic: 'AND',
  },
  {
    name: 'RSI Overbought',
    description: 'RSI(14) > 70 — overbought stocks',
    conditions: [
      { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: '>', value: 70 },
    ],
    logic: 'AND',
  },
  {
    name: 'Golden Cross',
    description: 'EMA(50) crosses above SMA(200)',
    conditions: [
      {
        indicator: 'EMA', params: { period: 50 }, field: 'value', operator: 'crosses_above', value: 0,
        compareMode: 'indicator', compareIndicator: 'SMA', compareParams: { period: 200 }, compareField: 'value',
      },
    ],
    logic: 'AND',
  },
  {
    name: 'Death Cross',
    description: 'EMA(50) crosses below SMA(200)',
    conditions: [
      {
        indicator: 'EMA', params: { period: 50 }, field: 'value', operator: 'crosses_below', value: 0,
        compareMode: 'indicator', compareIndicator: 'SMA', compareParams: { period: 200 }, compareField: 'value',
      },
    ],
    logic: 'AND',
  },
  {
    name: 'MACD Bullish Cross',
    description: 'MACD line crosses above signal line',
    conditions: [
      {
        indicator: 'MACD', params: { fast: 12, slow: 26, signal: 9 }, field: 'line', operator: 'crosses_above', value: 0,
        compareMode: 'indicator', compareIndicator: 'MACD', compareParams: { fast: 12, slow: 26, signal: 9 }, compareField: 'signal_line',
      },
    ],
    logic: 'AND',
  },
  {
    name: 'Bollinger Squeeze',
    description: 'Price below lower Bollinger Band + RSI < 35',
    conditions: [
      {
        indicator: 'CLOSE', params: {}, field: 'value', operator: '<', value: 0,
        compareMode: 'indicator', compareIndicator: 'BOLLINGER', compareParams: { period: 20, std_dev: 2 }, compareField: 'lower',
      },
      { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: '<', value: 35 },
    ],
    logic: 'AND',
  },
  {
    name: 'Volume Breakout',
    description: 'Volume > 2x SMA(20) of volume + price rising 3 bars',
    conditions: [
      { indicator: 'CLOSE', params: {}, field: 'value', operator: 'rising', value: 0, lookback: 3 },
      { indicator: 'MFI', params: { period: 14 }, field: 'value', operator: '>', value: 60 },
    ],
    logic: 'AND',
  },
  {
    name: 'Supertrend Buy',
    description: 'Supertrend direction is bullish (1)',
    conditions: [
      { indicator: 'SUPERTREND', params: { period: 10, multiplier: 3 }, field: 'direction', operator: '==', value: 1 },
    ],
    logic: 'AND',
  },
  {
    name: 'RSI + MACD Combo',
    description: 'RSI rising + MACD histogram > 0',
    conditions: [
      { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: 'rising', value: 0, lookback: 3 },
      { indicator: 'MACD', params: { fast: 12, slow: 26, signal: 9 }, field: 'histogram', operator: '>', value: 0 },
    ],
    logic: 'AND',
  },
  {
    name: 'Price Above VWAP',
    description: 'Close crosses above VWAP (intraday)',
    conditions: [
      {
        indicator: 'CLOSE', params: {}, field: 'value', operator: 'crosses_above', value: 0,
        compareMode: 'indicator', compareIndicator: 'VWAP', compareParams: {}, compareField: 'value',
      },
    ],
    logic: 'AND',
  },
];

// ── Symbol Watchlists ────────────────────────────────────────────────────
const SYMBOL_WATCHLISTS: Record<string, string[]> = {
  'NIFTY 50': [
    'RELIANCE', 'TCS', 'HDFCBANK', 'INFY', 'ICICIBANK', 'HINDUNILVR', 'ITC', 'SBIN',
    'BHARTIARTL', 'KOTAKBANK', 'LT', 'AXISBANK', 'WIPRO', 'ASIANPAINT', 'MARUTI',
    'BAJFINANCE', 'HCLTECH', 'SUNPHARMA', 'TITAN', 'TATASTEEL', 'ULTRACEMCO',
    'ONGC', 'NTPC', 'POWERGRID', 'M&M', 'NESTLEIND', 'ADANIENT', 'JSWSTEEL',
    'BAJAJFINSV', 'TECHM', 'INDUSINDBK', 'HINDALCO', 'TATAMOTORS', 'COALINDIA',
    'GRASIM', 'SBILIFE', 'CIPLA', 'DIVISLAB', 'DRREDDY', 'BPCL',
    'APOLLOHOSP', 'EICHERMOT', 'TATACONSUM', 'BRITANNIA', 'HDFCLIFE',
    'HEROMOTOCO', 'BAJAJ-AUTO', 'LTIM', 'ADANIPORTS', 'UPL',
  ],
  'BANK NIFTY': [
    'HDFCBANK', 'ICICIBANK', 'KOTAKBANK', 'AXISBANK', 'SBIN', 'INDUSINDBK',
    'BANKBARODA', 'AUBANK', 'FEDERALBNK', 'BANDHANBNK', 'PNB', 'IDFCFIRSTB',
  ],
  'IT STOCKS': [
    'TCS', 'INFY', 'WIPRO', 'HCLTECH', 'TECHM', 'LTIM', 'MPHASIS', 'COFORGE',
    'PERSISTENT', 'TATAELXSI', 'LTTS', 'MINDTREE',
  ],
  'PHARMA': [
    'SUNPHARMA', 'DRREDDY', 'CIPLA', 'DIVISLAB', 'AUROPHARMA', 'LUPIN',
    'BIOCON', 'TORNTPHARM', 'ALKEM', 'GLENMARK',
  ],
};

const STOCK_BROKERS = ['fyers', 'zerodha', 'upstox', 'dhan', 'angelone', 'kotak', 'groww', 'aliceblue', 'fivepaisa', 'iifl', 'motilal', 'shoonya'];

// ── Helper to format condition detail for display ────────────────────────
function formatConditionDetail(d: ConditionDetail): React.ReactNode {
  const opLabels: Record<string, string> = {
    '>': '>', '<': '<', '>=': '>=', '<=': '<=', '==': '==',
    'crosses_above': 'X↑', 'crosses_below': 'X↓', 'between': 'BTW',
    'crossed_above_within': 'X↑ N', 'crossed_below_within': 'X↓ N',
    'rising': '↑N', 'falling': '↓N',
  };
  const op = opLabels[d.operator] || d.operator;
  const val = d.computed_value != null ? d.computed_value.toFixed(2) : '—';

  if (d.target_indicator) {
    const targetVal = d.target_computed_value != null ? d.target_computed_value.toFixed(2) : '—';
    return (
      <span>
        <span style={{ color: F.WHITE, fontWeight: 700 }}>{d.indicator}</span>
        {d.field !== 'value' && <span style={{ color: F.MUTED }}>.{d.field}</span>}
        <span style={{ color: F.CYAN }}>={val}</span>
        <span style={{ color: F.ORANGE, margin: '0 3px' }}>{op}</span>
        <span style={{ color: F.YELLOW, fontWeight: 700 }}>{d.target_indicator}</span>
        <span style={{ color: F.CYAN }}>={targetVal}</span>
      </span>
    );
  }

  return (
    <span>
      <span style={{ color: F.WHITE, fontWeight: 700 }}>{d.indicator}</span>
      {d.field !== 'value' && <span style={{ color: F.MUTED }}>.{d.field}</span>}
      <span style={{ color: F.CYAN }}>={val}</span>
      <span style={{ color: F.ORANGE, margin: '0 3px' }}>{op}</span>
      <span style={{ color: F.MUTED }}>{d.target}</span>
    </span>
  );
}

const ScannerPanel: React.FC = () => {
  const stockBroker = useStockBrokerContextOptional();
  const { activeBroker } = useBrokerContext();
  const provider = stockBroker?.activeBroker
    || (STOCK_BROKERS.includes(activeBroker) ? activeBroker : null)
    || 'fyers';

  const [conditions, setConditions] = useState<ConditionItem[]>([
    { indicator: 'RSI', params: { period: 14 }, field: 'value', operator: '<', value: 30 },
  ]);
  const [logic, setLogic] = useState<'AND' | 'OR'>('AND');
  const [symbolsInput, setSymbolsInput] = useState('');
  const [timeframe, setTimeframe] = useState('1d');
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

    setScanning(true);
    setError('');
    setResults(null);
    setDebugLog([]);
    setPrefetchWarning('');
    setSymbolErrors([]);
    setExpandedResult(null);

    try {
      const result = await runAlgoScan(buildConditionsJson(), JSON.stringify(symbols), timeframe, provider);
      setScanning(false);

      if (result.debug) setDebugLog(result.debug);
      if (result.prefetch_warning) setPrefetchWarning(result.prefetch_warning);

      if (result.success && result.data) {
        setResults(result.data.matches);
        setScanned(result.data.scanned);
        if (result.data.errors && result.data.errors.length > 0) {
          setSymbolErrors(result.data.errors);
        }
        if (result.data.debug) setDebugLog(result.data.debug);
        if (result.data.prefetch_warning) setPrefetchWarning(result.data.prefetch_warning);
      } else {
        setError(result.error || 'Scan failed — check debug log for details');
      }
    } catch (e) {
      setScanning(false);
      setError(`Unexpected error: ${e instanceof Error ? e.message : String(e)}`);
    }
  };

  const applyPreset = (preset: ScanPreset) => {
    // Deep clone to avoid mutating preset constants (params/compareParams are nested objects)
    setConditions(JSON.parse(JSON.stringify(preset.conditions)));
    setLogic(preset.logic);
    setShowPresets(false);
  };

  const appendWatchlist = (name: string) => {
    const symbols = SYMBOL_WATCHLISTS[name];
    if (!symbols) return;
    const existing = symbolsInput.split(/[,\n]/).map(s => s.trim().toUpperCase()).filter(Boolean);
    const newSymbols = symbols.filter(s => !existing.includes(s));
    const merged = [...existing, ...newSymbols];
    setSymbolsInput(merged.join(', '));
  };

  // Build a summary of the current scan conditions
  const conditionSummary = conditions.map(c => {
    const def = getIndicatorDef(c.indicator);
    const label = def?.label || c.indicator;
    const paramValues = Object.values(c.params);
    const paramStr = paramValues.length > 0 ? `(${paramValues.join(',')})` : '';
    const fieldStr = c.field !== 'value' ? `.${c.field}` : '';
    const opStr = c.operator;
    if (c.compareMode === 'indicator' && c.compareIndicator) {
      const cDef = getIndicatorDef(c.compareIndicator);
      const cLabel = cDef?.label || c.compareIndicator;
      const cParamValues = Object.values(c.compareParams || {});
      const cParamStr = cParamValues.length > 0 ? `(${cParamValues.join(',')})` : '';
      return `${label}${paramStr}${fieldStr} ${opStr} ${cLabel}${cParamStr}`;
    }
    return `${label}${paramStr}${fieldStr} ${opStr} ${c.value}`;
  }).join(` ${logic} `);

  return (
    <div style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Scanner Header + Presets Toggle */}
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between' }}>
        <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>SCANNER</span>
        <div style={{ display: 'flex', gap: '6px' }}>
          <button
            onClick={() => setShowPresets(!showPresets)}
            style={{
              ...chipStyle,
              color: showPresets ? F.ORANGE : F.GRAY,
              borderColor: showPresets ? F.ORANGE : F.BORDER,
            }}
          >
            <Zap size={8} style={{ marginRight: '3px', display: 'inline' }} />
            PRESETS
          </button>
          <button
            onClick={() => {
              setConditions([{ indicator: 'RSI', params: { period: 14 }, field: 'value', operator: '<', value: 30 }]);
              setLogic('AND');
              setResults(null);
              setError('');
              setDebugLog([]);
              setPrefetchWarning('');
              setSymbolErrors([]);
            }}
            style={chipStyle}
          >
            <RotateCcw size={8} style={{ marginRight: '3px', display: 'inline' }} />
            RESET
          </button>
        </div>
      </div>

      {/* Scan Presets Panel */}
      {showPresets && (
        <div style={{
          backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px',
          padding: '8px',
        }}>
          <div style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '8px' }}>
            SCAN TEMPLATES — CLICK TO APPLY
          </div>
          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))', gap: '6px' }}>
            {SCAN_PRESETS.map((preset, i) => (
              <button
                key={i}
                onClick={() => applyPreset(preset)}
                style={{
                  padding: '8px 10px', backgroundColor: F.DARK_BG, border: `1px solid ${F.BORDER}`,
                  borderRadius: '2px', cursor: 'pointer', textAlign: 'left', transition: 'all 0.15s',
                }}
                onMouseEnter={e => {
                  (e.currentTarget as HTMLElement).style.borderColor = F.ORANGE;
                  (e.currentTarget as HTMLElement).style.backgroundColor = F.HEADER_BG;
                }}
                onMouseLeave={e => {
                  (e.currentTarget as HTMLElement).style.borderColor = F.BORDER;
                  (e.currentTarget as HTMLElement).style.backgroundColor = F.DARK_BG;
                }}
              >
                <div style={{ fontSize: '9px', fontWeight: 700, color: F.ORANGE, marginBottom: '2px' }}>
                  {preset.name}
                </div>
                <div style={{ fontSize: '8px', color: F.MUTED, lineHeight: '1.4' }}>
                  {preset.description}
                </div>
              </button>
            ))}
          </div>
        </div>
      )}

      {/* Active Condition Summary */}
      {conditions.length > 0 && (
        <div style={{
          padding: '6px 10px', backgroundColor: `${F.ORANGE}08`, border: `1px solid ${F.ORANGE}20`,
          borderRadius: '2px', fontSize: '9px', color: F.ORANGE, fontFamily: '"IBM Plex Mono", monospace',
        }}>
          <span style={{ fontWeight: 700, marginRight: '6px' }}>SCAN:</span>
          {conditionSummary}
        </div>
      )}

      {/* Condition Builder */}
      <ConditionBuilder label="Scan Conditions" conditions={conditions} logic={logic}
        onConditionsChange={setConditions} onLogicChange={setLogic} />

      {/* Symbol Universe + Timeframe */}
      <div style={{ display: 'grid', gridTemplateColumns: '2fr 1fr', gap: '12px' }}>
        <div>
          <label style={labelStyle}>SYMBOLS (COMMA OR NEWLINE SEPARATED)</label>
          <textarea
            value={symbolsInput}
            onChange={e => setSymbolsInput(e.target.value)}
            placeholder="RELIANCE, TCS, INFY, HDFCBANK, ICICIBANK"
            rows={3}
            style={{ ...inputStyle, resize: 'none' } as React.CSSProperties}
          />
          {/* Watchlist Quick-Add */}
          <div style={{ display: 'flex', gap: '4px', marginTop: '4px', flexWrap: 'wrap' }}>
            {Object.keys(SYMBOL_WATCHLISTS).map(name => (
              <button
                key={name}
                onClick={() => appendWatchlist(name)}
                style={{
                  ...chipStyle,
                  padding: '2px 6px',
                  fontSize: '7px',
                }}
                onMouseEnter={e => {
                  (e.currentTarget as HTMLElement).style.borderColor = F.CYAN;
                  (e.currentTarget as HTMLElement).style.color = F.CYAN;
                }}
                onMouseLeave={e => {
                  (e.currentTarget as HTMLElement).style.borderColor = F.BORDER;
                  (e.currentTarget as HTMLElement).style.color = F.GRAY;
                }}
                title={`Add ${SYMBOL_WATCHLISTS[name].length} symbols`}
              >
                + {name} ({SYMBOL_WATCHLISTS[name].length})
              </button>
            ))}
          </div>
        </div>
        <div>
          <label style={labelStyle}>TIMEFRAME</label>
          <select value={timeframe} onChange={e => setTimeframe(e.target.value)} style={inputStyle}>
            {TIMEFRAMES.map(tf => <option key={tf.value} value={tf.value}>{tf.label}</option>)}
          </select>
          <button onClick={handleScan} disabled={scanning} style={{
            width: '100%', marginTop: '8px', padding: '8px 16px',
            backgroundColor: F.ORANGE, color: F.DARK_BG, border: 'none', borderRadius: '2px',
            fontSize: '9px', fontWeight: 700, cursor: 'pointer', opacity: scanning ? 0.5 : 1,
            display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '4px',
          }}>
            {scanning ? <Loader size={10} className="animate-spin" /> : <Search size={10} />}
            {scanning ? 'SCANNING...' : 'RUN SCAN'}
          </button>
        </div>
      </div>

      {/* Error Display */}
      {error && (
        <div style={{ padding: '8px 10px', backgroundColor: `${F.RED}15`, border: `1px solid ${F.RED}40`, borderRadius: '2px' }}>
          <div style={{ fontSize: '9px', fontWeight: 700, color: F.RED, marginBottom: '2px' }}>SCAN ERROR</div>
          <div style={{ fontSize: '9px', color: F.RED, lineHeight: '1.4', whiteSpace: 'pre-wrap' }}>{error}</div>
        </div>
      )}

      {/* Prefetch Warning */}
      {prefetchWarning && !error && (
        <div style={{
          padding: '6px 10px', backgroundColor: `${F.YELLOW}12`, border: `1px solid ${F.YELLOW}30`,
          borderRadius: '2px', display: 'flex', alignItems: 'center', gap: '6px',
        }}>
          <AlertTriangle size={10} color={F.YELLOW} />
          <span style={{ fontSize: '9px', color: F.YELLOW }}>{prefetchWarning}</span>
        </div>
      )}

      {/* Symbol-level errors */}
      {symbolErrors.length > 0 && (
        <div style={{ padding: '6px 10px', backgroundColor: `${F.YELLOW}10`, border: `1px solid ${F.BORDER}`, borderRadius: '2px' }}>
          <div style={{ fontSize: '8px', fontWeight: 700, color: F.YELLOW, letterSpacing: '0.5px', marginBottom: '4px' }}>
            SYMBOL WARNINGS ({symbolErrors.length})
          </div>
          {symbolErrors.map((se, i) => (
            <div key={i} style={{ fontSize: '9px', color: F.GRAY, lineHeight: '1.4' }}>
              <span style={{ color: F.WHITE, fontWeight: 700 }}>{se.symbol}</span>: {se.error}
            </div>
          ))}
        </div>
      )}

      {/* Debug Log (collapsible) */}
      {debugLog.length > 0 && (
        <div style={{ backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px' }}>
          <button
            onClick={() => setShowDebug(!showDebug)}
            style={{
              width: '100%', padding: '6px 10px', backgroundColor: F.HEADER_BG,
              border: 'none', borderBottom: showDebug ? `1px solid ${F.BORDER}` : 'none',
              color: F.MUTED, fontSize: '8px', fontWeight: 700, letterSpacing: '0.5px',
              cursor: 'pointer', display: 'flex', alignItems: 'center', gap: '4px',
              textAlign: 'left',
            }}
          >
            {showDebug ? <ChevronDown size={10} /> : <ChevronRight size={10} />}
            DEBUG LOG ({debugLog.length} entries)
          </button>
          {showDebug && (
            <div style={{
              padding: '8px 10px', maxHeight: '200px', overflow: 'auto',
              fontFamily: '"IBM Plex Mono", monospace', fontSize: '8px', lineHeight: '1.5',
            }}>
              {debugLog.map((line, i) => (
                <div key={i} style={{
                  color: line.includes('ERROR') ? F.RED
                    : line.includes('WARNING') ? F.YELLOW
                    : line.includes('summary') ? F.CYAN
                    : F.MUTED,
                }}>
                  {line}
                </div>
              ))}
            </div>
          )}
        </div>
      )}

      {/* Results */}
      {results !== null && (
        <div style={{ backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`, borderRadius: '2px' }}>
          <div style={{
            padding: '8px 12px', backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}`,
            display: 'flex', alignItems: 'center', justifyContent: 'space-between',
          }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
              RESULTS: <span style={{ color: F.CYAN }}>{results.length}</span> MATCH{results.length !== 1 ? 'ES' : ''} / {scanned} SCANNED
            </span>
            {results.length > 0 && (
              <span style={{ fontSize: '8px', color: F.MUTED }}>
                <List size={8} style={{ marginRight: '3px', display: 'inline' }} />
                CLICK ROW FOR DETAILS
              </span>
            )}
          </div>
          {results.length === 0 ? (
            <div style={{ padding: '24px', textAlign: 'center', fontSize: '10px', color: F.MUTED }}>
              No symbols match the conditions
            </div>
          ) : (
            <div style={{ overflow: 'auto' }}>
              <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '9px', fontFamily: '"IBM Plex Mono", monospace' }}>
                <thead>
                  <tr style={{ borderBottom: `1px solid ${F.BORDER}` }}>
                    <th style={{ padding: '6px 12px', textAlign: 'left', fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>#</th>
                    <th style={{ padding: '6px 12px', textAlign: 'left', fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>SYMBOL</th>
                    <th style={{ padding: '6px 12px', textAlign: 'right', fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>PRICE</th>
                    <th style={{ padding: '6px 12px', textAlign: 'left', fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>CONDITIONS</th>
                    <th style={{ padding: '6px 12px', textAlign: 'center', fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>STATUS</th>
                  </tr>
                </thead>
                <tbody>
                  {results.map((r, i) => (
                    <React.Fragment key={i}>
                      <tr
                        onClick={() => setExpandedResult(expandedResult === i ? null : i)}
                        style={{
                          borderBottom: `1px solid ${F.BORDER}15`,
                          cursor: 'pointer',
                          backgroundColor: expandedResult === i ? `${F.ORANGE}08` : 'transparent',
                          transition: 'background-color 0.15s',
                        }}
                        onMouseEnter={e => {
                          if (expandedResult !== i) (e.currentTarget as HTMLElement).style.backgroundColor = F.HEADER_BG;
                        }}
                        onMouseLeave={e => {
                          if (expandedResult !== i) (e.currentTarget as HTMLElement).style.backgroundColor = 'transparent';
                        }}
                      >
                        <td style={{ padding: '6px 12px', color: F.MUTED }}>{i + 1}</td>
                        <td style={{ padding: '6px 12px', fontWeight: 700, color: F.WHITE }}>{r.symbol}</td>
                        <td style={{ padding: '6px 12px', textAlign: 'right', color: F.CYAN, fontWeight: 700 }}>{r.price.toFixed(2)}</td>
                        <td style={{ padding: '6px 12px' }}>
                          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '4px' }}>
                            {r.details.map((d, j) => (
                              <span key={j} style={{
                                display: 'inline-flex', alignItems: 'center', gap: '3px',
                                padding: '1px 5px', borderRadius: '2px', fontSize: '8px',
                                backgroundColor: d.met ? `${F.GREEN}15` : `${F.RED}15`,
                                border: `1px solid ${d.met ? F.GREEN : F.RED}30`,
                                color: d.met ? F.GREEN : F.RED,
                              }}>
                                {d.met ? '✓' : '✗'} {d.indicator}{d.field !== 'value' ? `.${d.field}` : ''}
                              </span>
                            ))}
                          </div>
                        </td>
                        <td style={{ padding: '6px 12px', textAlign: 'center' }}>
                          <span style={{
                            padding: '2px 6px', borderRadius: '2px', fontSize: '8px', fontWeight: 700,
                            backgroundColor: `${F.GREEN}15`, color: F.GREEN, border: `1px solid ${F.GREEN}40`,
                          }}>
                            MATCH
                          </span>
                        </td>
                      </tr>
                      {/* Expanded detail row */}
                      {expandedResult === i && (
                        <tr>
                          <td colSpan={5} style={{ padding: '0' }}>
                            <div style={{
                              padding: '8px 12px', backgroundColor: `${F.DARK_BG}`,
                              borderBottom: `1px solid ${F.BORDER}`,
                            }}>
                              <div style={{ fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px', marginBottom: '6px' }}>
                                CONDITION DETAILS FOR {r.symbol}
                              </div>
                              <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                                {r.details.map((d, j) => (
                                  <div key={j} style={{
                                    display: 'flex', alignItems: 'center', gap: '8px',
                                    padding: '4px 8px', backgroundColor: F.PANEL_BG,
                                    borderRadius: '2px', border: `1px solid ${F.BORDER}`,
                                    fontSize: '9px',
                                  }}>
                                    <span style={{
                                      width: '16px', height: '16px', borderRadius: '2px',
                                      display: 'flex', alignItems: 'center', justifyContent: 'center',
                                      fontSize: '8px', fontWeight: 700, flexShrink: 0,
                                      backgroundColor: d.met ? `${F.GREEN}20` : `${F.RED}20`,
                                      color: d.met ? F.GREEN : F.RED,
                                    }}>
                                      {d.met ? '✓' : '✗'}
                                    </span>
                                    <div style={{ flex: 1 }}>
                                      {formatConditionDetail(d)}
                                    </div>
                                    {d.error && (
                                      <span style={{ fontSize: '8px', color: F.RED, fontStyle: 'italic' }}>
                                        {d.error}
                                      </span>
                                    )}
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
            </div>
          )}
        </div>
      )}
    </div>
  );
};

export default ScannerPanel;
