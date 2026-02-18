/**
 * PerformanceRiskPanel — Bloomberg-style unified performance & risk analytics.
 * Single dense layout: NAV chart (top-left) + Risk metrics (bottom-left) + Daily returns table (right).
 * No sub-tabs. No deep risk (covered by FFNView). Pure client-side math on yfinance data.
 */
import React, { useState, useEffect, useMemo, useRef } from 'react';
import { AlertCircle, Loader } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, COMMON_STYLES } from '../finceptStyles';
import { valColor } from '../components/helpers';
import { formatCurrency } from '../portfolio/utils';
import { yfinanceService } from '../../../../services/markets/yfinanceService';
import type { HistoricalDataPoint } from '../../../../services/markets/yfinanceService';
import type { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import { cacheService } from '../../../../services/cache/cacheService';

interface PerformanceRiskPanelProps {
  portfolioSummary: PortfolioSummary;
  currency: string;
}

type Period = '1M' | '3M' | '6M' | '1Y' | 'ALL';
const PERIODS: Period[] = ['1M', '3M', '6M', '1Y', 'ALL'];
const RISK_FREE_RATE = 0.04;
const BENCHMARK = 'SPY';

function periodDays(p: Period): number {
  switch (p) {
    case '1M': return 35;
    case '3M': return 95;
    case '6M': return 185;
    case '1Y': return 370;
    case 'ALL': return 1825;
  }
}

function fmtDate(d: Date): string { return d.toISOString().split('T')[0]; }

function buildValues(
  holdings: { symbol: string; quantity: number }[],
  histMap: Map<string, HistoricalDataPoint[]>,
): { timestamps: number[]; values: number[] } {
  const allTs = new Set<number>();
  for (const [, data] of histMap) for (const pt of data) allTs.add(pt.timestamp);
  const sorted = [...allTs].sort((a, b) => a - b);
  if (sorted.length === 0) return { timestamps: [], values: [] };

  const lookup = new Map<string, Map<number, number>>();
  for (const [sym, data] of histMap) {
    const m = new Map<number, number>();
    for (const pt of data) m.set(pt.timestamp, pt.close);
    lookup.set(sym, m);
  }

  const timestamps: number[] = [];
  const values: number[] = [];
  const lastClose = new Map<string, number>();

  for (const ts of sorted) {
    let val = 0;
    let hasAny = false;
    for (const h of holdings) {
      const symMap = lookup.get(h.symbol);
      let close = symMap?.get(ts);
      if (close !== undefined) { lastClose.set(h.symbol, close); hasAny = true; }
      else close = lastClose.get(h.symbol);
      if (close !== undefined) val += h.quantity * close;
    }
    if (hasAny || lastClose.size > 0) { timestamps.push(ts); values.push(val); }
  }
  return { timestamps, values };
}

function dailyReturns(v: number[]): number[] {
  const r: number[] = [];
  for (let i = 1; i < v.length; i++) if (v[i - 1] > 0) r.push((v[i] - v[i - 1]) / v[i - 1]);
  return r;
}

function calcSharpe(r: number[]): number | null {
  if (r.length < 5) return null;
  const mean = r.reduce((s, x) => s + x, 0) / r.length;
  const variance = r.reduce((s, x) => s + (x - mean) ** 2, 0) / (r.length - 1);
  const std = Math.sqrt(variance);
  return std === 0 ? null : (mean * 252 - RISK_FREE_RATE) / (std * Math.sqrt(252));
}

function calcBeta(pr: number[], br: number[]): number | null {
  const n = Math.min(pr.length, br.length);
  if (n < 5) return null;
  const p = pr.slice(-n), b = br.slice(-n);
  const mP = p.reduce((s, x) => s + x, 0) / n;
  const mB = b.reduce((s, x) => s + x, 0) / n;
  let cov = 0, varB = 0;
  for (let i = 0; i < n; i++) { cov += (p[i] - mP) * (b[i] - mB); varB += (b[i] - mB) ** 2; }
  return varB === 0 ? null : cov / varB;
}

function calcVol(r: number[]): number | null {
  if (r.length < 2) return null;
  const mean = r.reduce((s, x) => s + x, 0) / r.length;
  const variance = r.reduce((s, x) => s + (x - mean) ** 2, 0) / (r.length - 1);
  return Math.sqrt(variance) * Math.sqrt(252) * 100;
}

function calcMaxDD(v: number[]): number | null {
  if (v.length < 2) return null;
  let peak = v[0], maxDD = 0;
  for (const x of v) { if (x > peak) peak = x; const dd = (peak - x) / peak; if (dd > maxDD) maxDD = dd; }
  return maxDD * 100;
}

function calcVaR95(r: number[], portfolioValue: number): number | null {
  if (r.length < 5) return null;
  const mean = r.reduce((s, x) => s + x, 0) / r.length;
  const variance = r.reduce((s, x) => s + (x - mean) ** 2, 0) / (r.length - 1);
  return Math.abs(mean - 1.645 * Math.sqrt(variance)) * portfolioValue;
}

// ─── Micro-components ───

const PanelHeader: React.FC<{ title: string; meta?: string }> = ({ title, meta }) => (
  <div style={{
    height: '22px', flexShrink: 0,
    borderBottom: `1px solid ${FINCEPT.ORANGE}50`,
    display: 'flex', alignItems: 'center', justifyContent: 'space-between',
    padding: '0 6px', backgroundColor: '#0D0D0D',
  }}>
    <span style={{ fontSize: '9px', fontWeight: 800, color: FINCEPT.ORANGE, letterSpacing: '0.5px' }}>{title}</span>
    {meta && <span style={{ fontSize: '8px', color: FINCEPT.MUTED, fontWeight: 600 }}>{meta}</span>}
  </div>
);

const MetricCard: React.FC<{
  label: string; value: string; color: string; sub: string;
}> = ({ label, value, color, sub }) => (
  <div style={{
    padding: '6px 8px',
    backgroundColor: '#080808',
    border: `1px solid ${FINCEPT.BORDER}`,
    borderTop: `2px solid ${color}40`,
  }}>
    <div style={{ fontSize: '7px', fontWeight: 700, color: FINCEPT.MUTED, letterSpacing: '0.5px', marginBottom: '2px' }}>{label}</div>
    <div style={{ fontSize: '13px', fontWeight: 800, color, fontFamily: TYPOGRAPHY.MONO }}>{value}</div>
    <div style={{ fontSize: '7px', color: FINCEPT.MUTED, marginTop: '1px' }}>{sub}</div>
  </div>
);

const StatChip: React.FC<{ label: string; value: string; color?: string }> = ({ label, value, color = FINCEPT.WHITE }) => (
  <div style={{ display: 'flex', alignItems: 'baseline', gap: '4px' }}>
    <span style={{ fontSize: '8px', color: FINCEPT.MUTED, fontWeight: 700, letterSpacing: '0.3px' }}>{label}</span>
    <span style={{ fontSize: '10px', fontWeight: 800, color, fontFamily: TYPOGRAPHY.MONO }}>{value}</span>
  </div>
);

// ─── Main Component ───

const PerformanceRiskPanel: React.FC<PerformanceRiskPanelProps> = ({ portfolioSummary, currency }) => {
  const [period, setPeriod] = useState<Period>('3M');
  const [loading, setLoading] = useState(false);
  const [values, setValues] = useState<number[]>([]);
  const [timestamps, setTimestamps] = useState<number[]>([]);
  const [benchReturns, setBenchReturns] = useState<number[]>([]);
  const fetchIdRef = useRef(0);

  const holdings = portfolioSummary.holdings;
  const holdingsKey = useMemo(
    () => holdings.map(h => `${h.symbol}:${h.quantity}`).sort().join(','),
    [holdings],
  );

  // Fetch historical data (with cache)
  useEffect(() => {
    if (holdings.length === 0) { setValues([]); setTimestamps([]); setBenchReturns([]); return; }
    const fetchId = ++fetchIdRef.current;
    let cancelled = false;
    const ckPrefix = `perf-risk:${holdingsKey}:${period}`;

    const fetchAll = async () => {
      // ── Try cache first ──────────────────────────────────────────────────────
      const cached = await cacheService.get<{ values: number[]; timestamps: number[]; benchReturns: number[] }>(ckPrefix);
      if (cached && !cancelled && fetchIdRef.current === fetchId) {
        setTimestamps(cached.data.timestamps);
        setValues(cached.data.values);
        setBenchReturns(cached.data.benchReturns);
        return; // No need to fetch from network
      }

      setLoading(true);
      try {
        const days = periodDays(period);
        const end = new Date();
        const start = new Date();
        start.setDate(start.getDate() - days);
        const startStr = fmtDate(start);
        const endStr = fmtDate(end);
        const symbols = holdings.map(h => h.symbol);
        const fetches = await Promise.allSettled([
          ...symbols.map(sym => yfinanceService.getHistoricalData(sym, startStr, endStr)),
          yfinanceService.getHistoricalData(BENCHMARK, startStr, endStr),
        ]);
        if (cancelled || fetchIdRef.current !== fetchId) return;

        const histMap = new Map<string, HistoricalDataPoint[]>();
        symbols.forEach((sym, i) => {
          const r = fetches[i];
          if (r.status === 'fulfilled' && r.value.length > 0) histMap.set(sym, r.value);
        });
        const benchResult = fetches[fetches.length - 1];
        const benchData = benchResult.status === 'fulfilled' ? benchResult.value : [];

        if (histMap.size === 0) { setValues([]); setTimestamps([]); setBenchReturns([]); return; }

        const holdingsList = holdings.map(h => ({ symbol: h.symbol, quantity: h.quantity }));
        const { timestamps: ts, values: vals } = buildValues(holdingsList, histMap);
        const bv = benchData.map(p => p.close);
        const br = dailyReturns(bv);

        if (!cancelled && fetchIdRef.current === fetchId) {
          setTimestamps(ts); setValues(vals); setBenchReturns(br);
          // Cache with 15m TTL (market data ages quickly)
          cacheService.set(ckPrefix, { values: vals, timestamps: ts, benchReturns: br }, 'market-historical', '15m');
        }
      } catch {
        if (!cancelled) { setValues([]); setTimestamps([]); setBenchReturns([]); }
      } finally {
        if (!cancelled) setLoading(false);
      }
    };

    fetchAll();
    return () => { cancelled = true; };
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [holdingsKey, period, portfolioSummary.portfolio.id]);

  // Derived metrics
  const portReturns = useMemo(() => dailyReturns(values), [values]);
  const sharpe = useMemo(() => calcSharpe(portReturns), [portReturns]);
  const beta = useMemo(() => calcBeta(portReturns, benchReturns), [portReturns, benchReturns]);
  const vol = useMemo(() => calcVol(portReturns), [portReturns]);
  const maxDD = useMemo(() => calcMaxDD(values), [values]);
  const var95 = useMemo(() => calcVaR95(portReturns, portfolioSummary.total_market_value), [portReturns, portfolioSummary.total_market_value]);

  const periodReturn = useMemo(() => {
    if (values.length < 2) return 0;
    const first = values[0], last = values[values.length - 1];
    return first === 0 ? 0 : ((last - first) / first) * 100;
  }, [values]);

  const riskScore = useMemo(() => {
    if (vol === null) return null;
    const sorted = [...holdings].sort((a, b) => b.weight - a.weight);
    const conc = sorted.slice(0, 3).reduce((s, h) => s + h.weight, 0);
    const volScore = Math.min(vol / 40, 1) * 50;
    const concScore = Math.min(conc / 80, 1) * 50;
    return Math.round(volScore + concScore);
  }, [vol, holdings]);

  // SVG chart path
  const { linePath, areaPath } = useMemo(() => {
    if (values.length < 2) return { linePath: 'M0,60 L600,60', areaPath: 'M0,60 L600,60 L600,80 L0,80 Z' };
    const min = Math.min(...values), max = Math.max(...values);
    const range = max - min || 1;
    const pad = 6;
    const H = 100;
    const pts = values.map((v, i) => ({
      x: (i / (values.length - 1)) * 600,
      y: pad + (1 - (v - min) / range) * (H - 2 * pad),
    }));
    const line = pts.map((p, i) => `${i === 0 ? 'M' : 'L'}${p.x.toFixed(1)},${p.y.toFixed(1)}`).join(' ');
    const last = pts[pts.length - 1];
    return { linePath: line, areaPath: `${line} L${last.x.toFixed(1)},${H} L0,${H} Z` };
  }, [values]);

  const lineColor = periodReturn >= 0 ? FINCEPT.GREEN : FINCEPT.RED;

  // Daily returns table data (newest first)
  const dailyData = useMemo(() => {
    if (values.length < 2) return [];
    return values.slice(1).map((v, i) => {
      const prev = values[i];
      const change = v - prev;
      const changePct = prev > 0 ? (change / prev) * 100 : 0;
      const d = new Date(timestamps[i + 1] * 1000);
      return { date: d.toISOString().split('T')[0], value: v, change, changePct };
    }).reverse();
  }, [values, timestamps]);

  // Risk recommendations
  const recommendations = useMemo(() => {
    const recs: { type: 'warn' | 'ok'; text: string }[] = [];
    if (sharpe !== null && sharpe < 1) recs.push({ type: 'warn', text: 'Low Sharpe — consider diversifying for better risk-adjusted return' });
    if (sharpe !== null && sharpe > 2) recs.push({ type: 'ok', text: 'Excellent Sharpe — strong risk-adjusted performance' });
    if (beta !== null && beta > 1.5) recs.push({ type: 'warn', text: 'High Beta — more volatile than market, consider defensive assets' });
    if (vol !== null && vol > 25) recs.push({ type: 'warn', text: 'High Volatility — consider adding bonds or low-vol assets' });
    if (maxDD !== null && maxDD > 20) recs.push({ type: 'warn', text: 'Large Drawdown — review stop-loss and position sizing' });
    if (beta !== null && beta >= 0.8 && beta <= 1.2) recs.push({ type: 'ok', text: 'Balanced Beta — good market correlation' });
    return recs;
  }, [sharpe, beta, vol, maxDD]);

  const hasData = values.length >= 2;

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden', backgroundColor: FINCEPT.DARK_BG }}>

      {/* ── Top command bar ── */}
      <div style={{
        height: '32px', flexShrink: 0,
        background: 'linear-gradient(180deg, #141414 0%, #0A0A0A 100%)',
        borderBottom: `1px solid ${FINCEPT.ORANGE}60`,
        display: 'flex', alignItems: 'center', padding: '0 10px', gap: '16px',
      }}>
        {/* Period selector */}
        <div style={{ display: 'flex', gap: '2px', alignItems: 'center' }}>
          <span style={{ fontSize: '8px', color: FINCEPT.MUTED, fontWeight: 700, marginRight: '4px' }}>PERIOD</span>
          {PERIODS.map(p => (
            <button key={p} onClick={() => setPeriod(p)} style={{
              padding: '2px 7px', fontSize: '8px', fontWeight: 700,
              backgroundColor: period === p ? FINCEPT.ORANGE : 'transparent',
              color: period === p ? '#000' : FINCEPT.GRAY,
              border: period === p ? 'none' : `1px solid ${FINCEPT.BORDER}`,
              cursor: 'pointer', letterSpacing: '0.3px',
            }}>{p}</button>
          ))}
          {loading && <Loader size={9} color={FINCEPT.ORANGE} style={{ marginLeft: '4px' }} className="animate-spin" />}
        </div>

        <div style={{ width: '1px', height: '16px', backgroundColor: `${FINCEPT.ORANGE}30` }} />

        {/* Key metrics chips */}
        {hasData ? (
          <>
            <StatChip label="PERIOD RTN" value={`${periodReturn >= 0 ? '+' : ''}${periodReturn.toFixed(2)}%`} color={valColor(periodReturn)} />
            <StatChip label="SHARPE" value={sharpe !== null ? sharpe.toFixed(2) : '--'} color={sharpe !== null ? (sharpe > 1 ? FINCEPT.GREEN : sharpe > 0 ? FINCEPT.YELLOW : FINCEPT.RED) : FINCEPT.MUTED} />
            <StatChip label={`BETA/${BENCHMARK}`} value={beta !== null ? beta.toFixed(2) : '--'} color={FINCEPT.CYAN} />
            <StatChip label="VOL" value={vol !== null ? `${vol.toFixed(1)}%` : '--'} color={vol !== null ? (vol < 15 ? FINCEPT.GREEN : vol < 25 ? FINCEPT.YELLOW : FINCEPT.RED) : FINCEPT.MUTED} />
            <StatChip label="MAX DD" value={maxDD !== null ? `-${maxDD.toFixed(1)}%` : '--'} color={maxDD !== null && maxDD > 10 ? FINCEPT.RED : FINCEPT.MUTED} />
            <StatChip label="VaR95" value={var95 !== null ? formatCurrency(var95, currency) : '--'} color={FINCEPT.YELLOW} />
          </>
        ) : (
          <span style={{ fontSize: '8px', color: FINCEPT.MUTED }}>
            {loading ? 'Loading data...' : 'No data for period'}
          </span>
        )}

        <div style={{ flex: 1 }} />
        <span style={{ fontSize: '7px', color: FINCEPT.MUTED }}>
          {hasData ? `${portReturns.length} TRADING DAYS · RF ${(RISK_FREE_RATE * 100).toFixed(0)}%` : ''}
        </span>
      </div>

      {/* ── Body: Left column + Right column ── */}
      <div style={{ flex: 1, display: 'flex', overflow: 'hidden', gap: '1px', backgroundColor: FINCEPT.BORDER }}>

        {/* LEFT: Chart + Risk cards + Recommendations */}
        <div style={{ width: '55%', display: 'flex', flexDirection: 'column', overflow: 'hidden', backgroundColor: FINCEPT.DARK_BG }}>

          {/* NAV Chart */}
          <div style={{ display: 'flex', flexDirection: 'column', overflow: 'hidden', borderBottom: `1px solid ${FINCEPT.BORDER}` }}>
            <PanelHeader title="PORTFOLIO NAV · PERFORMANCE CHART" meta={`vs ${BENCHMARK}`} />
            {!hasData ? (
              <div style={{ ...COMMON_STYLES.emptyState, height: '120px' }}>
                {loading
                  ? <Loader size={16} color={FINCEPT.ORANGE} className="animate-spin" />
                  : <><AlertCircle size={14} color={FINCEPT.MUTED} /><span style={{ fontSize: '9px', color: FINCEPT.MUTED, marginTop: '4px' }}>No data for period</span></>
                }
              </div>
            ) : (
              <div style={{ padding: '6px', backgroundColor: '#050505' }}>
                {/* Mini stat row above chart */}
                <div style={{ display: 'flex', gap: '12px', marginBottom: '5px' }}>
                  <div>
                    <span style={{ fontSize: '7px', color: FINCEPT.MUTED, fontWeight: 700 }}>CURRENT NAV  </span>
                    <span style={{ fontSize: '11px', fontWeight: 800, color: FINCEPT.YELLOW, fontFamily: TYPOGRAPHY.MONO }}>
                      {formatCurrency(values[values.length - 1], currency)}
                    </span>
                  </div>
                  <div>
                    <span style={{ fontSize: '7px', color: FINCEPT.MUTED, fontWeight: 700 }}>START NAV  </span>
                    <span style={{ fontSize: '11px', fontWeight: 800, color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO }}>
                      {formatCurrency(values[0], currency)}
                    </span>
                  </div>
                  <div>
                    <span style={{ fontSize: '7px', color: FINCEPT.MUTED, fontWeight: 700 }}>RETURN  </span>
                    <span style={{ fontSize: '11px', fontWeight: 800, color: valColor(periodReturn), fontFamily: TYPOGRAPHY.MONO }}>
                      {periodReturn >= 0 ? '+' : ''}{periodReturn.toFixed(2)}%
                    </span>
                  </div>
                </div>

                {/* SVG Chart */}
                <div style={{ border: `1px solid ${FINCEPT.BORDER}30`, backgroundColor: '#030303' }}>
                  <svg width="100%" height="110" viewBox="0 0 600 100" preserveAspectRatio="none" style={{ display: 'block' }}>
                    <defs>
                      <linearGradient id="perfGrad" x1="0" y1="0" x2="0" y2="1">
                        <stop offset="0%" stopColor={lineColor} stopOpacity="0.18" />
                        <stop offset="100%" stopColor={lineColor} stopOpacity="0" />
                      </linearGradient>
                    </defs>
                    {[25, 50, 75].map(y => (
                      <line key={y} x1="0" y1={y} x2="600" y2={y} stroke="#1a1a1a" strokeWidth="0.5" />
                    ))}
                    <path d={areaPath} fill="url(#perfGrad)" />
                    <path d={linePath} fill="none" stroke={lineColor} strokeWidth="1.5" />
                  </svg>
                </div>
              </div>
            )}
          </div>

          {/* Risk metric cards */}
          <div style={{ display: 'flex', flexDirection: 'column', overflow: 'hidden', flex: 1 }}>
            <PanelHeader title="RISK METRICS" meta={`${BENCHMARK} BENCHMARK · ${(RISK_FREE_RATE * 100).toFixed(0)}% RF`} />
            <div style={{ flex: 1, overflow: 'auto', padding: '6px' }}>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '4px', marginBottom: '6px' }}>
                <MetricCard
                  label="SHARPE RATIO"
                  value={sharpe !== null ? sharpe.toFixed(2) : '--'}
                  color={sharpe !== null ? (sharpe > 1 ? FINCEPT.GREEN : sharpe > 0 ? FINCEPT.YELLOW : FINCEPT.RED) : FINCEPT.GRAY}
                  sub="Risk-adj. return (ann.)"
                />
                <MetricCard
                  label={`BETA vs ${BENCHMARK}`}
                  value={beta !== null ? beta.toFixed(2) : '--'}
                  color={beta !== null ? (beta < 0.8 ? FINCEPT.BLUE : beta < 1.2 ? FINCEPT.CYAN : FINCEPT.ORANGE) : FINCEPT.GRAY}
                  sub="Market sensitivity"
                />
                <MetricCard
                  label="VOLATILITY"
                  value={vol !== null ? `${vol.toFixed(1)}%` : '--'}
                  color={vol !== null ? (vol < 15 ? FINCEPT.GREEN : vol < 25 ? FINCEPT.YELLOW : FINCEPT.RED) : FINCEPT.GRAY}
                  sub="Annualized std dev"
                />
                <MetricCard
                  label="MAX DRAWDOWN"
                  value={maxDD !== null ? `-${maxDD.toFixed(1)}%` : '--'}
                  color={maxDD !== null ? (maxDD > 20 ? FINCEPT.RED : maxDD > 10 ? FINCEPT.ORANGE : FINCEPT.GREEN) : FINCEPT.GRAY}
                  sub="Peak-to-trough"
                />
                <MetricCard
                  label="VaR (95%)"
                  value={var95 !== null ? formatCurrency(var95, currency) : '--'}
                  color={FINCEPT.YELLOW}
                  sub="1-day 95% confidence"
                />
                <MetricCard
                  label="RISK SCORE"
                  value={riskScore !== null ? `${riskScore}/100` : '--'}
                  color={riskScore !== null ? (riskScore > 70 ? FINCEPT.RED : riskScore > 40 ? FINCEPT.YELLOW : FINCEPT.GREEN) : FINCEPT.GRAY}
                  sub="Vol + concentration"
                />
              </div>

              {/* Recommendations */}
              {recommendations.length > 0 && (
                <div>
                  <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.ORANGE, letterSpacing: '0.5px', marginBottom: '3px' }}>
                    RISK RECOMMENDATIONS
                  </div>
                  {recommendations.map((r, i) => (
                    <div key={i} style={{
                      padding: '3px 6px', marginBottom: '2px', fontSize: '8px',
                      backgroundColor: r.type === 'warn' ? 'rgba(255,59,59,0.08)' : 'rgba(0,214,111,0.08)',
                      color: r.type === 'warn' ? FINCEPT.RED : FINCEPT.GREEN,
                      borderLeft: `2px solid ${r.type === 'warn' ? FINCEPT.RED : FINCEPT.GREEN}`,
                    }}>
                      [{r.type === 'warn' ? '!' : '✓'}] {r.text}
                    </div>
                  ))}
                </div>
              )}
            </div>
          </div>
        </div>

        {/* RIGHT: Daily Returns table */}
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', overflow: 'hidden', backgroundColor: FINCEPT.DARK_BG }}>
          <PanelHeader title="DAILY RETURNS" meta={`${dailyData.length} DAYS`} />

          {!hasData ? (
            <div style={{ ...COMMON_STYLES.emptyState, flex: 1 }}>
              {loading
                ? <Loader size={16} color={FINCEPT.ORANGE} className="animate-spin" />
                : <span style={{ fontSize: '9px', color: FINCEPT.MUTED }}>No data for period</span>
              }
            </div>
          ) : (
            <div style={{ flex: 1, overflow: 'auto' }}>
              <table style={{ width: '100%', borderCollapse: 'collapse' }}>
                <thead style={{ position: 'sticky', top: 0, zIndex: 1 }}>
                  <tr style={{ backgroundColor: '#0A0A0A' }}>
                    {['DATE', 'NAV', 'CHANGE', 'CHG %'].map((h, i) => (
                      <th key={h} style={{
                        padding: '4px 8px', fontSize: '8px', fontWeight: 700,
                        color: FINCEPT.ORANGE, textAlign: i === 0 ? 'left' : 'right',
                        borderBottom: `1px solid ${FINCEPT.ORANGE}50`,
                        letterSpacing: '0.3px', whiteSpace: 'nowrap',
                      }}>{h}</th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {dailyData.map((d, i) => (
                    <tr key={d.date} style={{
                      borderBottom: `1px solid ${FINCEPT.BORDER}20`,
                      backgroundColor: i % 2 === 0 ? 'transparent' : 'rgba(255,255,255,0.01)',
                    }}>
                      <td style={{ padding: '2px 8px', fontSize: '9px', color: FINCEPT.GRAY, fontFamily: TYPOGRAPHY.MONO }}>{d.date}</td>
                      <td style={{ padding: '2px 8px', fontSize: '9px', color: FINCEPT.CYAN, textAlign: 'right', fontWeight: 600, fontFamily: TYPOGRAPHY.MONO }}>
                        {formatCurrency(d.value, currency)}
                      </td>
                      <td style={{ padding: '2px 8px', fontSize: '9px', color: valColor(d.change), textAlign: 'right', fontWeight: 600, fontFamily: TYPOGRAPHY.MONO }}>
                        {d.change >= 0 ? '+' : ''}{formatCurrency(d.change, currency)}
                      </td>
                      <td style={{ padding: '2px 8px', fontSize: '9px', color: valColor(d.changePct), textAlign: 'right', fontWeight: 700, fontFamily: TYPOGRAPHY.MONO }}>
                        {d.changePct >= 0 ? '+' : ''}{d.changePct.toFixed(2)}%
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default PerformanceRiskPanel;
