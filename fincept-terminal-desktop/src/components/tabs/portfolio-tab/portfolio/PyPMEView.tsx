import React, { useState } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, LineChart, Line, Legend } from 'recharts';
import { Activity, Play, Loader2, Plus, Trash2, TrendingUp } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, EFFECTS, COMMON_STYLES } from '../finceptStyles';
import type { PortfolioSummary } from '../../../../services/portfolio/portfolioService';

const COLORS = {
  ORANGE: FINCEPT.ORANGE,
  WHITE: FINCEPT.WHITE,
  RED: FINCEPT.RED,
  GREEN: FINCEPT.GREEN,
  YELLOW: FINCEPT.YELLOW,
  GRAY: FINCEPT.GRAY,
  CYAN: FINCEPT.CYAN,
  BLUE: FINCEPT.BLUE,
  PURPLE: FINCEPT.PURPLE,
  DARK_BG: FINCEPT.DARK_BG,
  PANEL_BG: FINCEPT.PANEL_BG,
  HEADER_BG: FINCEPT.HEADER_BG,
  HOVER: FINCEPT.HOVER,
  BORDER: FINCEPT.BORDER,
  MUTED: FINCEPT.MUTED,
};

type PMEMode = 'basic' | 'extended' | 'tessa';

interface CashflowEntry {
  date: string;
  amount: number;
  price: number;
  pmePrice: number;
}

interface PyPMEViewProps {
  portfolioSummary: PortfolioSummary;
}

