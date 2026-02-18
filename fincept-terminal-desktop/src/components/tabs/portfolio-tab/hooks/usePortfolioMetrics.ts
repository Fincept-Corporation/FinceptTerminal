import { useMemo, useEffect, useState, useRef } from 'react';
import { yfinanceService } from '../../../../services/markets/yfinanceService';
import type { HistoricalDataPoint } from '../../../../services/markets/yfinanceService';
import type { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import type { ComputedMetrics } from '../types';

const RISK_FREE_RATE = 0.04; // 4% annualized
const BENCHMARK_SYMBOL = 'SPY';
const HISTORY_DAYS = 180; // ~6 months of daily data

function formatDate(d: Date): string {
  return d.toISOString().split('T')[0];
}

/**
 * Build daily portfolio value series from per-symbol close prices + quantities.
 * Returns array of daily portfolio values aligned to timestamps.
 */
function buildDailyPortfolioValues(
  holdings: { symbol: string; quantity: number }[],
  historyMap: Map<string, HistoricalDataPoint[]>,
): { timestamps: number[]; values: number[] } {
  // Collect all timestamps
  const allTs = new Set<number>();
  for (const [, data] of historyMap) {
    for (const pt of data) allTs.add(pt.timestamp);
  }
  const sorted = [...allTs].sort((a, b) => a - b);
  if (sorted.length === 0) return { timestamps: [], values: [] };

  // Lookup: symbol -> ts -> close
  const lookup = new Map<string, Map<number, number>>();
  for (const [sym, data] of historyMap) {
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
      if (close !== undefined) {
        lastClose.set(h.symbol, close);
        hasAny = true;
      } else {
        close = lastClose.get(h.symbol);
      }
      if (close !== undefined) val += h.quantity * close;
    }
    if (hasAny || lastClose.size > 0) {
      timestamps.push(ts);
      values.push(val);
    }
  }
  return { timestamps, values };
}

/** Convert value series to daily log returns */
function dailyReturns(values: number[]): number[] {
  const ret: number[] = [];
  for (let i = 1; i < values.length; i++) {
    if (values[i - 1] > 0) {
      ret.push((values[i] - values[i - 1]) / values[i - 1]);
    }
  }
  return ret;
}

/** Annualized Sharpe ratio from daily returns */
function calcSharpe(returns: number[]): number | null {
  if (returns.length < 5) return null;
  const mean = returns.reduce((s, r) => s + r, 0) / returns.length;
  const variance = returns.reduce((s, r) => s + (r - mean) ** 2, 0) / (returns.length - 1);
  const std = Math.sqrt(variance);
  if (std === 0) return null;
  const annReturn = mean * 252;
  const annStd = std * Math.sqrt(252);
  return (annReturn - RISK_FREE_RATE) / annStd;
}

/** Beta vs benchmark from daily returns */
function calcBeta(portReturns: number[], benchReturns: number[]): number | null {
  const n = Math.min(portReturns.length, benchReturns.length);
  if (n < 5) return null;
  const pr = portReturns.slice(-n);
  const br = benchReturns.slice(-n);
  const meanP = pr.reduce((s, r) => s + r, 0) / n;
  const meanB = br.reduce((s, r) => s + r, 0) / n;
  let cov = 0, varB = 0;
  for (let i = 0; i < n; i++) {
    cov += (pr[i] - meanP) * (br[i] - meanB);
    varB += (br[i] - meanB) ** 2;
  }
  if (varB === 0) return null;
  return cov / varB;
}

/** Annualized volatility from daily returns */
function calcVolatility(returns: number[]): number | null {
  if (returns.length < 2) return null;
  const mean = returns.reduce((s, r) => s + r, 0) / returns.length;
  const variance = returns.reduce((s, r) => s + (r - mean) ** 2, 0) / (returns.length - 1);
  return Math.sqrt(variance) * Math.sqrt(252) * 100; // as percentage
}

/** Max drawdown from portfolio value series */
function calcMaxDrawdown(values: number[]): number | null {
  if (values.length < 2) return null;
  let peak = values[0];
  let maxDD = 0;
  for (const v of values) {
    if (v > peak) peak = v;
    const dd = (peak - v) / peak;
    if (dd > maxDD) maxDD = dd;
  }
  return maxDD * 100; // as percentage
}

/** VaR 95% parametric from daily returns, scaled to portfolio value */
function calcVaR95(returns: number[], portfolioValue: number): number | null {
  if (returns.length < 5) return null;
  const mean = returns.reduce((s, r) => s + r, 0) / returns.length;
  const variance = returns.reduce((s, r) => s + (r - mean) ** 2, 0) / (returns.length - 1);
  const std = Math.sqrt(variance);
  // 1-day 95% VaR = portfolio_value * (mean - 1.645 * std)
  const varPct = Math.abs(mean - 1.645 * std);
  return varPct * portfolioValue;
}

