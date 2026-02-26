/**
 * AnalyticsSectorsPanel — Merged Analytics + Sectors view, FINCEPT-styled.
 * Phase 1B: Added ANALYTICS sub-tab (calculate_portfolio_metrics, analyze_portfolio_performance,
 * ffn_calculate_performance, ffn_monthly_returns) and CORRELATION sub-tab
 * (fortitudo_covariance_analysis, ffn_compare_assets).
 */
import React, { useMemo, useEffect, useState, useCallback } from 'react';
import { invoke } from '@tauri-apps/api/core';
import { useTranslation } from 'react-i18next';
import { TrendingUp, TrendingDown, PieChart, BarChart3, Loader, Zap } from 'lucide-react';
import { FINCEPT, COMMON_STYLES, BORDERS } from '../finceptStyles';
import { valColor } from '../components/helpers';
import { formatCurrency, formatPercent } from '../portfolio/utils';
import { sectorService } from '../../../../services/data-sources/sectorService';
import type { PortfolioSummary, PortfolioHolding } from '../../../../services/portfolio/portfolioService';
import { cacheService } from '../../../../services/cache/cacheService';

interface AnalyticsSectorsPanelProps {
  portfolioSummary: PortfolioSummary;
  currency: string;
}

interface SectorAllocation {
  sector: string;
  value: number;
  weight: number;
  holdings: PortfolioHolding[];
  color: string;
}

const SECTOR_COLORS = [
  FINCEPT.ORANGE, FINCEPT.CYAN, FINCEPT.GREEN, FINCEPT.YELLOW,
  FINCEPT.PURPLE, FINCEPT.BLUE, FINCEPT.RED,
];

// ─── Metric Card ───
const MetricCard: React.FC<{
  label: string; value: string; color: string; sub?: string;
}> = ({ label, value, color, sub }) => (
  <div style={{
    padding: '8px 10px', backgroundColor: FINCEPT.PANEL_BG,
    border: BORDERS.STANDARD, borderRadius: '2px',
  }}>
    <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GRAY, letterSpacing: '0.5px', textTransform: 'uppercase', marginBottom: '2px' }}>
      {label}
    </div>
    <div style={{ fontSize: '13px', fontWeight: 700, color }}>{value}</div>
    {sub && <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginTop: '1px' }}>{sub}</div>}
  </div>
);

// ─── Mini SVG Pie ───
const SectorPie: React.FC<{ allocations: SectorAllocation[] }> = ({ allocations }) => {
  const R = 60, CX = 70, CY = 70;
  let angle = 0;
  const arcs = allocations.map(s => {
    const start = angle;
    const sweep = (s.weight / 100) * 360;
    angle += sweep;
    return { ...s, start, end: angle };
  });

  const polar = (a: number) => ({
    x: CX + R * Math.cos((a - 90) * Math.PI / 180),
    y: CY + R * Math.sin((a - 90) * Math.PI / 180),
  });

  return (
    <svg width="140" height="140">
      {arcs.map(a => {
        const s = polar(a.start), e = polar(a.end);
        const large = a.end - a.start > 180 ? 1 : 0;
        return (
          <path
            key={a.sector}
            d={`M ${CX} ${CY} L ${s.x} ${s.y} A ${R} ${R} 0 ${large} 1 ${e.x} ${e.y} Z`}
            fill={a.color} stroke={FINCEPT.DARK_BG} strokeWidth="1.5" opacity="0.85"
          />
        );
      })}
      <circle cx={CX} cy={CY} r={R * 0.35} fill={FINCEPT.PANEL_BG} />
      <text x={CX} y={CY + 4} textAnchor="middle" fill={FINCEPT.WHITE} fontSize="11" fontWeight="700">
        {allocations.length}
      </text>
    </svg>
  );
};