const PyPMEView: React.FC<PyPMEViewProps> = ({ portfolioSummary }) => {
  const [mode, setMode] = useState<PMEMode>('basic');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<any>(null);

  // Basic/Verbose PME inputs
  const [cashflows, setCashflows] = useState<number[]>([-1000, -500, 200]);
  const [prices, setPrices] = useState<number[]>([100, 110, 120, 130]);
  const [pmePrices, setPmePrices] = useState<number[]>([100, 105, 112, 118]);

  // Extended PME inputs
  const [entries, setEntries] = useState<CashflowEntry[]>([
    { date: '2020-01-01', amount: -1000, price: 100, pmePrice: 100 },
    { date: '2021-01-01', amount: -500, price: 110, pmePrice: 105 },
    { date: '2022-01-01', amount: 200, price: 130, pmePrice: 118 },
  ]);

  // Tessa inputs
  const [pmeTicker, setPmeTicker] = useState('SPY');
  const [pmeSource, setPmeSource] = useState('yahoo');
  const [verbose, setVerbose] = useState(true);

  const addEntry = () => {
    setEntries([...entries, { date: '', amount: 0, price: 0, pmePrice: 0 }]);
  };

  const removeEntry = (idx: number) => {
    setEntries(entries.filter((_, i) => i !== idx));
  };

  const updateEntry = (idx: number, field: keyof CashflowEntry, value: string | number) => {
    const updated = [...entries];
    (updated[idx] as any)[field] = value;
    setEntries(updated);
  };

  const calculate = async () => {
    setLoading(true);
    setError(null);
    setResult(null);

    try {
      let raw: string;

      if (mode === 'basic') {
        if (verbose) {
          raw = await invoke<string>('pypme_verbose', {
            cashflows,
            prices,
            pmePrices,
          });
        } else {
          raw = await invoke<string>('pypme_calculate', {
            cashflows,
            prices,
            pmePrices,
          });
        }
      } else if (mode === 'extended') {
        const dates = entries.map(e => e.date);
        const cf = entries.map(e => e.amount);
        const pr = entries.map(e => e.price);
        const pp = entries.map(e => e.pmePrice);

        if (verbose) {
          raw = await invoke<string>('pypme_verbose_xpme', {
            dates,
            cashflows: cf,
            prices: pr,
            pmePrices: pp,
          });
        } else {
          raw = await invoke<string>('pypme_xpme', {
            dates,
            cashflows: cf,
            prices: pr,
            pmePrices: pp,
          });
        }
      } else {
        // tessa
        const dates = entries.map(e => e.date);
        const cf = entries.map(e => e.amount);
        const pr = entries.map(e => e.price);

        if (verbose) {
          raw = await invoke<string>('pypme_tessa_verbose_xpme', {
            dates,
            cashflows: cf,
            prices: pr,
            pmeTicker: pmeTicker,
            pmeSource: pmeSource,
          });
        } else {
          raw = await invoke<string>('pypme_tessa_xpme', {
            dates,
            cashflows: cf,
            prices: pr,
            pmeTicker: pmeTicker,
            pmeSource: pmeSource,
          });
        }
      }

      const parsed = JSON.parse(raw);
      if (parsed.error) {
        setError(parsed.error);
      } else {
        setResult(parsed);
      }
    } catch (e: any) {
      setError(e?.toString() || 'PME calculation failed');
    } finally {
      setLoading(false);
    }
  };

  const inputStyle: React.CSSProperties = {
    backgroundColor: COLORS.DARK_BG,
    color: COLORS.WHITE,
    border: `1px solid ${COLORS.BORDER}`,
    padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
    fontSize: TYPOGRAPHY.BODY,
    fontFamily: TYPOGRAPHY.MONO,
    width: '100%',
  };

  const labelStyle: React.CSSProperties = {
    fontSize: TYPOGRAPHY.TINY,
    color: COLORS.GRAY,
    letterSpacing: '0.5px',
    marginBottom: '2px',
    textTransform: 'uppercase' as const,
  };

  const renderBasicInputs = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
      <div>
        <div style={labelStyle}>CASHFLOWS (comma-separated, negative = investment)</div>
        <input
          type="text"
          value={cashflows.join(', ')}
          onChange={e => setCashflows(e.target.value.split(',').map(v => parseFloat(v.trim()) || 0))}
          style={inputStyle}
          placeholder="-1000, -500, 200"
        />
      </div>
      <div>
        <div style={labelStyle}>FUND PRICES (one per period, include final NAV)</div>
        <input
          type="text"
          value={prices.join(', ')}
          onChange={e => setPrices(e.target.value.split(',').map(v => parseFloat(v.trim()) || 0))}
          style={inputStyle}
          placeholder="100, 110, 120, 130"
        />
      </div>
      <div>
        <div style={labelStyle}>BENCHMARK PRICES (public market index, same periods)</div>
        <input
          type="text"
          value={pmePrices.join(', ')}
          onChange={e => setPmePrices(e.target.value.split(',').map(v => parseFloat(v.trim()) || 0))}
          style={inputStyle}
          placeholder="100, 105, 112, 118"
        />
      </div>
    </div>
  );

  const renderExtendedInputs = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.SMALL }}>
      <div style={{ display: 'flex', gap: SPACING.SMALL, fontSize: TYPOGRAPHY.TINY, color: COLORS.GRAY, textTransform: 'uppercase', letterSpacing: '0.5px' }}>
        <div style={{ flex: 2 }}>DATE</div>
        <div style={{ flex: 2 }}>CASHFLOW</div>
        <div style={{ flex: 2 }}>FUND PRICE</div>
        {mode === 'extended' && <div style={{ flex: 2 }}>BENCHMARK PRICE</div>}
        <div style={{ width: 28 }}></div>
      </div>
      {entries.map((entry, i) => (
        <div key={i} style={{ display: 'flex', gap: SPACING.SMALL, alignItems: 'center' }}>
          <input
            type="date"
            value={entry.date}
            onChange={e => updateEntry(i, 'date', e.target.value)}
            style={{ ...inputStyle, flex: 2 }}
          />
          <input
            type="number"
            value={entry.amount}
            onChange={e => updateEntry(i, 'amount', parseFloat(e.target.value) || 0)}
            style={{ ...inputStyle, flex: 2 }}
            placeholder="Cashflow"
          />
          <input
            type="number"
            value={entry.price}
            onChange={e => updateEntry(i, 'price', parseFloat(e.target.value) || 0)}
            style={{ ...inputStyle, flex: 2 }}
            placeholder="Fund Price"
          />
          {mode === 'extended' && (
            <input
              type="number"
              value={entry.pmePrice}
              onChange={e => updateEntry(i, 'pmePrice', parseFloat(e.target.value) || 0)}
              style={{ ...inputStyle, flex: 2 }}
              placeholder="Benchmark"
            />
          )}
          <button
            onClick={() => removeEntry(i)}
            style={{
              width: 28,
              height: 28,
              backgroundColor: 'transparent',
              border: `1px solid ${COLORS.BORDER}`,
              color: COLORS.RED,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
            }}
          >
            <Trash2 size={12} />
          </button>
        </div>
      ))}
      <button
        onClick={addEntry}
        style={{
          padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
          backgroundColor: 'transparent',
          border: `1px dashed ${COLORS.BORDER}`,
          color: COLORS.GRAY,
          fontSize: TYPOGRAPHY.SMALL,
          cursor: 'pointer',
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          gap: '4px',
        }}
      >
        <Plus size={12} /> ADD ROW
      </button>
    </div>
  );

  const renderTessaInputs = () => (
    <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
      {renderExtendedInputs()}
      <div style={{ display: 'flex', gap: SPACING.DEFAULT }}>
        <div style={{ flex: 1 }}>
          <div style={labelStyle}>BENCHMARK TICKER</div>
          <input
            type="text"
            value={pmeTicker}
            onChange={e => setPmeTicker(e.target.value.toUpperCase())}
            style={inputStyle}
            placeholder="SPY"
          />
        </div>
        <div style={{ flex: 1 }}>
          <div style={labelStyle}>DATA SOURCE</div>
          <select
            value={pmeSource}
            onChange={e => setPmeSource(e.target.value)}
            style={inputStyle}
          >
            <option value="yahoo">Yahoo Finance</option>
            <option value="coingecko">CoinGecko</option>
          </select>
        </div>
      </div>
    </div>
  );

  const renderResults = () => {
    if (!result) return null;

    const pmeValue = result.pme ?? result.xpme;
    const isOutperforming = pmeValue > 1.0;

    return (
      <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT }}>
        {/* PME Score Card */}
        <div style={{
          display: 'flex',
          gap: SPACING.DEFAULT,
          flexWrap: 'wrap',
        }}>
          <div style={{
            padding: SPACING.MEDIUM,
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${isOutperforming ? COLORS.GREEN : COLORS.RED}`,
            flex: 1,
            minWidth: 180,
          }}>
            <div style={{ fontSize: TYPOGRAPHY.TINY, color: COLORS.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>
              {result.xpme !== undefined ? 'xPME RATIO' : 'PME RATIO'}
            </div>
            <div style={{
              fontSize: '24px',
              fontWeight: TYPOGRAPHY.BOLD,
              color: isOutperforming ? COLORS.GREEN : COLORS.RED,
              fontFamily: TYPOGRAPHY.MONO,
            }}>
              {pmeValue.toFixed(4)}
            </div>
            <div style={{
              fontSize: TYPOGRAPHY.TINY,
              color: isOutperforming ? COLORS.GREEN : COLORS.RED,
              marginTop: '4px',
            }}>
              {isOutperforming ? 'FUND OUTPERFORMS BENCHMARK' : 'BENCHMARK OUTPERFORMS FUND'}
            </div>
          </div>

          {result.nav_pme !== undefined && (
            <div style={{
              padding: SPACING.MEDIUM,
              backgroundColor: COLORS.PANEL_BG,
              border: `1px solid ${COLORS.BORDER}`,
              flex: 1,
              minWidth: 180,
            }}>
              <div style={{ fontSize: TYPOGRAPHY.TINY, color: COLORS.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>
                NAV PME
              </div>
              <div style={{
                fontSize: '24px',
                fontWeight: TYPOGRAPHY.BOLD,
                color: COLORS.CYAN,
                fontFamily: TYPOGRAPHY.MONO,
              }}>
                {result.nav_pme.toFixed(4)}
              </div>
              <div style={{ fontSize: TYPOGRAPHY.TINY, color: COLORS.GRAY, marginTop: '4px' }}>
                PME-adjusted Net Asset Value
              </div>
            </div>
          )}

          <div style={{
            padding: SPACING.MEDIUM,
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            flex: 1,
            minWidth: 180,
          }}>
            <div style={{ fontSize: TYPOGRAPHY.TINY, color: COLORS.GRAY, letterSpacing: '0.5px', marginBottom: '4px' }}>
              INTERPRETATION
            </div>
            <div style={{ fontSize: TYPOGRAPHY.BODY, color: COLORS.WHITE, lineHeight: '1.5' }}>
              {pmeValue > 1.2 ? 'Strong outperformance vs public markets' :
               pmeValue > 1.0 ? 'Modest outperformance vs public markets' :
               pmeValue > 0.8 ? 'Slight underperformance vs public markets' :
               'Significant underperformance vs public markets'}
            </div>
          </div>
        </div>

        {/* Details Table */}
        {result.details && result.details.length > 0 && (
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            overflow: 'auto',
          }}>
            <div style={{
              padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
              fontSize: TYPOGRAPHY.SMALL,
              color: COLORS.ORANGE,
              fontWeight: TYPOGRAPHY.BOLD,
              borderBottom: `1px solid ${COLORS.BORDER}`,
              letterSpacing: '0.5px',
            }}>
              DETAILED BREAKDOWN
            </div>
            <div style={{ overflowX: 'auto' }}>
              <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: TYPOGRAPHY.SMALL }}>
                <thead>
                  <tr style={{ borderBottom: `1px solid ${COLORS.BORDER}` }}>
                    {Object.keys(result.details[0]).map(key => (
                      <th key={key} style={{
                        padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
                        textAlign: 'right',
                        color: COLORS.GRAY,
                        fontWeight: TYPOGRAPHY.BOLD,
                        fontSize: TYPOGRAPHY.TINY,
                        letterSpacing: '0.5px',
                        textTransform: 'uppercase',
                        whiteSpace: 'nowrap',
                      }}>
                        {key}
                      </th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {result.details.map((row: any, i: number) => (
                    <tr key={i} style={{ borderBottom: `1px solid ${COLORS.BORDER}` }}>
                      {Object.values(row).map((val: any, j: number) => (
                        <td key={j} style={{
                          padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
                          textAlign: 'right',
                          color: COLORS.WHITE,
                          fontFamily: TYPOGRAPHY.MONO,
                          whiteSpace: 'nowrap',
                        }}>
                          {typeof val === 'number' ? val.toFixed(4) : String(val)}
                        </td>
                      ))}
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </div>
        )}

        {/* Chart - Price comparison */}
        {result.details && result.details.length > 0 && (
          <div style={{
            backgroundColor: COLORS.PANEL_BG,
            border: `1px solid ${COLORS.BORDER}`,
            padding: SPACING.DEFAULT,
          }}>
            <div style={{
              fontSize: TYPOGRAPHY.SMALL,
              color: COLORS.CYAN,
              fontWeight: TYPOGRAPHY.BOLD,
              marginBottom: SPACING.SMALL,
              letterSpacing: '0.5px',
            }}>
              PME CASHFLOW ANALYSIS
            </div>
            <ResponsiveContainer width="100%" height={220}>
              <BarChart data={result.details}>
                <CartesianGrid strokeDasharray="3 3" stroke={COLORS.BORDER} />
                <XAxis
                  dataKey={Object.keys(result.details[0])[0]}
                  tick={{ fontSize: 9, fill: COLORS.GRAY }}
                />
                <YAxis tick={{ fontSize: 9, fill: COLORS.GRAY }} />
                <Tooltip
                  contentStyle={{ backgroundColor: COLORS.PANEL_BG, border: `1px solid ${COLORS.BORDER}`, fontSize: 10 }}
                  labelStyle={{ color: COLORS.GRAY }}
                />
                <Legend wrapperStyle={{ fontSize: 10 }} />
                {Object.keys(result.details[0]).slice(1).map((key, idx) => (
                  <Bar
                    key={key}
                    dataKey={key}
                    fill={[COLORS.ORANGE, COLORS.CYAN, COLORS.GREEN, COLORS.PURPLE, COLORS.YELLOW][idx % 5]}
                    opacity={0.8}
                  />
                ))}
              </BarChart>
            </ResponsiveContainer>
          </div>
        )}
      </div>
    );
  };

  return (
    <div style={{ padding: SPACING.DEFAULT, display: 'flex', flexDirection: 'column', gap: SPACING.DEFAULT, height: '100%', overflow: 'auto' }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        gap: SPACING.DEFAULT,
        flexWrap: 'wrap',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
          <TrendingUp size={14} color={COLORS.ORANGE} />
          <span style={{
            color: COLORS.ORANGE,
            fontWeight: TYPOGRAPHY.BOLD,
            fontSize: TYPOGRAPHY.SUBHEADING,
            letterSpacing: '1px',
          }}>
            PUBLIC MARKET EQUIVALENT
          </span>
        </div>

        <div style={{ display: 'flex', gap: '2px' }}>
          {([
            { id: 'basic' as PMEMode, label: 'BASIC PME' },
            { id: 'extended' as PMEMode, label: 'EXTENDED xPME' },
            { id: 'tessa' as PMEMode, label: 'TESSA AUTO' },
          ]).map(tab => (
            <button
              key={tab.id}
              onClick={() => { setMode(tab.id); setResult(null); setError(null); }}
              style={{
                padding: `${SPACING.SMALL} ${SPACING.DEFAULT}`,
                backgroundColor: mode === tab.id ? COLORS.ORANGE : 'transparent',
                border: `1px solid ${mode === tab.id ? COLORS.ORANGE : COLORS.BORDER}`,
                color: mode === tab.id ? COLORS.DARK_BG : COLORS.GRAY,
                fontSize: TYPOGRAPHY.SMALL,
                fontWeight: TYPOGRAPHY.BOLD,
                cursor: 'pointer',
                transition: EFFECTS.TRANSITION_FAST,
              }}
            >
              {tab.label}
            </button>
          ))}
        </div>

        <label style={{ display: 'flex', alignItems: 'center', gap: '4px', fontSize: TYPOGRAPHY.SMALL, color: COLORS.GRAY, cursor: 'pointer' }}>
          <input
            type="checkbox"
            checked={verbose}
            onChange={e => setVerbose(e.target.checked)}
            style={{ accentColor: COLORS.ORANGE }}
          />
          VERBOSE
        </label>

        <div style={{ flex: 1 }} />

        <button
          onClick={calculate}
          disabled={loading}
          style={{
            padding: `${SPACING.SMALL} ${SPACING.MEDIUM}`,
            backgroundColor: loading ? COLORS.MUTED : COLORS.ORANGE,
            color: COLORS.DARK_BG,
            border: 'none',
            fontSize: TYPOGRAPHY.SMALL,
            fontWeight: TYPOGRAPHY.BOLD,
            cursor: loading ? 'default' : 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: '4px',
          }}
        >
          {loading ? <Loader2 size={12} className="animate-spin" /> : <Play size={12} />}
          {loading ? 'COMPUTING...' : 'CALCULATE PME'}
        </button>
      </div>

      {/* Description */}
      <div style={{
        padding: SPACING.DEFAULT,
        backgroundColor: COLORS.PANEL_BG,
        border: `1px solid ${COLORS.BORDER}`,
        fontSize: TYPOGRAPHY.SMALL,
        color: COLORS.GRAY,
        lineHeight: '1.5',
      }}>
        {mode === 'basic' && 'Basic PME compares private fund returns against a public market benchmark using index-based cashflow replication. Enter cashflows, fund prices per period, and corresponding benchmark prices.'}
        {mode === 'extended' && 'Extended PME (xPME) uses exact dates for time-weighted calculations. Provide dated cashflows with corresponding fund and benchmark prices for accurate IRR-based PME.'}
        {mode === 'tessa' && 'Tessa PME automatically fetches benchmark prices from Yahoo Finance or CoinGecko. Just provide your fund cashflows/prices and select a benchmark ticker.'}
      </div>

      {/* Inputs */}
      <div style={{
        padding: SPACING.DEFAULT,
        backgroundColor: COLORS.PANEL_BG,
        border: `1px solid ${COLORS.BORDER}`,
      }}>
        {mode === 'basic' && renderBasicInputs()}
        {mode === 'extended' && renderExtendedInputs()}
        {mode === 'tessa' && renderTessaInputs()}
      </div>

      {/* Error */}
      {error && (
        <div style={{
          padding: SPACING.DEFAULT,
          backgroundColor: 'rgba(255, 59, 59, 0.1)',
          border: `1px solid ${COLORS.RED}`,
          color: COLORS.RED,
          fontSize: TYPOGRAPHY.BODY,
        }}>
          {error}
        </div>
      )}

      {/* Results */}
      {renderResults()}

      {/* Empty state */}
      {!result && !error && !loading && (
        <div style={{
          padding: '40px',
          textAlign: 'center',
          color: COLORS.GRAY,
          fontSize: TYPOGRAPHY.BODY,
          border: `1px solid ${COLORS.BORDER}`,
          backgroundColor: COLORS.PANEL_BG,
        }}>
          <Activity size={32} style={{ marginBottom: '8px', opacity: 0.3 }} />
          <div>Configure inputs and click CALCULATE PME</div>
          <div style={{ fontSize: TYPOGRAPHY.TINY, marginTop: '4px' }}>
            PME {'>'} 1.0 = fund outperforms, PME {'<'} 1.0 = benchmark outperforms — powered by pypme
          </div>
        </div>
      )}
    </div>
  );
};

export default PyPMEView;
