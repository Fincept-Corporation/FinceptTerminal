import React, { useEffect, useRef } from 'react';
import { TrendingUp, TrendingDown, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { portfolioService } from '@/services/portfolio/portfolioService';
import { yfinanceService } from '@/services/markets/yfinanceService';
import { useTranslation } from 'react-i18next';
import { useCache } from '@/hooks/useCache';

interface PerformanceWidgetProps {
  id: string;
  portfolioId?: string;
  onRemove?: () => void;
  onNavigate?: () => void;
  onConfigure?: () => void;
}

interface PerformanceData {
  period: string;
  pnl: number;
  pnlPercent: number;
  holdings: number;
}

interface PerformanceSummary {
  rows: PerformanceData[];
  totalPnL: number;
  totalPnLPercent: number;
  portfolioName: string;
}

function formatDate(d: Date): string {
  return d.toISOString().split('T')[0];
}

/** Find the closest available close price on or before a target date from sorted historical data */
function getPriceAtDate(
  history: { timestamp: number; close: number }[],
  targetDate: Date
): number | null {
  const targetTs = targetDate.getTime() / 1000;
  // history is sorted ascending by timestamp
  let best: number | null = null;
  for (const bar of history) {
    if (bar.timestamp <= targetTs) {
      best = bar.close;
    } else {
      break;
    }
  }
  return best;
}

export const PerformanceWidget: React.FC<PerformanceWidgetProps> = ({
  id,
  portfolioId,
  onRemove,
  onNavigate,
  onConfigure
}) => {
  const { t } = useTranslation('dashboard');

  const {
    data: perfData,
    isLoading: loading,
    isFetching,
    error,
    refresh,
    invalidate
  } = useCache<PerformanceSummary>({
    key: `widget:performance-tracker:${portfolioId || 'default'}`,
    category: 'portfolio',
    ttl: '5m',
    refetchInterval: 5 * 60 * 1000,
    staleWhileRevalidate: true,
    resetOnKeyChange: true,
    fetcher: async () => {
      try {
        await portfolioService.initialize();
        const portfolios = await portfolioService.getPortfolios();

        if (portfolios.length === 0) {
          return { rows: [], totalPnL: 0, totalPnLPercent: 0, portfolioName: '' };
        }

        const portfolio = portfolioId
          ? (portfolios.find(p => p.id === portfolioId) ?? portfolios[0])
          : portfolios[0];

        const summary = await portfolioService.getPortfolioSummary(portfolio.id);

        if (summary.holdings.length === 0) {
          return { rows: [], totalPnL: 0, totalPnLPercent: 0, portfolioName: portfolio.name };
        }

        // Period boundary dates
        const now = new Date();
        const todayStart = new Date(now.getFullYear(), now.getMonth(), now.getDate());
        const weekStart = new Date(now);
        weekStart.setDate(now.getDate() - 7);
        weekStart.setHours(0, 0, 0, 0);
        const monthStart = new Date(now.getFullYear(), now.getMonth() - 1, now.getDate());
        const ytdStart = new Date(now.getFullYear(), 0, 1);
        const yearStart = new Date(now.getFullYear() - 1, now.getMonth(), now.getDate());

        // Fetch 1Y of daily history for all unique symbols in parallel
        const symbols = [...new Set(summary.holdings.map(h => h.symbol))];
        const historyStart = formatDate(yearStart);
        const historyEnd = formatDate(now);

        const historyMap: Record<string, { timestamp: number; close: number }[]> = {};

        await Promise.all(
          symbols.map(async (symbol) => {
            const data = await yfinanceService.getHistoricalData(symbol, historyStart, historyEnd);
            // Sort ascending by timestamp
            historyMap[symbol] = data
              .map(d => ({ timestamp: d.timestamp, close: d.adj_close || d.close }))
              .sort((a, b) => a.timestamp - b.timestamp);
          })
        );

        /**
         * Calculate portfolio P&L for a period:
         * - portfolioValue(date) = Σ holdings[i].quantity * price(symbol, date)
         * - pnl = portfolioValue(now) - portfolioValue(periodStart)
         * - winRate = % of holdings up vs that period start
         */
        const calcPeriod = (startDate: Date) => {
          let pnlSum = 0;
          let holdingsWithData = 0;

          for (const holding of summary.holdings) {
            const history = historyMap[holding.symbol];
            if (!history || history.length === 0) continue;

            const currentPrice = holding.current_price;
            const startPrice = getPriceAtDate(history, startDate);

            if (startPrice === null || startPrice === 0) continue;

            holdingsWithData++;
            pnlSum += (currentPrice - startPrice) * holding.quantity;
          }

          return { pnl: pnlSum, holdings: holdingsWithData };
        };

        // Today: use summary's day change (current vs previous close) — most accurate for intraday
        const todayPnL = summary.total_day_change;

        const weekMetrics  = calcPeriod(weekStart);
        const monthMetrics = calcPeriod(monthStart);
        const ytdMetrics   = calcPeriod(ytdStart);
        const yearMetrics  = calcPeriod(yearStart);

        const totalPnL = summary.total_unrealized_pnl;
        const totalPnLPercent = summary.total_unrealized_pnl_percent;
        const costBasis = summary.total_cost_basis || 1;

        const pnlPct = (pnl: number) => costBasis > 0 ? (pnl / costBasis) * 100 : 0;

        return {
          rows: [
            {
              period: 'Today',
              pnl: todayPnL,
              pnlPercent: pnlPct(todayPnL),
              holdings: summary.holdings.length,
            },
            {
              period: '1 Week',
              pnl: weekMetrics.pnl,
              pnlPercent: pnlPct(weekMetrics.pnl),
              holdings: weekMetrics.holdings,
            },
            {
              period: '1 Month',
              pnl: monthMetrics.pnl,
              pnlPercent: pnlPct(monthMetrics.pnl),
              holdings: monthMetrics.holdings,
            },
            {
              period: 'YTD',
              pnl: ytdMetrics.pnl,
              pnlPercent: pnlPct(ytdMetrics.pnl),
              holdings: ytdMetrics.holdings,
            },
            {
              period: '1 Year',
              pnl: yearMetrics.pnl,
              pnlPercent: pnlPct(yearMetrics.pnl),
              holdings: yearMetrics.holdings,
            },
          ],
          totalPnL,
          totalPnLPercent,
          portfolioName: portfolio.name
        };
      } catch (err) {
        console.error('[PerformanceWidget] Error:', err);
        return { rows: [], totalPnL: 0, totalPnLPercent: 0, portfolioName: '' };
      }
    }
  });

  // When portfolioId changes, invalidate old cache and fetch fresh
  const prevPortfolioId = useRef(portfolioId);
  useEffect(() => {
    if (prevPortfolioId.current !== portfolioId) {
      prevPortfolioId.current = portfolioId;
      invalidate().then(() => refresh());
    }
  }, [portfolioId, invalidate, refresh]);

  const { rows: performance, totalPnL, totalPnLPercent, portfolioName } =
    perfData || { rows: [], totalPnL: 0, totalPnLPercent: 0, portfolioName: '' };

  const formatCurrency = (value: number) => {
    const absValue = Math.abs(value);
    if (absValue >= 1_000_000) return `${value < 0 ? '-' : ''}$${(Math.abs(value) / 1_000_000).toFixed(2)}M`;
    if (absValue >= 1_000)     return `${value < 0 ? '-' : ''}$${(Math.abs(value) / 1_000).toFixed(1)}K`;
    return `${value < 0 ? '-$' : '$'}${Math.abs(value).toFixed(2)}`;
  };

  return (
    <BaseWidget
      id={id}
      title={t('widgets.performanceTracker')}
      onRemove={onRemove}
      onRefresh={refresh}
      onConfigure={onConfigure}
      isLoading={(loading && !perfData) || isFetching}
      error={error?.message || null}
      headerColor={totalPnL >= 0 ? 'var(--ft-color-success)' : 'var(--ft-color-alert)'}
    >
      {performance.length === 0 ? (
        <div style={{
          padding: '24px',
          textAlign: 'center',
          color: 'var(--ft-color-text-muted)',
          fontSize: 'var(--ft-font-size-small)'
        }}>
          <div style={{ marginBottom: '8px' }}>No portfolio data available</div>
          <div style={{ fontSize: 'var(--ft-font-size-tiny)' }}>
            Create a portfolio and add holdings to track performance
          </div>
        </div>
      ) : (
        <div style={{ padding: '4px' }}>
          {/* Total P&L Header */}
          <div style={{
            padding: '8px',
            backgroundColor: 'var(--ft-color-panel)',
            margin: '4px',
            borderRadius: '2px',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center'
          }}>
            <div>
              {portfolioName && (
                <div style={{ fontSize: '8px', color: 'var(--ft-color-text-muted)', marginBottom: '2px', textTransform: 'uppercase', letterSpacing: '0.5px' }}>
                  {portfolioName}
                </div>
              )}
              <div style={{ fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text-muted)' }}>
                {t('widgets.totalPnL')}
              </div>
              <div style={{
                fontSize: 'var(--ft-font-size-heading)',
                fontWeight: 'bold',
                color: totalPnL >= 0 ? 'var(--ft-color-success)' : 'var(--ft-color-alert)',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}>
                {totalPnL >= 0 ? <TrendingUp size={16} /> : <TrendingDown size={16} />}
                {totalPnL >= 0 ? '+' : ''}{formatCurrency(totalPnL)}
              </div>
            </div>
            <div style={{ textAlign: 'right' }}>
              <div style={{ fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text-muted)' }}>
                RETURN
              </div>
              <div style={{
                fontSize: 'var(--ft-font-size-heading)',
                fontWeight: 'bold',
                color: totalPnLPercent >= 0 ? 'var(--ft-color-success)' : 'var(--ft-color-alert)'
              }}>
                {totalPnLPercent >= 0 ? '+' : ''}{totalPnLPercent.toFixed(2)}%
              </div>
            </div>
          </div>

          {/* Header row */}
          <div style={{
            display: 'grid',
            gridTemplateColumns: '60px 1fr 45px',
            gap: '4px',
            padding: '4px 8px',
            borderBottom: '1px solid var(--ft-border-color)',
            color: 'var(--ft-color-text-muted)',
            fontSize: 'var(--ft-font-size-tiny)'
          }}>
            <span>PERIOD</span>
            <span style={{ textAlign: 'right' }}>P&amp;L</span>
            <span style={{ textAlign: 'right' }}>HOLD.</span>
          </div>

          {/* Performance rows */}
          {performance.map((perf) => (
            <div
              key={perf.period}
              style={{
                display: 'grid',
                gridTemplateColumns: '60px 1fr 45px',
                gap: '4px',
                padding: '5px 8px',
                borderBottom: '1px solid var(--ft-border-color)',
                alignItems: 'center'
              }}
            >
              <span style={{ fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)' }}>
                {perf.period}
              </span>
              <div style={{ textAlign: 'right' }}>
                <div style={{
                  fontSize: 'var(--ft-font-size-small)',
                  fontWeight: 'bold',
                  color: perf.pnl >= 0 ? 'var(--ft-color-success)' : 'var(--ft-color-alert)'
                }}>
                  {perf.pnl >= 0 ? '+' : ''}{formatCurrency(perf.pnl)}
                </div>
                <div style={{
                  fontSize: 'var(--ft-font-size-tiny)',
                  color: perf.pnl >= 0 ? 'var(--ft-color-success)' : 'var(--ft-color-alert)'
                }}>
                  {perf.pnl >= 0 ? '+' : ''}{perf.pnlPercent.toFixed(2)}%
                </div>
              </div>
              <span style={{
                textAlign: 'right',
                fontSize: 'var(--ft-font-size-small)',
                color: 'var(--ft-color-text)'
              }}>
                {perf.holdings}
              </span>
            </div>
          ))}

          {onNavigate && (
            <div
              onClick={onNavigate}
              style={{
                padding: '6px',
                textAlign: 'center',
                color: 'var(--ft-color-success)',
                fontSize: 'var(--ft-font-size-tiny)',
                cursor: 'pointer',
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: '4px',
                borderTop: '1px solid var(--ft-border-color)',
                marginTop: '4px'
              }}
            >
              <span>{t('widgets.viewDetailedAnalytics')}</span>
              <ExternalLink size={10} />
            </div>
          )}
        </div>
      )}
    </BaseWidget>
  );
};
