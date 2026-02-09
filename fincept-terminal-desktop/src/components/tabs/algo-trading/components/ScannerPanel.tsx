import React, { useState } from 'react';
import { Search, Loader, ChevronDown, ChevronRight, AlertTriangle } from 'lucide-react';
import type { ConditionItem, ScanResult } from '../types';
import { TIMEFRAMES } from '../constants/indicators';
import { runAlgoScan } from '../services/algoTradingService';
import { useStockBrokerContextOptional } from '@/contexts/StockBrokerContext';
import { useBrokerContext } from '@/contexts/BrokerContext';
import ConditionBuilder from './ConditionBuilder';

const F = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', DARK_BG: '#000000', PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A', BORDER: '#2A2A2A', MUTED: '#4A4A4A', CYAN: '#00E5FF',
  YELLOW: '#FFD600',
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

interface SymbolError {
  symbol: string;
  error: string;
}

const STOCK_BROKERS = ['fyers', 'zerodha', 'upstox', 'dhan', 'angelone', 'kotak', 'groww', 'aliceblue', 'fivepaisa', 'iifl', 'motilal', 'shoonya'];

const ScannerPanel: React.FC = () => {
  const stockBroker = useStockBrokerContextOptional();
  const { activeBroker } = useBrokerContext();
  // For scanner, always prefer stock broker. Fallback to 'fyers' if the active broker is crypto-only.
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

    try {
      const result = await runAlgoScan(buildConditionsJson(), JSON.stringify(symbols), timeframe, provider);
      setScanning(false);

      // Capture debug info regardless of success/failure
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

  return (
    <div style={{ padding: '16px', display: 'flex', flexDirection: 'column', gap: '12px' }}>
      <span style={{ fontSize: '11px', fontWeight: 700, color: F.WHITE, letterSpacing: '0.5px' }}>SCANNER</span>

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
          <div style={{ fontSize: '9px', fontWeight: 700, color: F.RED, marginBottom: '2px' }}>
            SCAN ERROR
          </div>
          <div style={{ fontSize: '9px', color: F.RED, lineHeight: '1.4', whiteSpace: 'pre-wrap' }}>
            {error}
          </div>
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
          <div style={{ padding: '8px 12px', backgroundColor: F.HEADER_BG, borderBottom: `1px solid ${F.BORDER}` }}>
            <span style={{ fontSize: '9px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>
              RESULTS: <span style={{ color: F.CYAN }}>{results.length}</span> MATCH{results.length !== 1 ? 'ES' : ''} / {scanned} SCANNED
            </span>
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
                    <th style={{ padding: '6px 12px', textAlign: 'left', fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>SYMBOL</th>
                    <th style={{ padding: '6px 12px', textAlign: 'right', fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>PRICE</th>
                    <th style={{ padding: '6px 12px', textAlign: 'left', fontSize: '8px', fontWeight: 700, color: F.GRAY, letterSpacing: '0.5px' }}>INDICATORS</th>
                  </tr>
                </thead>
                <tbody>
                  {results.map((r, i) => (
                    <tr key={i} style={{ borderBottom: `1px solid ${F.BORDER}15` }}>
                      <td style={{ padding: '6px 12px', fontWeight: 700, color: F.WHITE }}>{r.symbol}</td>
                      <td style={{ padding: '6px 12px', textAlign: 'right', color: F.CYAN }}>{r.price.toFixed(2)}</td>
                      <td style={{ padding: '6px 12px' }}>
                        {r.details.map((d, j) => (
                          <span key={j} style={{ marginRight: '8px', color: d.met ? F.GREEN : F.RED, fontSize: '9px' }}>
                            {d.indicator}({d.field})={d.computed_value?.toFixed(2)}
                          </span>
                        ))}
                      </td>
                    </tr>
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
