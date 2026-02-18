/**
 * ReportsPanel — Merged Reports + PME + Active Management.
 * Internal sub-tabs: TAX | PME | ACTIVE MGMT
 * Replaces old ReportsView, PyPMEView, ActiveManagementView
 */
import React, { useState, useEffect, useMemo } from 'react';
import { invoke } from '@tauri-apps/api/core';
import {
  FileText, TrendingUp, Activity, Play, Loader2, Plus, Trash2,
  AlertTriangle, Target, Award, Download,
} from 'lucide-react';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, Legend } from 'recharts';
import { FINCEPT, BORDERS, TYPOGRAPHY, SPACING, COMMON_STYLES, EFFECTS } from '../finceptStyles';
import { valColor } from '../components/helpers';
import { formatCurrency, formatPercent, formatNumber, calculateTaxLiability } from '../portfolio/utils';
import { activeManagementService, type ComprehensiveAnalysisResult } from '@/services/portfolio/activeManagementService';
import type { PortfolioSummary, Transaction } from '../../../../services/portfolio/portfolioService';

interface ReportsPanelProps {
  portfolioSummary: PortfolioSummary;
  transactions: Transaction[];
  currency: string;
}

type SubTab = 'tax' | 'pme' | 'active';
type PMEMode = 'basic' | 'extended' | 'tessa';

interface CashflowEntry {
  date: string;
  amount: number;
  price: number;
  pmePrice: number;
}

// ─── Shared styles ───
const LABEL: React.CSSProperties = {
  fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY,
  letterSpacing: '0.5px', textTransform: 'uppercase', marginBottom: '2px',
};

const CARD: React.CSSProperties = {
  padding: '8px', backgroundColor: FINCEPT.PANEL_BG,
  border: BORDERS.STANDARD, borderRadius: '2px',
};

const INPUT: React.CSSProperties = {
  backgroundColor: FINCEPT.DARK_BG, color: FINCEPT.WHITE,
  border: `1px solid ${FINCEPT.BORDER}`, padding: '4px 8px',
  fontSize: '10px', fontFamily: TYPOGRAPHY.MONO, width: '100%',
};