export const usePortfolioMetrics = (summary: PortfolioSummary | null): ComputedMetrics => {
  const [historicalMetrics, setHistoricalMetrics] = useState<{
    sharpe: number | null;
    beta: number | null;
    volatility: number | null;
    maxDrawdown: number | null;
    var95: number | null;
  } | null>(null);

  const fetchIdRef = useRef(0);

  // Stable key to avoid refetching on every auto-refresh
  const holdingsKey = useMemo(
    () => summary?.holdings.map(h => `${h.symbol}:${h.quantity}`).sort().join(',') ?? '',
    [summary?.holdings],
  );
  const portfolioId = summary?.portfolio.id ?? '';

  // Fetch historical data and compute proper metrics
  useEffect(() => {
    if (!summary || summary.holdings.length === 0) {
      setHistoricalMetrics(null);
      return;
    }

    const fetchId = ++fetchIdRef.current;
    let cancelled = false;

    const compute = async () => {
      try {
        const end = new Date();
        const start = new Date();
        start.setDate(start.getDate() - HISTORY_DAYS);
        const startStr = formatDate(start);
        const endStr = formatDate(end);

        const symbols = summary.holdings.map(h => h.symbol);

        // Fetch all holdings + benchmark in parallel
        const fetches = await Promise.allSettled([
          ...symbols.map(sym => yfinanceService.getHistoricalData(sym, startStr, endStr)),
          yfinanceService.getHistoricalData(BENCHMARK_SYMBOL, startStr, endStr),
        ]);

        if (cancelled || fetchIdRef.current !== fetchId) return;

        // Build history map for holdings
        const historyMap = new Map<string, HistoricalDataPoint[]>();
        symbols.forEach((sym, i) => {
          const r = fetches[i];
          if (r.status === 'fulfilled' && r.value.length > 0) {
            historyMap.set(sym, r.value);
          }
        });

        // Benchmark data
        const benchResult = fetches[fetches.length - 1];
        const benchData = benchResult.status === 'fulfilled' ? benchResult.value : [];

        if (historyMap.size === 0) {
          // No data available
          if (!cancelled) setHistoricalMetrics(null);
          return;
        }

        // Build portfolio daily values
        const holdingsList = summary.holdings.map(h => ({ symbol: h.symbol, quantity: h.quantity }));
        const { values } = buildDailyPortfolioValues(holdingsList, historyMap);
        const portReturns = dailyReturns(values);

        // Benchmark returns
        const benchValues = benchData.map(p => p.close);
        const benchReturns = dailyReturns(benchValues);

        const sharpe = calcSharpe(portReturns);
        const beta = calcBeta(portReturns, benchReturns);
        const volatility = calcVolatility(portReturns);
        const maxDrawdown = calcMaxDrawdown(values);
        const var95 = calcVaR95(portReturns, summary.total_market_value);

        if (!cancelled && fetchIdRef.current === fetchId) {
          setHistoricalMetrics({ sharpe, beta, volatility, maxDrawdown, var95 });
        }
      } catch (err) {
        console.error('[usePortfolioMetrics] Error computing metrics:', err);
        if (!cancelled) setHistoricalMetrics(null);
      }
    };

    compute();
    return () => { cancelled = true; };
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [holdingsKey, portfolioId]);

  return useMemo(() => {
    if (!summary || summary.holdings.length === 0) {
      return { sharpe: null, beta: null, volatility: null, maxDrawdown: null, var95: null, riskScore: null, concentrationTop3: null };
    }

    const holdings = summary.holdings;

    // Concentration: sum of top 3 holdings by weight
    const sorted = [...holdings].sort((a, b) => b.weight - a.weight);
    const concentrationTop3 = sorted.slice(0, 3).reduce((s, h) => s + h.weight, 0);

    const sharpe = historicalMetrics?.sharpe != null ? Math.round(historicalMetrics.sharpe * 100) / 100 : null;
    const beta = historicalMetrics?.beta != null ? Math.round(historicalMetrics.beta * 100) / 100 : null;
    const volatility = historicalMetrics?.volatility != null ? Math.round(historicalMetrics.volatility * 10) / 10 : null;
    const maxDrawdown = historicalMetrics?.maxDrawdown != null ? Math.round(historicalMetrics.maxDrawdown * 10) / 10 : null;
    const var95 = historicalMetrics?.var95 != null ? Math.round(historicalMetrics.var95 * 100) / 100 : null;

    // Risk score from volatility + concentration
    let riskScore: number | null = null;
    if (volatility !== null) {
      const volScore = Math.min(volatility / 40, 1) * 50;
      const concScore = Math.min(concentrationTop3 / 80, 1) * 50;
      riskScore = Math.round(volScore + concScore);
    }

    return {
      sharpe,
      beta,
      volatility,
      maxDrawdown,
      var95,
      riskScore,
      concentrationTop3: Math.round(concentrationTop3 * 10) / 10,
    };
  }, [summary, historicalMetrics]);
};
