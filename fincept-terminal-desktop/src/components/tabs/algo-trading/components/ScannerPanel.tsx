import React, { useState } from 'react';
import { Search, Loader } from 'lucide-react';
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

const ScannerPanel: React.FC = () => {
  const stockBroker = useStockBrokerContextOptional();
  const { activeBroker } = useBrokerContext();
  // Prefer stock broker (fyers, zerodha) over crypto broker
  const provider = stockBroker?.activeBroker || activeBroker || 'fyers';

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
    setScanning(true); setError(''); setResults(null);
    const result = await runAlgoScan(buildConditionsJson(), JSON.stringify(symbols), timeframe, provider);
    setScanning(false);
    if (result.success && result.data) { setResults(result.data.matches); setScanned(result.data.scanned); }
    else setError(result.error || 'Scan failed');
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

      {error && (
        <div style={{ padding: '6px 10px', backgroundColor: `${F.RED}20`, color: F.RED, fontSize: '9px', fontWeight: 700, borderRadius: '2px' }}>{error}</div>
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