// ═════════════════════════════════════
// TAX SUB-TAB
// ═════════════════════════════════════
const TaxSubTab: React.FC<{ portfolioSummary: PortfolioSummary; transactions: Transaction[]; currency: string }> = ({
  portfolioSummary, transactions, currency,
}) => {
  const taxCalc = calculateTaxLiability(transactions, 0.15, 0.20, 365);
  const dividendTxns = transactions.filter(t => t.transaction_type === 'DIVIDEND');
  const totalDividends = dividendTxns.reduce((sum, t) => sum + (t.price * t.quantity), 0);
  const dividendYield = portfolioSummary.total_market_value > 0
    ? (totalDividends / portfolioSummary.total_market_value) * 100 : 0;

  const buys = transactions.filter(t => t.transaction_type === 'BUY').length;
  const sells = transactions.filter(t => t.transaction_type === 'SELL').length;

  return (
    <div style={{ padding: '8px', overflow: 'auto', height: '100%' }}>
      {/* Summary Cards */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(4, 1fr)', gap: '6px', marginBottom: '10px' }}>
        <div style={{ ...CARD, borderLeft: `3px solid ${FINCEPT.ORANGE}` }}>
          <div style={LABEL}>TOTAL TAX LIABILITY</div>
          <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.ORANGE }}>{formatCurrency(taxCalc.totalTax, currency)}</div>
        </div>
        <div style={{ ...CARD, borderLeft: `3px solid ${FINCEPT.GREEN}` }}>
          <div style={LABEL}>LONG-TERM GAINS</div>
          <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.GREEN }}>{formatCurrency(taxCalc.longTermGains, currency)}</div>
          <div style={{ fontSize: '8px', color: FINCEPT.MUTED, marginTop: '2px' }}>Tax: {formatCurrency(taxCalc.longTermTax, currency)}</div>
        </div>
        <div style={{ ...CARD, borderLeft: `3px solid ${FINCEPT.YELLOW}` }}>
          <div style={LABEL}>SHORT-TERM GAINS</div>
          <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.YELLOW }}>{formatCurrency(taxCalc.shortTermGains, currency)}</div>
          <div style={{ fontSize: '8px', color: FINCEPT.MUTED, marginTop: '2px' }}>Tax: {formatCurrency(taxCalc.shortTermTax, currency)}</div>
        </div>
        <div style={{ ...CARD, borderLeft: `3px solid ${FINCEPT.CYAN}` }}>
          <div style={LABEL}>UNREALIZED GAINS</div>
          <div style={{ fontSize: '14px', fontWeight: 700, color: valColor(portfolioSummary.total_unrealized_pnl) }}>
            {formatCurrency(portfolioSummary.total_unrealized_pnl, currency)}
          </div>
          <div style={{ fontSize: '8px', color: FINCEPT.MUTED, marginTop: '2px' }}>Not taxable until sold</div>
        </div>
      </div>

      {/* Tax Breakdown Table */}
      <div style={{ marginBottom: '10px' }}>
        <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: '4px', padding: '4px 8px' }}>TAX BREAKDOWN</div>
        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
              {['CATEGORY', 'GAINS', 'RATE', 'TAX DUE'].map((h, i) => (
                <th key={h} style={{ padding: '4px 8px', fontSize: '8px', fontWeight: 700, color: FINCEPT.ORANGE, textAlign: i === 0 ? 'left' : 'right', borderBottom: `1px solid ${FINCEPT.ORANGE}`, letterSpacing: '0.3px' }}>
                  {h}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}`, backgroundColor: 'rgba(255,215,0,0.03)' }}>
              <td style={{ padding: '4px 8px', fontSize: '9px', color: FINCEPT.YELLOW, fontWeight: 700 }}>Short-Term Capital Gains (&lt; 1 year)</td>
              <td style={{ padding: '4px 8px', fontSize: '9px', color: FINCEPT.CYAN, textAlign: 'right' }}>{formatCurrency(taxCalc.shortTermGains, currency)}</td>
              <td style={{ padding: '4px 8px', fontSize: '9px', color: FINCEPT.GRAY, textAlign: 'right' }}>15%</td>
              <td style={{ padding: '4px 8px', fontSize: '9px', color: FINCEPT.ORANGE, textAlign: 'right', fontWeight: 700 }}>{formatCurrency(taxCalc.shortTermTax, currency)}</td>
            </tr>
            <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}`, backgroundColor: 'rgba(0,214,111,0.03)' }}>
              <td style={{ padding: '4px 8px', fontSize: '9px', color: FINCEPT.GREEN, fontWeight: 700 }}>Long-Term Capital Gains (&gt; 1 year)</td>
              <td style={{ padding: '4px 8px', fontSize: '9px', color: FINCEPT.CYAN, textAlign: 'right' }}>{formatCurrency(taxCalc.longTermGains, currency)}</td>
              <td style={{ padding: '4px 8px', fontSize: '9px', color: FINCEPT.GRAY, textAlign: 'right' }}>20%</td>
              <td style={{ padding: '4px 8px', fontSize: '9px', color: FINCEPT.ORANGE, textAlign: 'right', fontWeight: 700 }}>{formatCurrency(taxCalc.longTermTax, currency)}</td>
            </tr>
            <tr style={{ backgroundColor: `${FINCEPT.ORANGE}08`, borderTop: `2px solid ${FINCEPT.ORANGE}` }}>
              <td style={{ padding: '4px 8px', fontSize: '9px', color: FINCEPT.ORANGE, fontWeight: 700 }}>TOTAL TAX DUE</td>
              <td colSpan={2} />
              <td style={{ padding: '4px 8px', fontSize: '11px', color: FINCEPT.ORANGE, textAlign: 'right', fontWeight: 700 }}>{formatCurrency(taxCalc.totalTax, currency)}</td>
            </tr>
          </tbody>
        </table>
      </div>

      {/* Bottom Row: Transaction Summary + Dividend Info */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
        <div style={CARD}>
          <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: '6px', padding: '0' }}>TRANSACTION SUMMARY</div>
          {[
            { label: 'TOTAL TRANSACTIONS', value: String(transactions.length), color: FINCEPT.CYAN },
            { label: 'BUY TRANSACTIONS', value: String(buys), color: FINCEPT.GREEN },
            { label: 'SELL TRANSACTIONS', value: String(sells), color: FINCEPT.RED },
            { label: 'CURRENT POSITIONS', value: String(portfolioSummary.holdings.length), color: FINCEPT.CYAN },
          ].map(row => (
            <div key={row.label} style={{ display: 'flex', justifyContent: 'space-between', padding: '3px 0', borderBottom: `1px solid ${FINCEPT.BORDER}20` }}>
              <span style={{ ...LABEL, marginBottom: 0 }}>{row.label}</span>
              <span style={{ fontSize: '9px', fontWeight: 700, color: row.color }}>{row.value}</span>
            </div>
          ))}
        </div>
        <div style={CARD}>
          <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: '6px', padding: '0' }}>DIVIDEND INCOME</div>
          <div style={{ display: 'flex', justifyContent: 'space-between', padding: '3px 0', borderBottom: `1px solid ${FINCEPT.BORDER}20` }}>
            <span style={{ ...LABEL, marginBottom: 0 }}>TOTAL DIVIDENDS</span>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.GREEN }}>{formatCurrency(totalDividends, currency)}</span>
          </div>
          <div style={{ display: 'flex', justifyContent: 'space-between', padding: '3px 0' }}>
            <span style={{ ...LABEL, marginBottom: 0 }}>PORTFOLIO YIELD</span>
            <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.CYAN }}>{dividendYield.toFixed(2)}%</span>
          </div>
          <div style={{ marginTop: '8px', fontSize: '8px', color: FINCEPT.MUTED }}>
            Consult with a tax professional for accurate filing.
          </div>
        </div>
      </div>
    </div>
  );
};

