import React, { useState, useEffect, useMemo, useRef } from 'react';
import { Activity, Loader } from 'lucide-react';
import { FINCEPT } from '../finceptStyles';
import { formatPercent, formatCurrency } from '../portfolio/utils';
import { valColor } from './helpers';
import { useTranslation } from 'react-i18next';
import { yfinanceService } from '../../../../services/markets/yfinanceService';
import type { HistoricalDataPoint } from '../../../../services/markets/yfinanceService';
import type { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import { cacheService, CacheService } from '../../../../services/cache/cacheService';

interface PerformanceChartPanelProps {
  portfolioSummary: PortfolioSummary;
  currency: string;
}

type Period = '1D' | '1W' | '1M' | '3M' | 'YTD' | '1Y' | 'ALL';
const PERIODS: Period[] = ['1D', '1W', '1M', '3M', 'YTD', '1Y', 'ALL'];

/** Number of calendar days to fetch for each period */
function periodToDays(period: Period): number {
  switch (period) {
    case '1D': return 2;    // need previous close for reference
    case '1W': return 9;
    case '1M': return 35;
    case '3M': return 95;
    case 'YTD': {
      const now = new Date();
      const jan1 = new Date(now.getFullYear(), 0, 1);
      return Math.ceil((now.getTime() - jan1.getTime()) / (24 * 60 * 60 * 1000)) + 5;
    }
    case '1Y': return 370;
    case 'ALL': return 1825; // ~5 years
  }
}

function formatDate(date: Date): string {
  return date.toISOString().split('T')[0];
}

/**
 * Given per-symbol historical close prices and portfolio holdings (qty),
 * compute daily portfolio market value by summing qty * close for each day.
 */
function buildPortfolioValueSeries(
  holdingsMap: Map<string, { quantity: number }>,
  historyMap: Map<string, HistoricalDataPoint[]>,
): { date: number; value: number }[] {
  // Collect all unique timestamps across all symbols
  const allTimestamps = new Set<number>();
  for (const [, data] of historyMap) {
    for (const pt of data) {
      allTimestamps.add(pt.timestamp);
    }
  }

  const sortedTimestamps = [...allTimestamps].sort((a, b) => a - b);
  if (sortedTimestamps.length === 0) return [];

  // Build lookup: symbol -> timestamp -> close
  const closeLookup = new Map<string, Map<number, number>>();
  for (const [symbol, data] of historyMap) {
    const map = new Map<number, number>();
    for (const pt of data) {
      map.set(pt.timestamp, pt.close);
    }
    closeLookup.set(symbol, map);
  }

  const series: { date: number; value: number }[] = [];
  // Track last known close per symbol for forward-fill
  const lastClose = new Map<string, number>();

  for (const ts of sortedTimestamps) {
    let portfolioValue = 0;
    let allSymbolsHaveData = false;

    for (const [symbol, holding] of holdingsMap) {
      const symbolCloses = closeLookup.get(symbol);
      let close = symbolCloses?.get(ts);

      if (close !== undefined) {
        lastClose.set(symbol, close);
        allSymbolsHaveData = true;
      } else {
        // Forward-fill from last known close
        close = lastClose.get(symbol);
      }

      if (close !== undefined) {
        portfolioValue += holding.quantity * close;
      }
    }

    // Only add points where we have at least some data
    if (allSymbolsHaveData || lastClose.size > 0) {
      series.push({ date: ts, value: portfolioValue });
    }
  }

  return series;
}

const PerformanceChartPanel: React.FC<PerformanceChartPanelProps> = ({ portfolioSummary, currency }) => {
  const { t } = useTranslation('portfolio');
  const [period, setPeriod] = useState<Period>('1M');
  const [loading, setLoading] = useState(false);
  const [valueSeries, setValueSeries] = useState<{ date: number; value: number }[]>([]);
  const fetchIdRef = useRef(0);

  const pnlPct = portfolioSummary.total_unrealized_pnl_percent;
  const holdings = portfolioSummary.holdings;

  // Stable key: only re-fetch when the actual symbol set, quantities, or period changes
  // (NOT when portfolioSummary reference changes from auto-refresh)
  const holdingsKey = useMemo(
    () => holdings.map(h => `${h.symbol}:${h.quantity}`).sort().join(','),
    [holdings],
  );
  const portfolioId = portfolioSummary.portfolio.id;

  // Fetch historical prices for all holdings when period or holdings change
  useEffect(() => {
    if (holdings.length === 0) {
      setValueSeries([]);
      return;
    }

    const fetchId = ++fetchIdRef.current;
    let cancelled = false;

    const fetchAll = async () => {
      setLoading(true);
      try {
        const days = periodToDays(period);
        const endDate = new Date();
        const startDate = new Date();
        startDate.setDate(startDate.getDate() - days);

        const startStr = formatDate(startDate);
        const endStr = formatDate(endDate);

        // Fetch history for all symbols in parallel with caching
        const symbols = holdings.map(h => h.symbol);
        const results = await Promise.allSettled(
          symbols.map(async (sym) => {
            const cacheKey = CacheService.key('market-historical', sym, startStr, endStr);
            const cached = await cacheService.get<HistoricalDataPoint[]>(cacheKey);

            if (cached && !cached.isStale) {
              return cached.data;
            }

            const data = await yfinanceService.getHistoricalData(sym, startStr, endStr);
            await cacheService.set(cacheKey, data, 'market-historical', 3600); // 1 hour TTL
            return data;
          })
        );

        if (cancelled || fetchIdRef.current !== fetchId) return;

        // Build maps
        const holdingsMap = new Map<string, { quantity: number }>();
        for (const h of holdings) {
          holdingsMap.set(h.symbol, { quantity: h.quantity });
        }

        const historyMap = new Map<string, HistoricalDataPoint[]>();
        results.forEach((r, i) => {
          if (r.status === 'fulfilled' && r.value.length > 0) {
            historyMap.set(symbols[i], r.value);
          }
        });

        if (historyMap.size === 0) {
          // No data from yfinance - fall back to cost basis → current value
          const costBasis = holdings.reduce((sum, h) => sum + h.cost_basis, 0);
          const mktVal = holdings.reduce((sum, h) => sum + h.market_value, 0);
          setValueSeries([
            { date: startDate.getTime() / 1000, value: costBasis },
            { date: endDate.getTime() / 1000, value: mktVal },
          ]);
          return;
        }

        const series = buildPortfolioValueSeries(holdingsMap, historyMap);

        // Append current live value as the final point if last data point is stale
        if (series.length > 0) {
          const lastTs = series[series.length - 1].date;
          const nowTs = Date.now() / 1000;
          const mktVal = holdings.reduce((sum, h) => sum + h.market_value, 0);
          if (nowTs - lastTs > 3600) {
            series.push({ date: nowTs, value: mktVal });
          }
        }

        if (!cancelled && fetchIdRef.current === fetchId) {
          setValueSeries(series);
        }
      } catch (err) {
        console.error('[PerformanceChart] Error fetching history:', err);
        if (!cancelled) {
          const costBasis = holdings.reduce((sum, h) => sum + h.cost_basis, 0);
          const mktVal = holdings.reduce((sum, h) => sum + h.market_value, 0);
          setValueSeries([
            { date: Date.now() / 1000 - periodToDays(period) * 86400, value: costBasis },
            { date: Date.now() / 1000, value: mktVal },
          ]);
        }
      } finally {
        if (!cancelled) setLoading(false);
      }
    };

    fetchAll();
    return () => { cancelled = true; };
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [holdingsKey, period, portfolioId]);

  // Generate SVG path from value series
  const { linePath, areaPath, hasHistory, minVal, maxVal } = useMemo(() => {
    if (valueSeries.length < 2) {
      return {
        linePath: 'M0,90 L500,90',
        areaPath: 'M0,90 L500,90 L500,180 L0,180 Z',
        hasHistory: false,
        minVal: 0,
        maxVal: 0,
      };
    }

    const values = valueSeries.map(p => p.value);
    const min = Math.min(...values);
    const max = Math.max(...values);
    const range = max - min || 1;
    const padding = 10;

    const pts = valueSeries.map((p, i) => {
      const x = (i / (valueSeries.length - 1)) * 500;
      const y = padding + (1 - (p.value - min) / range) * (180 - 2 * padding);
      return { x, y };
    });

    const line = pts.map((p, i) => `${i === 0 ? 'M' : 'L'}${p.x.toFixed(1)},${p.y.toFixed(1)}`).join(' ');
    const lastPt = pts[pts.length - 1];
    const area = `${line} L${lastPt.x.toFixed(1)},180 L0,180 Z`;

    return { linePath: line, areaPath: area, hasHistory: true, minVal: min, maxVal: max };
  }, [valueSeries]);

  // Determine chart color from first→last value change (not overall P&L)
  const chartPnl = useMemo(() => {
    if (valueSeries.length < 2) return pnlPct;
    const first = valueSeries[0].value;
    const last = valueSeries[valueSeries.length - 1].value;
    if (first === 0) return 0;
    return ((last - first) / first) * 100;
  }, [valueSeries, pnlPct]);

  const lineColor = chartPnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED;
  const gradientId = chartPnl >= 0 ? 'portGradGreen' : 'portGradRed';

  return (
    <div style={{ flex: 3, borderRight: `1px solid ${FINCEPT.BORDER}`, display: 'flex', flexDirection: 'column', minWidth: 0 }}>
      {/* Header */}
      <div style={{
        padding: '4px 10px', backgroundColor: '#0A0A0A', borderBottom: '1px solid #111',
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
          <Activity size={10} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '10px', fontWeight: 700, color: FINCEPT.ORANGE, letterSpacing: '0.5px' }}>{t('chart.performance', 'PERFORMANCE')}</span>
          {loading && <Loader size={9} color={FINCEPT.GRAY} className="animate-spin" />}
          {hasHistory && !loading && (
            <span style={{ fontSize: '8px', color: FINCEPT.GRAY }}>{valueSeries.length} pts</span>
          )}
        </div>
        <div style={{ display: 'flex', gap: '2px' }}>
          {PERIODS.map(pr => (
            <button key={pr} onClick={() => setPeriod(pr)} style={{
              padding: '2px 6px', fontSize: '8px', fontWeight: 700,
              backgroundColor: period === pr ? FINCEPT.ORANGE : 'transparent',
              color: period === pr ? '#000' : FINCEPT.GRAY,
              border: 'none', cursor: 'pointer',
            }}>
              {pr}
            </button>
          ))}
        </div>
      </div>

      {/* Info Bar — sits ABOVE the chart, not overlapping */}
      <div style={{
        padding: '6px 12px', backgroundColor: '#080808',
        borderBottom: '1px solid #111',
        display: 'flex', alignItems: 'center', justifyContent: 'space-between',
      }}>
        {/* Left: legend + period change % */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <div style={{ width: '14px', height: '3px', backgroundColor: lineColor }} />
          <span style={{ fontSize: '11px', fontWeight: 700, color: FINCEPT.WHITE }}>
            {t('chart.portfolio', 'PORTFOLIO')}
          </span>
          {hasHistory && (
            <span style={{ fontSize: '11px', fontWeight: 700, color: valColor(chartPnl) }}>
              {chartPnl >= 0 ? '+' : ''}{chartPnl.toFixed(2)}%
            </span>
          )}
        </div>
        {/* Right: total return + market value */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{ textAlign: 'right' }}>
            <span style={{ fontSize: '9px', color: FINCEPT.WHITE, fontWeight: 600, marginRight: '6px' }}>{t('chart.totalReturn', 'TOTAL RETURN')}</span>
            <span style={{ fontSize: '14px', fontWeight: 800, color: valColor(pnlPct) }}>
              {formatPercent(pnlPct)}
            </span>
          </div>
          <div style={{ width: '1px', height: '16px', backgroundColor: FINCEPT.BORDER }} />
          <div style={{ textAlign: 'right' }}>
            <span style={{ fontSize: '9px', color: FINCEPT.WHITE, fontWeight: 600, marginRight: '6px' }}>NAV</span>
            <span style={{ fontSize: '13px', fontWeight: 700, color: FINCEPT.YELLOW }}>
              {formatCurrency(portfolioSummary.total_market_value, currency)}
            </span>
          </div>
        </div>
      </div>

      {/* Chart Area — clean SVG with Y-axis labels on the side */}
      <div style={{ flex: 1, backgroundColor: '#030303', position: 'relative', overflow: 'hidden', display: 'flex' }}>
        {/* Y-axis labels column */}
        {hasHistory && maxVal > 0 && (
          <div style={{
            width: '52px', flexShrink: 0, display: 'flex', flexDirection: 'column',
            justifyContent: 'space-between', padding: '6px 4px',
          }}>
            <span style={{ fontSize: '9px', fontWeight: 600, color: FINCEPT.WHITE }}>{formatCurrency(maxVal, currency)}</span>
            <span style={{ fontSize: '9px', fontWeight: 600, color: FINCEPT.WHITE }}>{formatCurrency(minVal, currency)}</span>
          </div>
        )}

        {/* SVG chart */}
        <div style={{ flex: 1, position: 'relative' }}>
          <svg width="100%" height="100%" viewBox="0 0 500 180" preserveAspectRatio="none" style={{ display: 'block' }}>
            <defs>
              <linearGradient id={gradientId} x1="0" y1="0" x2="0" y2="1">
                <stop offset="0%" stopColor={lineColor} stopOpacity="0.15" />
                <stop offset="100%" stopColor={lineColor} stopOpacity="0" />
              </linearGradient>
            </defs>
            {/* Grid lines */}
            {[36, 72, 108, 144].map(y => (
              <line key={y} x1="0" y1={y} x2="500" y2={y} stroke="#1a1a1a" strokeWidth="0.5" />
            ))}
            {[100, 200, 300, 400].map(x => (
              <line key={x} x1={x} y1="0" x2={x} y2="180" stroke="#1a1a1a" strokeWidth="0.5" />
            ))}
            {/* Area fill */}
            <path d={areaPath} fill={`url(#${gradientId})`} />
            {/* Portfolio line */}
            <path d={linePath} fill="none" stroke={lineColor} strokeWidth="2" />
          </svg>

          {/* Loading overlay */}
          {loading && (
            <div style={{
              position: 'absolute', inset: 0,
              display: 'flex', alignItems: 'center', justifyContent: 'center',
              backgroundColor: 'rgba(0,0,0,0.5)',
            }}>
              <Loader size={16} color={FINCEPT.ORANGE} className="animate-spin" />
            </div>
          )}

          {/* No-data message */}
          {!hasHistory && !loading && (
            <div style={{
              position: 'absolute', inset: 0,
              display: 'flex', alignItems: 'center', justifyContent: 'center',
            }}>
              <span style={{ fontSize: '11px', color: FINCEPT.WHITE, fontWeight: 600 }}>
                {t('chart.historyMessage', 'Add holdings to see portfolio performance over time')}
              </span>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default PerformanceChartPanel;