const AnalyticsSectorsPanel: React.FC<AnalyticsSectorsPanelProps> = ({ portfolioSummary, currency }) => {
  const { t } = useTranslation('portfolio');
  const holdings = portfolioSummary.holdings;
  const [sectorsLoaded, setSectorsLoaded] = useState(false);
  const [, forceUpdate] = useState(0);
  const [subTab, setSubTab] = useState<'overview' | 'analytics' | 'correlation'>('overview');

  // Python analytics state
  const [analyticsData, setAnalyticsData] = useState<{
    portfolioMetrics: any | null;
    performanceAnalysis: any | null;
    ffnPerformance: any | null;
    ffnMonthly: any | null;
  }>({ portfolioMetrics: null, performanceAnalysis: null, ffnPerformance: null, ffnMonthly: null });
  const [analyticsLoading, setAnalyticsLoading] = useState(false);

  const [correlationData, setCorrelationData] = useState<{
    covariance: any | null;
    comparison: any | null;
  }>({ covariance: null, comparison: null });
  const [correlationLoading, setCorrelationLoading] = useState(false);

  // Fetch real historical close prices from yfinance for FFN commands
  const fetchHistoricalPrices = useCallback(async (symbols: string[]): Promise<Record<string, Record<string, number>>> => {
    const endDate = new Date().toISOString().split('T')[0];
    const startDate = new Date(Date.now() - 365 * 24 * 60 * 60 * 1000).toISOString().split('T')[0];
    const priceMap: Record<string, Record<string, number>> = {};
    const fetches = symbols.map(async (sym) => {
      const raw = await invoke<string>('execute_yfinance_command', {
        command: 'historical', args: [sym, startDate, endDate, '1d'],
      });
      const parsed = JSON.parse(raw);
      if (Array.isArray(parsed) && parsed.length > 0) {
        const datePrices: Record<string, number> = {};
        for (const bar of parsed) {
          const d = new Date(bar.timestamp * 1000).toISOString().split('T')[0];
          datePrices[d] = bar.close;
        }
        priceMap[sym] = datePrices;
      }
    });
    await Promise.all(fetches);
    return priceMap;
  }, []);

  const buildReturnsJson = useCallback(() => {
    return JSON.stringify(holdings.map(h => h.symbol).join(','));
  }, [holdings]);

  // ── Cache prefix: per portfolio composition ──────────────────────────────────
  const cachePrefix = useMemo(() => {
    const symbols = holdings.map(h => h.symbol).sort().join(',');
    const weights = holdings.map(h => Math.round(h.weight * 10)).join(',');
    return `analytics:${symbols}:${weights}`;
  }, [holdings]);

  // ── Restore cached results on mount ──────────────────────────────────────────
  useEffect(() => {
    let cancelled = false;
    const restore = async () => {
      const [cachedAnalytics, cachedCorrelation] = await Promise.all([
        cacheService.get<typeof analyticsData>(`${cachePrefix}:analytics`),
        cacheService.get<typeof correlationData>(`${cachePrefix}:correlation`),
      ]);
      if (cancelled) return;
      if (cachedAnalytics) setAnalyticsData(cachedAnalytics.data);
      if (cachedCorrelation) setCorrelationData(cachedCorrelation.data);
    };
    restore();
    return () => { cancelled = true; };
  }, [cachePrefix]);  

  // Fetch analytics data
  const fetchAnalytics = useCallback(async () => {
    if (holdings.length === 0) return;
    setAnalyticsLoading(true);
    const returnsData = buildReturnsJson();

    // Fetch real historical prices for FFN commands
    const symbols = holdings.map(h => h.symbol);
    const priceMap = await fetchHistoricalPrices(symbols);
    const pricesJson = JSON.stringify(priceMap);

    const [metrics, perf, ffnPerf, ffnMonth] = await Promise.allSettled([
      invoke<string>('calculate_portfolio_metrics', { returnsData }),
      invoke<string>('analyze_portfolio_performance', { portfolioData: returnsData }),
      invoke<string>('ffn_calculate_performance', { pricesJson }),
      invoke<string>('ffn_monthly_returns', { pricesJson }),
    ]);

    const newData = {
      portfolioMetrics: metrics.status === 'fulfilled' ? safeParse(metrics.value) : null,
      performanceAnalysis: perf.status === 'fulfilled' ? safeParse(perf.value) : null,
      ffnPerformance: ffnPerf.status === 'fulfilled' ? safeParse(ffnPerf.value) : null,
      ffnMonthly: ffnMonth.status === 'fulfilled' ? safeParse(ffnMonth.value) : null,
    };
    setAnalyticsData(newData);
    cacheService.set(`${cachePrefix}:analytics`, newData, 'api-response', '1h');
    setAnalyticsLoading(false);
  }, [holdings, fetchHistoricalPrices, buildReturnsJson, cachePrefix]);

  // Fetch correlation data
  const fetchCorrelation = useCallback(async () => {
    if (holdings.length === 0) return;
    setCorrelationLoading(true);
    const returnsJson = buildReturnsJson();

    // Fetch real historical prices for FFN commands
    const symbols = holdings.map(h => h.symbol);
    const priceMap = await fetchHistoricalPrices(symbols);
    const pricesJson = JSON.stringify(priceMap);

    const [cov, compare] = await Promise.allSettled([
      invoke<string>('fortitudo_covariance_analysis', { returnsJson }),
      invoke<string>('ffn_compare_assets', { pricesJson }),
    ]);

    const newData = {
      covariance: cov.status === 'fulfilled' ? safeParse(cov.value) : null,
      comparison: compare.status === 'fulfilled' ? safeParse(compare.value) : null,
    };
    setCorrelationData(newData);
    cacheService.set(`${cachePrefix}:correlation`, newData, 'api-response', '1h');
    setCorrelationLoading(false);
  }, [holdings, buildReturnsJson, fetchHistoricalPrices, cachePrefix]);

  // Auto-fetch when sub-tab activates (only if not already loaded from cache)
  useEffect(() => {
    if (subTab === 'analytics' && !analyticsData.portfolioMetrics && !analyticsLoading) fetchAnalytics();
    if (subTab === 'correlation' && !correlationData.covariance && !correlationLoading) fetchCorrelation();
  }, [subTab, analyticsData.portfolioMetrics, analyticsLoading, fetchAnalytics, correlationData.covariance, correlationLoading, fetchCorrelation]);

  // Prefetch sectors
  useEffect(() => {
    const symbols = holdings.map(h => h.symbol);
    if (symbols.length > 0) {
      setSectorsLoaded(false);
      sectorService.prefetchSectors(symbols).then(() => {
        setSectorsLoaded(true);
        forceUpdate(n => n + 1);
      });
    }
  }, [holdings]);

  // Top gainers/losers
  const topGainers = useMemo(() =>
    [...holdings].sort((a, b) => b.unrealized_pnl_percent - a.unrealized_pnl_percent).slice(0, 5),
    [holdings]);
  const topLosers = useMemo(() =>
    [...holdings].sort((a, b) => a.unrealized_pnl_percent - b.unrealized_pnl_percent).slice(0, 5),
    [holdings]);

  // Sector allocations
  const sectorAllocations = useMemo((): SectorAllocation[] => {
    const map = new Map<string, { value: number; holdings: PortfolioHolding[] }>();
    const total = portfolioSummary.total_market_value;

    holdings.forEach(h => {
      const info = sectorService.getSectorInfo(h.symbol);
      const sector = info.sector;
      const existing = map.get(sector) || { value: 0, holdings: [] };
      map.set(sector, { value: existing.value + h.market_value, holdings: [...existing.holdings, h] });
    });

    return Array.from(map.entries())
      .map(([sector, data], i) => ({
        sector, value: data.value,
        weight: total > 0 ? (data.value / total) * 100 : 0,
        holdings: data.holdings,
        color: SECTOR_COLORS[i % SECTOR_COLORS.length],
      }))
      .sort((a, b) => b.weight - a.weight);
  }, [holdings, portfolioSummary.total_market_value, sectorsLoaded]);

  const pnl = portfolioSummary.total_unrealized_pnl;
  const dayChg = portfolioSummary.total_day_change;
  const gainers = holdings.filter(h => h.unrealized_pnl > 0).length;
  const losers = holdings.filter(h => h.unrealized_pnl < 0).length;

  return (
    <div style={{ height: '100%', display: 'flex', flexDirection: 'column', overflow: 'hidden', backgroundColor: FINCEPT.DARK_BG }}>
      {/* Sub-tab strip */}
      <div style={{ display: 'flex', backgroundColor: '#0A0A0A', borderBottom: `1px solid ${FINCEPT.BORDER}`, flexShrink: 0 }}>
        {([
          { id: 'overview' as const, label: 'OVERVIEW' },
          { id: 'analytics' as const, label: 'ANALYTICS' },
          { id: 'correlation' as const, label: 'CORRELATION' },
        ]).map(t => (
          <button key={t.id} onClick={() => setSubTab(t.id)} style={{
            padding: '5px 12px', fontSize: '8px', fontWeight: 700, letterSpacing: '0.3px',
            backgroundColor: subTab === t.id ? `${FINCEPT.ORANGE}15` : 'transparent',
            borderBottom: subTab === t.id ? `2px solid ${FINCEPT.ORANGE}` : '2px solid transparent',
            color: subTab === t.id ? FINCEPT.ORANGE : t.id !== 'overview' ? FINCEPT.CYAN : FINCEPT.GRAY,
            border: 'none', cursor: 'pointer',
            display: 'flex', alignItems: 'center', gap: '3px',
          }}>
            {t.id !== 'overview' && <Zap size={7} />}
            {t.label}
          </button>
        ))}
      </div>

      <div style={{ flex: 1, overflow: 'auto', padding: '8px' }}>

      {subTab === 'overview' ? (
      <>
      {/* ═══ METRICS ROW ═══ */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '6px', marginBottom: '10px' }}>
        <MetricCard label="NAV" value={formatCurrency(portfolioSummary.total_market_value, currency)} color={FINCEPT.YELLOW} />
        <MetricCard label="UNREALIZED P&L" value={`${pnl >= 0 ? '+' : ''}${formatCurrency(pnl, currency)}`} color={valColor(pnl)} sub={formatPercent(portfolioSummary.total_unrealized_pnl_percent)} />
        <MetricCard label="DAY CHANGE" value={`${dayChg >= 0 ? '+' : ''}${formatCurrency(dayChg, currency)}`} color={valColor(dayChg)} sub={formatPercent(portfolioSummary.total_day_change_percent)} />
        <MetricCard label="COST BASIS" value={formatCurrency(portfolioSummary.total_cost_basis, currency)} color={FINCEPT.WHITE} />
        <MetricCard label="POSITIONS" value={`${portfolioSummary.total_positions}`} color={FINCEPT.WHITE} sub={`${gainers}\u25B2 ${losers}\u25BC`} />
      </div>

      {/* ═══ GAINERS / LOSERS ═══ */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px', marginBottom: '10px' }}>
        {/* Gainers */}
        <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px' }}>
          <div style={{ padding: '5px 8px', borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', gap: '4px' }}>
            <TrendingUp size={9} color={FINCEPT.GREEN} />
            <span style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.GREEN, letterSpacing: '0.5px' }}>TOP GAINERS</span>
          </div>
          {topGainers.map(h => (
            <div key={h.symbol} style={{ display: 'flex', justifyContent: 'space-between', padding: '3px 8px', borderBottom: `1px solid ${FINCEPT.BORDER}20` }}>
              <span style={{ fontSize: '9px', color: FINCEPT.CYAN, fontWeight: 700 }}>{h.symbol}</span>
              <span style={{ fontSize: '9px', color: FINCEPT.GREEN, fontWeight: 700 }}>{formatPercent(h.unrealized_pnl_percent)}</span>
            </div>
          ))}
        </div>
        {/* Losers */}
        <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px' }}>
          <div style={{ padding: '5px 8px', borderBottom: `1px solid ${FINCEPT.BORDER}`, display: 'flex', alignItems: 'center', gap: '4px' }}>
            <TrendingDown size={9} color={FINCEPT.RED} />
            <span style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.RED, letterSpacing: '0.5px' }}>TOP LOSERS</span>
          </div>
          {topLosers.map(h => (
            <div key={h.symbol} style={{ display: 'flex', justifyContent: 'space-between', padding: '3px 8px', borderBottom: `1px solid ${FINCEPT.BORDER}20` }}>
              <span style={{ fontSize: '9px', color: FINCEPT.CYAN, fontWeight: 700 }}>{h.symbol}</span>
              <span style={{ fontSize: '9px', color: FINCEPT.RED, fontWeight: 700 }}>{formatPercent(h.unrealized_pnl_percent)}</span>
            </div>
          ))}
        </div>
      </div>

      {/* ═══ SECTOR BREAKDOWN ═══ */}
      <div style={{ backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, borderRadius: '2px', padding: '8px' }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '4px', marginBottom: '8px' }}>
          <PieChart size={10} color={FINCEPT.ORANGE} />
          <span style={{ fontSize: '9px', fontWeight: 700, color: FINCEPT.ORANGE, letterSpacing: '0.5px' }}>SECTOR ALLOCATION</span>
          {!sectorsLoaded && holdings.length > 0 && (
            <span style={{ fontSize: '8px', color: FINCEPT.CYAN, animation: 'pulse 1.5s infinite' }}>Loading...</span>
          )}
        </div>

        {sectorAllocations.length === 0 ? (
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px', padding: '12px', textAlign: 'center' }}>No sector data</div>
        ) : (
          <div style={{ display: 'flex', gap: '16px' }}>
            {/* Pie Chart */}
            <div style={{ flexShrink: 0 }}>
              <SectorPie allocations={sectorAllocations} />
              {/* Legend */}
              <div style={{ marginTop: '4px' }}>
                {sectorAllocations.map(s => (
                  <div key={s.sector} style={{ display: 'flex', alignItems: 'center', gap: '4px', marginBottom: '2px' }}>
                    <div style={{ width: '8px', height: '8px', backgroundColor: s.color, flexShrink: 0 }} />
                    <span style={{ fontSize: '8px', color: FINCEPT.WHITE, flex: 1 }}>{s.sector}</span>
                    <span style={{ fontSize: '8px', color: FINCEPT.CYAN, fontWeight: 700 }}>{s.weight.toFixed(1)}%</span>
                  </div>
                ))}
              </div>
            </div>

            {/* Sector Table */}
            <div style={{ flex: 1, overflow: 'auto' }}>
              {/* Header */}
              <div style={{ display: 'grid', gridTemplateColumns: '1.5fr 1fr 0.8fr 0.5fr', gap: '6px', padding: '4px 6px', backgroundColor: FINCEPT.HEADER_BG, borderBottom: `1px solid ${FINCEPT.ORANGE}`, marginBottom: '2px' }}>
                <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.ORANGE, letterSpacing: '0.3px' }}>SECTOR</div>
                <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.ORANGE, textAlign: 'right' }}>VALUE</div>
                <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.ORANGE, textAlign: 'right' }}>WEIGHT</div>
                <div style={{ fontSize: '8px', fontWeight: 700, color: FINCEPT.ORANGE, textAlign: 'right' }}>#</div>
              </div>

              {sectorAllocations.map((s, i) => (
                <React.Fragment key={s.sector}>
                  <div style={{
                    display: 'grid', gridTemplateColumns: '1.5fr 1fr 0.8fr 0.5fr', gap: '6px',
                    padding: '4px 6px', borderLeft: `3px solid ${s.color}`,
                    backgroundColor: i % 2 === 0 ? 'rgba(255,255,255,0.02)' : 'transparent',
                  }}>
                    <div style={{ fontSize: '9px', fontWeight: 700, color: s.color }}>{s.sector}</div>
                    <div style={{ fontSize: '9px', color: FINCEPT.CYAN, textAlign: 'right', fontWeight: 700 }}>{formatCurrency(s.value, currency)}</div>
                    <div style={{ fontSize: '9px', color: FINCEPT.WHITE, textAlign: 'right' }}>{s.weight.toFixed(1)}%</div>
                    <div style={{ fontSize: '9px', color: FINCEPT.GRAY, textAlign: 'right' }}>{s.holdings.length}</div>
                  </div>
                  {/* Holdings within sector */}
                  {s.holdings.map(h => (
                    <div key={h.id} style={{
                      display: 'grid', gridTemplateColumns: '1.5fr 1fr 0.8fr 0.5fr', gap: '6px',
                      padding: '2px 6px 2px 16px', borderLeft: `1px solid ${s.color}`, marginLeft: '3px',
                    }}>
                      <div style={{ fontSize: '8px', color: FINCEPT.CYAN }}>{'\u2514'} {h.symbol}</div>
                      <div style={{ fontSize: '8px', color: FINCEPT.WHITE, textAlign: 'right' }}>{formatCurrency(h.market_value, currency)}</div>
                      <div style={{ fontSize: '8px', color: FINCEPT.GRAY, textAlign: 'right' }}>{h.weight.toFixed(1)}%</div>
                      <div style={{ fontSize: '8px', color: valColor(h.unrealized_pnl_percent), textAlign: 'right', fontWeight: 600 }}>
                        {formatPercent(h.unrealized_pnl_percent)}
                      </div>
                    </div>
                  ))}
                </React.Fragment>
              ))}

              {/* Diversification Metrics */}
              <div style={{ marginTop: '8px', padding: '6px', backgroundColor: FINCEPT.HEADER_BG, borderTop: `1px solid ${FINCEPT.ORANGE}`, display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '8px' }}>
                <div>
                  <div style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 700, letterSpacing: '0.3px' }}>SECTORS</div>
                  <div style={{ fontSize: '11px', color: FINCEPT.CYAN, fontWeight: 700 }}>{sectorAllocations.length}</div>
                </div>
                <div>
                  <div style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 700, letterSpacing: '0.3px' }}>LARGEST</div>
                  <div style={{ fontSize: '11px', color: FINCEPT.YELLOW, fontWeight: 700 }}>
                    {sectorAllocations[0]?.weight.toFixed(1)}%
                  </div>
                </div>
                <div>
                  <div style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 700, letterSpacing: '0.3px' }}>CONCENTRATION</div>
                  <div style={{
                    fontSize: '11px', fontWeight: 700,
                    color: (sectorAllocations[0]?.weight ?? 0) > 40 ? FINCEPT.RED : (sectorAllocations[0]?.weight ?? 0) > 25 ? FINCEPT.YELLOW : FINCEPT.GREEN,
                  }}>
                    {(sectorAllocations[0]?.weight ?? 0) > 40 ? 'HIGH' : (sectorAllocations[0]?.weight ?? 0) > 25 ? 'MEDIUM' : 'LOW'}
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}
      </div>
      </>
      ) : subTab === 'analytics' ? (
        /* ═══ ANALYTICS — Python-powered ═══ */
        <>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
            <span style={{ fontSize: '9px', fontWeight: 800, color: FINCEPT.CYAN, letterSpacing: '0.5px' }}>PYTHON ANALYTICS</span>
            <button onClick={fetchAnalytics} disabled={analyticsLoading} style={{
              padding: '3px 10px', fontSize: '8px', fontWeight: 700,
              backgroundColor: `${FINCEPT.CYAN}15`, border: `1px solid ${FINCEPT.CYAN}40`,
              color: FINCEPT.CYAN, cursor: analyticsLoading ? 'wait' : 'pointer',
              display: 'flex', alignItems: 'center', gap: '4px',
            }}>
              {analyticsLoading ? <Loader size={8} className="animate-spin" /> : <Zap size={8} />}
              {analyticsLoading ? 'ANALYZING...' : 'RUN ANALYSIS'}
            </button>
          </div>

          {analyticsLoading && !analyticsData.portfolioMetrics ? (
            <div style={{ ...COMMON_STYLES.emptyState, height: '200px' }}>
              <Loader size={20} color={FINCEPT.CYAN} className="animate-spin" style={{ marginBottom: '8px' }} />
              <div style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>Running portfolio analytics...</div>
            </div>
          ) : (
            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
              {renderAnalyticsSection(analyticsData.portfolioMetrics, 'PORTFOLIO METRICS', 'portfolio_analytics.py')}
              {renderAnalyticsSection(analyticsData.performanceAnalysis, 'PERFORMANCE ANALYSIS', 'portfolio_analytics.py')}
              {renderAnalyticsSection(analyticsData.ffnPerformance, 'FFN PERFORMANCE', 'ffn_service.py')}
              {renderAnalyticsSection(analyticsData.ffnMonthly, 'FFN MONTHLY RETURNS', 'ffn_service.py')}
            </div>
          )}
        </>
      ) : (
        /* ═══ CORRELATION — Python-powered ═══ */
        <>
          <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '8px' }}>
            <span style={{ fontSize: '9px', fontWeight: 800, color: FINCEPT.CYAN, letterSpacing: '0.5px' }}>CORRELATION & COVARIANCE</span>
            <button onClick={fetchCorrelation} disabled={correlationLoading} style={{
              padding: '3px 10px', fontSize: '8px', fontWeight: 700,
              backgroundColor: `${FINCEPT.CYAN}15`, border: `1px solid ${FINCEPT.CYAN}40`,
              color: FINCEPT.CYAN, cursor: correlationLoading ? 'wait' : 'pointer',
              display: 'flex', alignItems: 'center', gap: '4px',
            }}>
              {correlationLoading ? <Loader size={8} className="animate-spin" /> : <Zap size={8} />}
              {correlationLoading ? 'ANALYZING...' : 'RUN ANALYSIS'}
            </button>
          </div>

          {correlationLoading && !correlationData.covariance ? (
            <div style={{ ...COMMON_STYLES.emptyState, height: '200px' }}>
              <Loader size={20} color={FINCEPT.CYAN} className="animate-spin" style={{ marginBottom: '8px' }} />
              <div style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>Computing correlation matrix...</div>
            </div>
          ) : (
            <div>
              {/* Correlation matrix */}
              <CorrelationMatrix data={correlationData.covariance} holdings={holdings} />
              {/* FFN comparison */}
              {correlationData.comparison && (
                <div style={{ marginTop: '10px' }}>
                  {renderAnalyticsSection(correlationData.comparison, 'ASSET COMPARISON', 'ffn_service.py')}
                </div>
              )}
            </div>
          )}
        </>
      )}
      </div>
    </div>
  );
};