// ═════════════════════════════════════
// PME SUB-TAB
// ═════════════════════════════════════
const PMESubTab: React.FC<{ portfolioSummary: PortfolioSummary }> = ({ portfolioSummary }) => {
  const [mode, setMode] = useState<PMEMode>('basic');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [result, setResult] = useState<any>(null);

  const [cashflows, setCashflows] = useState<number[]>([-1000, -500, 200]);
  const [prices, setPrices] = useState<number[]>([100, 110, 120, 130]);
  const [pmePrices, setPmePrices] = useState<number[]>([100, 105, 112, 118]);

  const [entries, setEntries] = useState<CashflowEntry[]>([
    { date: '2020-01-01', amount: -1000, price: 100, pmePrice: 100 },
    { date: '2021-01-01', amount: -500, price: 110, pmePrice: 105 },
    { date: '2022-01-01', amount: 200, price: 130, pmePrice: 118 },
  ]);

  const [pmeTicker, setPmeTicker] = useState('SPY');
  const [pmeSource, setPmeSource] = useState('yahoo');
  const [verbose, setVerbose] = useState(true);

  const addEntry = () => setEntries([...entries, { date: '', amount: 0, price: 0, pmePrice: 0 }]);
  const removeEntry = (idx: number) => setEntries(entries.filter((_, i) => i !== idx));
  const updateEntry = (idx: number, field: keyof CashflowEntry, value: string | number) => {
    const updated = [...entries];
    (updated[idx] as any)[field] = value;
    setEntries(updated);
  };

  const calculate = async () => {
    setLoading(true); setError(null); setResult(null);
    try {
      let raw: string;
      if (mode === 'basic') {
        raw = verbose
          ? await invoke<string>('pypme_verbose', { cashflows, prices, pmePrices })
          : await invoke<string>('pypme_calculate', { cashflows, prices, pmePrices });
      } else if (mode === 'extended') {
        const dates = entries.map(e => e.date);
        const cf = entries.map(e => e.amount);
        const pr = entries.map(e => e.price);
        const pp = entries.map(e => e.pmePrice);
        raw = verbose
          ? await invoke<string>('pypme_verbose_xpme', { dates, cashflows: cf, prices: pr, pmePrices: pp })
          : await invoke<string>('pypme_xpme', { dates, cashflows: cf, prices: pr, pmePrices: pp });
      } else {
        const dates = entries.map(e => e.date);
        const cf = entries.map(e => e.amount);
        const pr = entries.map(e => e.price);
        raw = verbose
          ? await invoke<string>('pypme_tessa_verbose_xpme', { dates, cashflows: cf, prices: pr, pmeTicker, pmeSource })
          : await invoke<string>('pypme_tessa_xpme', { dates, cashflows: cf, prices: pr, pmeTicker, pmeSource });
      }
      const parsed = JSON.parse(raw);
      if (parsed.error) setError(parsed.error); else setResult(parsed);
    } catch (e: any) {
      setError(e?.toString() || 'PME calculation failed');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div style={{ padding: '8px', display: 'flex', flexDirection: 'column', gap: '8px', height: '100%', overflow: 'auto' }}>
      {/* Header bar */}
      <div style={{ display: 'flex', alignItems: 'center', gap: '6px', flexWrap: 'wrap' }}>
        <TrendingUp size={12} color={FINCEPT.ORANGE} />
        <span style={{ color: FINCEPT.ORANGE, fontWeight: 700, fontSize: '10px', letterSpacing: '0.5px' }}>PUBLIC MARKET EQUIVALENT</span>

        <div style={{ display: 'flex', gap: '1px', marginLeft: '8px' }}>
          {(['basic', 'extended', 'tessa'] as PMEMode[]).map(m => (
            <button key={m} onClick={() => { setMode(m); setResult(null); setError(null); }}
              style={{
                padding: '3px 8px', fontSize: '8px', fontWeight: 700, cursor: 'pointer',
                backgroundColor: mode === m ? FINCEPT.ORANGE : 'transparent',
                border: `1px solid ${mode === m ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                color: mode === m ? '#000' : FINCEPT.GRAY,
              }}>
              {m === 'basic' ? 'BASIC' : m === 'extended' ? 'xPME' : 'TESSA'}
            </button>
          ))}
        </div>

        <label style={{ display: 'flex', alignItems: 'center', gap: '3px', fontSize: '8px', color: FINCEPT.GRAY, cursor: 'pointer', marginLeft: '4px' }}>
          <input type="checkbox" checked={verbose} onChange={e => setVerbose(e.target.checked)} style={{ accentColor: FINCEPT.ORANGE }} />
          VERBOSE
        </label>

        <div style={{ flex: 1 }} />

        <button onClick={calculate} disabled={loading}
          style={{
            padding: '3px 10px', backgroundColor: loading ? FINCEPT.MUTED : FINCEPT.ORANGE,
            color: '#000', border: 'none', fontSize: '9px', fontWeight: 700, cursor: loading ? 'default' : 'pointer',
            display: 'flex', alignItems: 'center', gap: '3px',
          }}>
          {loading ? <Loader2 size={10} className="animate-spin" /> : <Play size={10} />}
          {loading ? 'COMPUTING...' : 'CALCULATE'}
        </button>
      </div>

      {/* Description */}
      <div style={{ padding: '6px 8px', backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, fontSize: '9px', color: FINCEPT.GRAY, lineHeight: '1.4' }}>
        {mode === 'basic' && 'Basic PME compares private fund returns against a public market benchmark using index-based cashflow replication.'}
        {mode === 'extended' && 'Extended PME (xPME) uses exact dates for time-weighted calculations with IRR-based PME.'}
        {mode === 'tessa' && 'Tessa PME auto-fetches benchmark prices from Yahoo Finance or CoinGecko.'}
      </div>

      {/* Inputs */}
      <div style={{ padding: '8px', backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD }}>
        {mode === 'basic' ? (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
            <div>
              <div style={LABEL}>CASHFLOWS (comma-separated, negative = investment)</div>
              <input type="text" value={cashflows.join(', ')} onChange={e => setCashflows(e.target.value.split(',').map(v => parseFloat(v.trim()) || 0))} style={INPUT} placeholder="-1000, -500, 200" />
            </div>
            <div>
              <div style={LABEL}>FUND PRICES (one per period, include final NAV)</div>
              <input type="text" value={prices.join(', ')} onChange={e => setPrices(e.target.value.split(',').map(v => parseFloat(v.trim()) || 0))} style={INPUT} placeholder="100, 110, 120, 130" />
            </div>
            <div>
              <div style={LABEL}>BENCHMARK PRICES (public market index, same periods)</div>
              <input type="text" value={pmePrices.join(', ')} onChange={e => setPmePrices(e.target.value.split(',').map(v => parseFloat(v.trim()) || 0))} style={INPUT} placeholder="100, 105, 112, 118" />
            </div>
          </div>
        ) : (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            <div style={{ display: 'flex', gap: '4px', fontSize: '8px', color: FINCEPT.GRAY, textTransform: 'uppercase', letterSpacing: '0.3px' }}>
              <div style={{ flex: 2 }}>DATE</div>
              <div style={{ flex: 2 }}>CASHFLOW</div>
              <div style={{ flex: 2 }}>FUND PRICE</div>
              {mode === 'extended' && <div style={{ flex: 2 }}>BENCHMARK</div>}
              <div style={{ width: 24 }} />
            </div>
            {entries.map((entry, i) => (
              <div key={i} style={{ display: 'flex', gap: '4px', alignItems: 'center' }}>
                <input type="date" value={entry.date} onChange={e => updateEntry(i, 'date', e.target.value)} style={{ ...INPUT, flex: 2 }} />
                <input type="number" value={entry.amount} onChange={e => updateEntry(i, 'amount', parseFloat(e.target.value) || 0)} style={{ ...INPUT, flex: 2 }} placeholder="Cashflow" />
                <input type="number" value={entry.price} onChange={e => updateEntry(i, 'price', parseFloat(e.target.value) || 0)} style={{ ...INPUT, flex: 2 }} placeholder="Fund Price" />
                {mode === 'extended' && (
                  <input type="number" value={entry.pmePrice} onChange={e => updateEntry(i, 'pmePrice', parseFloat(e.target.value) || 0)} style={{ ...INPUT, flex: 2 }} placeholder="Benchmark" />
                )}
                <button onClick={() => removeEntry(i)} style={{ width: 24, height: 24, backgroundColor: 'transparent', border: `1px solid ${FINCEPT.BORDER}`, color: FINCEPT.RED, cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center' }}>
                  <Trash2 size={10} />
                </button>
              </div>
            ))}
            <button onClick={addEntry} style={{ padding: '4px 8px', backgroundColor: 'transparent', border: `1px dashed ${FINCEPT.BORDER}`, color: FINCEPT.GRAY, fontSize: '9px', cursor: 'pointer', display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '3px' }}>
              <Plus size={10} /> ADD ROW
            </button>
            {mode === 'tessa' && (
              <div style={{ display: 'flex', gap: '8px', marginTop: '4px' }}>
                <div style={{ flex: 1 }}>
                  <div style={LABEL}>BENCHMARK TICKER</div>
                  <input type="text" value={pmeTicker} onChange={e => setPmeTicker(e.target.value.toUpperCase())} style={INPUT} placeholder="SPY" />
                </div>
                <div style={{ flex: 1 }}>
                  <div style={LABEL}>DATA SOURCE</div>
                  <select value={pmeSource} onChange={e => setPmeSource(e.target.value)} style={INPUT}>
                    <option value="yahoo">Yahoo Finance</option>
                    <option value="coingecko">CoinGecko</option>
                  </select>
                </div>
              </div>
            )}
          </div>
        )}
      </div>

      {/* Error */}
      {error && (
        <div style={{ padding: '6px 8px', backgroundColor: 'rgba(255,59,59,0.08)', border: `1px solid ${FINCEPT.RED}`, color: FINCEPT.RED, fontSize: '9px' }}>
          {error}
        </div>
      )}

      {/* Results */}
      {result && (() => {
        const pmeValue = result.pme ?? result.xpme;
        const isOut = pmeValue > 1.0;
        return (
          <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
            {/* Score cards */}
            <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
              <div style={{ ...CARD, borderLeft: `3px solid ${isOut ? FINCEPT.GREEN : FINCEPT.RED}`, flex: 1, minWidth: 160 }}>
                <div style={LABEL}>{result.xpme !== undefined ? 'xPME RATIO' : 'PME RATIO'}</div>
                <div style={{ fontSize: '20px', fontWeight: 700, color: isOut ? FINCEPT.GREEN : FINCEPT.RED, fontFamily: TYPOGRAPHY.MONO }}>{pmeValue.toFixed(4)}</div>
                <div style={{ fontSize: '8px', color: isOut ? FINCEPT.GREEN : FINCEPT.RED, marginTop: '2px' }}>
                  {isOut ? 'FUND OUTPERFORMS BENCHMARK' : 'BENCHMARK OUTPERFORMS FUND'}
                </div>
              </div>
              {result.nav_pme !== undefined && (
                <div style={{ ...CARD, borderLeft: `3px solid ${FINCEPT.CYAN}`, flex: 1, minWidth: 160 }}>
                  <div style={LABEL}>NAV PME</div>
                  <div style={{ fontSize: '20px', fontWeight: 700, color: FINCEPT.CYAN, fontFamily: TYPOGRAPHY.MONO }}>{result.nav_pme.toFixed(4)}</div>
                  <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginTop: '2px' }}>PME-adjusted Net Asset Value</div>
                </div>
              )}
              <div style={{ ...CARD, flex: 1, minWidth: 160 }}>
                <div style={LABEL}>INTERPRETATION</div>
                <div style={{ fontSize: '9px', color: FINCEPT.WHITE, lineHeight: '1.4' }}>
                  {pmeValue > 1.2 ? 'Strong outperformance vs public markets' :
                   pmeValue > 1.0 ? 'Modest outperformance vs public markets' :
                   pmeValue > 0.8 ? 'Slight underperformance vs public markets' :
                   'Significant underperformance vs public markets'}
                </div>
              </div>
            </div>

            {/* Details table */}
            {result.details?.length > 0 && (
              <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, overflow: 'hidden' }}>
                <div style={{ padding: '4px 8px', fontSize: '9px', color: FINCEPT.ORANGE, fontWeight: 700, borderBottom: `1px solid ${FINCEPT.BORDER}`, letterSpacing: '0.3px' }}>DETAILED BREAKDOWN</div>
                <div style={{ overflowX: 'auto' }}>
                  <table style={{ width: '100%', borderCollapse: 'collapse', fontSize: '9px' }}>
                    <thead>
                      <tr style={{ borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
                        {Object.keys(result.details[0]).map(key => (
                          <th key={key} style={{ padding: '3px 8px', textAlign: 'right', color: FINCEPT.GRAY, fontWeight: 700, fontSize: '8px', letterSpacing: '0.3px', textTransform: 'uppercase', whiteSpace: 'nowrap' }}>{key}</th>
                        ))}
                      </tr>
                    </thead>
                    <tbody>
                      {result.details.map((row: any, i: number) => (
                        <tr key={i} style={{ borderBottom: `1px solid ${FINCEPT.BORDER}20` }}>
                          {Object.values(row).map((val: any, j: number) => (
                            <td key={j} style={{ padding: '3px 8px', textAlign: 'right', color: FINCEPT.WHITE, fontFamily: TYPOGRAPHY.MONO, whiteSpace: 'nowrap' }}>
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

            {/* Chart */}
            {result.details?.length > 0 && (
              <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, padding: '8px' }}>
                <div style={{ fontSize: '9px', color: FINCEPT.CYAN, fontWeight: 700, marginBottom: '4px', letterSpacing: '0.3px' }}>PME CASHFLOW ANALYSIS</div>
                <ResponsiveContainer width="100%" height={180}>
                  <BarChart data={result.details}>
                    <CartesianGrid strokeDasharray="3 3" stroke={FINCEPT.BORDER} />
                    <XAxis dataKey={Object.keys(result.details[0])[0]} tick={{ fontSize: 8, fill: FINCEPT.GRAY }} />
                    <YAxis tick={{ fontSize: 8, fill: FINCEPT.GRAY }} />
                    <Tooltip contentStyle={{ backgroundColor: FINCEPT.PANEL_BG, border: `1px solid ${FINCEPT.BORDER}`, fontSize: 9 }} labelStyle={{ color: FINCEPT.GRAY }} />
                    <Legend wrapperStyle={{ fontSize: 9 }} />
                    {Object.keys(result.details[0]).slice(1).map((key, idx) => (
                      <Bar key={key} dataKey={key} fill={[FINCEPT.ORANGE, FINCEPT.CYAN, FINCEPT.GREEN, FINCEPT.PURPLE, FINCEPT.YELLOW][idx % 5]} opacity={0.8} />
                    ))}
                  </BarChart>
                </ResponsiveContainer>
              </div>
            )}
          </div>
        );
      })()}

      {/* Empty state */}
      {!result && !error && !loading && (
        <div style={{ padding: '24px', textAlign: 'center', color: FINCEPT.GRAY, fontSize: '10px', border: BORDERS.STANDARD, backgroundColor: FINCEPT.PANEL_BG }}>
          <Activity size={24} style={{ marginBottom: '6px', opacity: 0.3 }} />
          <div>Configure inputs and click CALCULATE</div>
          <div style={{ fontSize: '8px', marginTop: '3px', color: FINCEPT.MUTED }}>
            PME {'>'} 1.0 = fund outperforms, PME {'<'} 1.0 = benchmark outperforms — powered by pypme
          </div>
        </div>
      )}
    </div>
  );
};

// ═════════════════════════════════════
// ACTIVE MANAGEMENT SUB-TAB
// ═════════════════════════════════════
const ActiveMgmtSubTab: React.FC<{ portfolioSummary: PortfolioSummary }> = ({ portfolioSummary }) => {
  const [loading, setLoading] = useState(false);
  const [analysis, setAnalysis] = useState<ComprehensiveAnalysisResult | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [benchmarkSymbol] = useState('^GSPC');

  const portfolioReturns = useMemo(() =>
    portfolioSummary.holdings.length > 0
      ? portfolioSummary.holdings.map(h => (h.day_change_percent || 0) / 100)
      : [],
  [portfolioSummary.holdings]);

  const portfolioWeights = useMemo(() =>
    portfolioSummary.holdings.map(h => h.weight / 100),
  [portfolioSummary.holdings]);

  useEffect(() => {
    if (portfolioReturns.length > 0) runAnalysis();
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  const runAnalysis = async () => {
    if (portfolioReturns.length === 0) {
      setError('No portfolio return data available');
      return;
    }
    setLoading(true); setError(null);
    try {
      const benchReturns = await activeManagementService.fetchBenchmarkReturns(
        benchmarkSymbol === '^GSPC' ? 'SPY' : benchmarkSymbol,
        portfolioReturns.length,
      );
      if (benchReturns.length === 0) {
        setError('Unable to fetch benchmark returns. Check your internet connection.');
        return;
      }
      const result = await activeManagementService.comprehensiveAnalysis(
        { returns: portfolioReturns, weights: portfolioWeights },
        { returns: benchReturns },
      );
      setAnalysis(result);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Analysis failed');
    } finally {
      setLoading(false);
    }
  };

  if (loading) {
    return (
      <div style={{ height: '100%', display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center' }}>
        <Loader2 size={18} color={FINCEPT.ORANGE} className="animate-spin" style={{ marginBottom: '8px' }} />
        <div style={{ color: FINCEPT.ORANGE, fontSize: '10px', fontWeight: 700, letterSpacing: '0.5px' }}>RUNNING ACTIVE MANAGEMENT ANALYSIS...</div>
      </div>
    );
  }

  if (error) {
    return (
      <div style={{ height: '100%', padding: '16px' }}>
        <div style={{ padding: '8px', backgroundColor: 'rgba(255,59,59,0.08)', border: `1px solid ${FINCEPT.RED}`, color: FINCEPT.RED, fontSize: '10px', borderRadius: '2px' }}>
          <AlertTriangle size={14} style={{ display: 'inline-block', marginRight: '4px', verticalAlign: 'middle' }} />
          {error}
        </div>
      </div>
    );
  }

  if (!analysis) {
    return (
      <div style={{ height: '100%', display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center' }}>
        <Activity size={28} color={FINCEPT.GRAY} style={{ marginBottom: '6px', opacity: 0.3 }} />
        <div style={{ color: FINCEPT.GRAY, fontSize: '10px', fontWeight: 700 }}>No analysis data yet</div>
        <div style={{ color: FINCEPT.MUTED, fontSize: '9px', marginTop: '3px' }}>Portfolio needs return data for active management analysis</div>
      </div>
    );
  }

  const { value_added_analysis, information_ratio_analysis, active_management_assessment, improvement_recommendations } = analysis;

  return (
    <div style={{ height: '100%', overflow: 'auto', padding: '8px' }}>
      {/* Header metrics */}
      <div style={{ display: 'flex', gap: '12px', marginBottom: '8px', fontSize: '9px' }}>
        <div>
          <span style={LABEL}>BENCHMARK: </span>
          <span style={{ color: FINCEPT.CYAN, fontSize: '10px' }}>{benchmarkSymbol}</span>
        </div>
        <div>
          <span style={LABEL}>QUALITY: </span>
          <span style={{
            color: active_management_assessment.quality_rating === 'Excellent' ? FINCEPT.GREEN :
              active_management_assessment.quality_rating === 'Good' ? FINCEPT.CYAN :
              active_management_assessment.quality_rating === 'Average' ? FINCEPT.YELLOW : FINCEPT.RED,
            fontWeight: 700, fontSize: '10px',
          }}>{active_management_assessment.quality_rating}</span>
        </div>
        <div>
          <span style={LABEL}>SCORE: </span>
          <span style={{ color: FINCEPT.CYAN, fontSize: '10px' }}>{active_management_assessment.quality_score}/100</span>
        </div>
      </div>

      {/* Key Metrics */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '6px', marginBottom: '8px' }}>
        {[
          { title: 'ACTIVE RETURN', value: formatPercent(value_added_analysis.active_return_annualized), color: valColor(value_added_analysis.active_return_annualized), icon: <TrendingUp size={12} />, sub: 'Annualized' },
          { title: 'INFORMATION RATIO', value: formatNumber(information_ratio_analysis.information_ratio_annualized, 2), color: information_ratio_analysis.information_ratio_annualized > 0.5 ? FINCEPT.GREEN : FINCEPT.YELLOW, icon: <Target size={12} />, sub: 'Risk-Adjusted' },
          { title: 'TRACKING ERROR', value: formatPercent(information_ratio_analysis.tracking_error_annualized), color: FINCEPT.CYAN, icon: <Activity size={12} />, sub: 'Annualized' },
          { title: 'HIT RATE', value: formatPercent(value_added_analysis.hit_rate), color: value_added_analysis.hit_rate > 0.55 ? FINCEPT.GREEN : FINCEPT.YELLOW, icon: <Award size={12} />, sub: 'Positive Periods' },
          { title: 'SIGNIFICANCE', value: value_added_analysis.statistical_significance.is_significant ? 'YES' : 'NO', color: value_added_analysis.statistical_significance.is_significant ? FINCEPT.GREEN : FINCEPT.RED, icon: <TrendingUp size={12} />, sub: `p=${value_added_analysis.statistical_significance.p_value.toFixed(3)}` },
          { title: 'T-STATISTIC', value: formatNumber(value_added_analysis.statistical_significance.t_statistic, 2), color: Math.abs(value_added_analysis.statistical_significance.t_statistic) > 2 ? FINCEPT.GREEN : FINCEPT.YELLOW, icon: <Activity size={12} />, sub: 'Statistical Measure' },
        ].map(m => (
          <div key={m.title} style={CARD}>
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: '2px' }}>
              <div style={LABEL}>{m.title}</div>
              <div style={{ color: m.color }}>{m.icon}</div>
            </div>
            <div style={{ fontSize: '14px', fontWeight: 700, color: FINCEPT.CYAN, marginBottom: '1px' }}>{m.value}</div>
            <div style={{ ...LABEL, color: FINCEPT.MUTED }}>{m.sub}</div>
          </div>
        ))}
      </div>

      {/* Value Decomposition */}
      <div style={{ ...CARD, marginBottom: '8px' }}>
        <div style={{ ...COMMON_STYLES.sectionHeader, marginBottom: '6px', padding: '0' }}>VALUE ADDED DECOMPOSITION</div>
        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', fontSize: '10px' }}>
          <div>
            <div style={LABEL}>ALLOCATION EFFECT</div>
            <div style={{ fontSize: '13px', fontWeight: 700, color: FINCEPT.CYAN }}>{formatPercent(value_added_analysis.value_added_decomposition.estimated_allocation_effect)}</div>
          </div>
          <div>
            <div style={LABEL}>SELECTION EFFECT</div>
            <div style={{ fontSize: '13px', fontWeight: 700, color: FINCEPT.CYAN }}>{formatPercent(value_added_analysis.value_added_decomposition.estimated_selection_effect)}</div>
          </div>
        </div>
        {value_added_analysis.value_added_decomposition.note && (
          <div style={{ fontSize: '8px', color: FINCEPT.MUTED, marginTop: '4px', fontStyle: 'italic' }}>
            {value_added_analysis.value_added_decomposition.note}
          </div>
        )}
      </div>

      {/* Strengths + Improvements */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '6px' }}>
        <div style={{ ...CARD, borderLeft: `3px solid ${FINCEPT.GREEN}` }}>
          <div style={{ ...LABEL, color: FINCEPT.GREEN, marginBottom: '4px' }}>KEY STRENGTHS</div>
          <ul style={{ margin: 0, paddingLeft: '12px', fontSize: '9px', color: FINCEPT.WHITE }}>
            {active_management_assessment.key_strengths.map((s, i) => <li key={i} style={{ marginBottom: '1px' }}>{s}</li>)}
          </ul>
        </div>
        <div style={{ ...CARD, borderLeft: `3px solid ${FINCEPT.YELLOW}` }}>
          <div style={{ ...LABEL, color: FINCEPT.YELLOW, marginBottom: '4px' }}>AREAS FOR IMPROVEMENT</div>
          <ul style={{ margin: 0, paddingLeft: '12px', fontSize: '9px', color: FINCEPT.WHITE }}>
            {active_management_assessment.areas_for_improvement.map((a, i) => <li key={i} style={{ marginBottom: '1px' }}>{a}</li>)}
          </ul>
        </div>
      </div>

      {/* Recommendations */}
      {improvement_recommendations?.length > 0 && (
        <div style={{ ...CARD, borderLeft: `3px solid ${FINCEPT.CYAN}`, marginTop: '6px' }}>
          <div style={{ ...LABEL, color: FINCEPT.CYAN, marginBottom: '4px' }}>IMPROVEMENT RECOMMENDATIONS</div>
          <ul style={{ margin: 0, paddingLeft: '12px', fontSize: '9px', color: FINCEPT.WHITE }}>
            {improvement_recommendations.map((r, i) => <li key={i} style={{ marginBottom: '1px' }}>{r}</li>)}
          </ul>
        </div>
      )}
    </div>
  );
};

// ═════════════════════════════════════
// MAIN REPORTS PANEL
// ═════════════════════════════════════
const ReportsPanel: React.FC<ReportsPanelProps> = ({ portfolioSummary, transactions, currency }) => {
  const [subTab, setSubTab] = useState<SubTab>('tax');

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden', backgroundColor: FINCEPT.DARK_BG }}>
      {/* Sub-tab strip */}
      <div style={{ display: 'flex', backgroundColor: '#0A0A0A', borderBottom: `1px solid ${FINCEPT.BORDER}`, flexShrink: 0 }}>
        {([
          { id: 'tax' as SubTab, label: 'TAX & REPORTS', icon: FileText },
          { id: 'pme' as SubTab, label: 'PME ANALYSIS', icon: TrendingUp },
          { id: 'active' as SubTab, label: 'ACTIVE MGMT', icon: Activity },
        ]).map(tab => {
          const Icon = tab.icon;
          const isActive = subTab === tab.id;
          return (
            <button key={tab.id} onClick={() => setSubTab(tab.id)}
              style={{
                padding: '5px 12px', fontSize: '8px', fontWeight: 700, letterSpacing: '0.3px',
                backgroundColor: isActive ? `${FINCEPT.ORANGE}15` : 'transparent',
                borderBottom: isActive ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
                color: isActive ? FINCEPT.ORANGE : FINCEPT.GRAY,
                border: 'none', cursor: 'pointer', textTransform: 'uppercase',
                display: 'flex', alignItems: 'center', gap: '3px',
              }}>
              <Icon size={9} />
              {tab.label}
            </button>
          );
        })}
      </div>

      {/* Content */}
      <div style={{ flex: 1, overflow: 'hidden' }}>
        {subTab === 'tax' && <TaxSubTab portfolioSummary={portfolioSummary} transactions={transactions} currency={currency} />}
        {subTab === 'pme' && <PMESubTab portfolioSummary={portfolioSummary} />}
        {subTab === 'active' && <ActiveMgmtSubTab portfolioSummary={portfolioSummary} />}
      </div>
    </div>
  );
};

export default ReportsPanel;