// ─── Helper: safe JSON parse ───
function safeParse(s: string): any {
  try { return JSON.parse(s); } catch { return null; }
}

// ─── Render analytics section ───
function renderAnalyticsSection(data: any | null, title: string, source: string): React.ReactNode {
  return (
    <div style={{ padding: '8px', backgroundColor: FINCEPT.PANEL_BG, border: BORDERS.STANDARD, marginBottom: '6px' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center', marginBottom: '4px' }}>
        <span style={{ fontSize: '9px', fontWeight: 800, color: FINCEPT.ORANGE, letterSpacing: '0.5px' }}>{title}</span>
        <span style={{ fontSize: '7px', color: FINCEPT.MUTED }}>via {source}</span>
      </div>
      {!data ? (
        <div style={{ fontSize: '9px', color: FINCEPT.MUTED }}>No data — run analysis to populate</div>
      ) : (
        Object.entries(data).filter(([, v]) => typeof v === 'number' || typeof v === 'string').map(([key, val]) => (
          <div key={key} style={{ display: 'flex', justifyContent: 'space-between', padding: '2px 0', borderBottom: `1px solid ${FINCEPT.BORDER}20` }}>
            <span style={{ fontSize: '9px', color: FINCEPT.GRAY, fontWeight: 600 }}>{key.replace(/_/g, ' ').toUpperCase()}</span>
            <span style={{ fontSize: '9px', color: typeof val === 'number' ? valColor(val as number) : FINCEPT.WHITE, fontWeight: 700 }}>
              {typeof val === 'number' ? (Math.abs(val as number) < 1 ? ((val as number) * 100).toFixed(2) + '%' : (val as number).toFixed(4)) : String(val)}
            </span>
          </div>
        ))
      )}
    </div>
  );
}

// ─── Correlation Matrix Component ───
const CorrelationMatrix: React.FC<{ data: any | null; holdings: PortfolioHolding[] }> = ({ data, holdings }) => {
  if (!data) return (
    <div style={{ padding: '12px', textAlign: 'center', color: FINCEPT.MUTED, fontSize: '10px' }}>
      Run analysis to see correlation matrix
    </div>
  );

  const corrMatrix = data.correlation_matrix || data.correlation || data;
  const symbols = data.symbols || data.assets || holdings.map(h => h.symbol);

  if (!Array.isArray(corrMatrix) || corrMatrix.length === 0) {
    return <div>{renderAnalyticsSection(data, 'COVARIANCE DATA', 'fortitudo_service.py')}</div>;
  }

  return (
    <div>
      <div style={{ fontSize: '9px', fontWeight: 800, color: FINCEPT.ORANGE, letterSpacing: '0.5px', marginBottom: '6px' }}>CORRELATION MATRIX</div>
      <div style={{ overflow: 'auto' }}>
        <table style={{ borderCollapse: 'collapse' }}>
          <thead>
            <tr>
              <th style={{ padding: '4px 8px', fontSize: '8px', color: FINCEPT.ORANGE }}></th>
              {symbols.map((s: string) => (
                <th key={s} style={{ padding: '4px 6px', fontSize: '8px', color: FINCEPT.CYAN, fontWeight: 700, textAlign: 'center' }}>{s}</th>
              ))}
            </tr>
          </thead>
          <tbody>
            {corrMatrix.map((row: number[], i: number) => (
              <tr key={i}>
                <td style={{ padding: '4px 8px', fontSize: '8px', color: FINCEPT.CYAN, fontWeight: 700 }}>{symbols[i] || i}</td>
                {(Array.isArray(row) ? row : []).map((val: number, j: number) => {
                  const absVal = Math.abs(val);
                  const bg = val >= 0
                    ? `rgba(0,214,111,${absVal * 0.3})`
                    : `rgba(255,59,59,${absVal * 0.3})`;
                  return (
                    <td key={j} style={{
                      padding: '4px 6px', fontSize: '8px', fontWeight: 600,
                      textAlign: 'center', backgroundColor: bg,
                      color: i === j ? FINCEPT.YELLOW : FINCEPT.WHITE,
                    }}>
                      {typeof val === 'number' ? val.toFixed(2) : '--'}
                    </td>
                  );
                })}
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );
};

export default AnalyticsSectorsPanel;
