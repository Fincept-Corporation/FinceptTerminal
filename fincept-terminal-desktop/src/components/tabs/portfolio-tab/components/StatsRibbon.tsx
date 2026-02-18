import React, { useState, useEffect } from 'react';
import { FINCEPT } from '../finceptStyles';
import { valColor } from './helpers';
import { formatCurrency, formatPercent } from '../portfolio/utils';
import { useTranslation } from 'react-i18next';
import type { PortfolioSummary } from '../../../../services/portfolio/portfolioService';
import type { ComputedMetrics } from '../types';
import { cacheService } from '../../../../services/cache/cacheService';
import { Database } from 'lucide-react';

interface StatsRibbonProps {
  portfolioSummary: PortfolioSummary | null;
  metrics: ComputedMetrics;
  currency: string;
}

const StatsRibbon: React.FC<StatsRibbonProps> = ({ portfolioSummary, metrics, currency }) => {
  const { t } = useTranslation('portfolio');
  const [cacheStats, setCacheStats] = useState<{ entries: number; sizeMB: number } | null>(null);

  useEffect(() => {
    const updateCacheStats = async () => {
      const stats = await cacheService.getStats();
      if (stats) {
        setCacheStats({
          entries: stats.total_entries,
          sizeMB: parseFloat((stats.total_size_bytes / (1024 * 1024)).toFixed(2))
        });
      }
    };
    updateCacheStats();
    const interval = setInterval(updateCacheStats, 5000);
    return () => clearInterval(interval);
  }, []);

  const s = portfolioSummary;
  if (!s) return null;

  const pnl = s.total_unrealized_pnl;
  const pnlPct = s.total_unrealized_pnl_percent;
  const dayChg = s.total_day_change;
  const dayChgPct = s.total_day_change_percent;
  const gainers = s.holdings.filter(h => h.unrealized_pnl > 0).length;
  const losers = s.holdings.filter(h => h.unrealized_pnl < 0).length;

  const cells = [
    { label: t('ribbon.totalValue', 'TOTAL VALUE'), value: formatCurrency(s.total_market_value, currency), sub: currency, color: FINCEPT.YELLOW },
    { label: t('ribbon.unrealizedPnl', 'UNREALIZED P&L'), value: `${pnl >= 0 ? '+' : ''}${formatCurrency(pnl, currency)}`, sub: formatPercent(pnlPct), color: valColor(pnl) },
    { label: t('ribbon.dayChange', 'DAY CHANGE'), value: `${dayChg >= 0 ? '+' : ''}${formatCurrency(dayChg, currency)}`, sub: formatPercent(dayChgPct), color: valColor(dayChg) },
    { label: t('ribbon.costBasis', 'COST BASIS'), value: formatCurrency(s.total_cost_basis, currency), sub: t('ribbon.fifo', 'FIFO'), color: FINCEPT.CYAN },
    { label: t('ribbon.positions', 'POSITIONS'), value: `${s.total_positions}`, sub: `${gainers}▲ ${losers}▼`, color: FINCEPT.WHITE },
    { label: t('ribbon.concTop3', 'CONC. TOP3'), value: metrics.concentrationTop3 !== null ? `${metrics.concentrationTop3}%` : '--', sub: t('ribbon.weight', 'WEIGHT'), color: FINCEPT.ORANGE },
    { label: t('ribbon.sharpe', 'SHARPE'), value: metrics.sharpe !== null ? metrics.sharpe.toFixed(2) : '--', sub: t('ribbon.annualized', 'ANNUALIZED'), color: FINCEPT.CYAN },
    { label: t('ribbon.beta', 'BETA'), value: metrics.beta !== null ? metrics.beta.toFixed(2) : '--', sub: t('ribbon.vsSpy', 'VS SPY'), color: FINCEPT.YELLOW },
    { label: t('ribbon.volatility', 'VOLATILITY'), value: metrics.volatility !== null ? `${metrics.volatility}%` : '--', sub: t('ribbon.ann30d', '30D ANN.'), color: FINCEPT.ORANGE },
    { label: t('ribbon.maxDrawdown', 'MAX DRAWDOWN'), value: metrics.maxDrawdown !== null ? `${metrics.maxDrawdown.toFixed(1)}%` : '--', sub: t('ribbon.sinceIncep', 'SINCE INCEP.'), color: FINCEPT.RED },
    { label: t('ribbon.riskScore', 'RISK SCORE'), value: metrics.riskScore !== null ? `${metrics.riskScore}/100` : '--', sub: t('ribbon.composite', 'COMPOSITE'), color: metrics.riskScore !== null && metrics.riskScore > 60 ? FINCEPT.RED : FINCEPT.GREEN },
    { label: t('ribbon.var95', 'VAR (95%)'), value: metrics.var95 !== null ? formatCurrency(metrics.var95, currency) : '--', sub: t('ribbon.oneDay', '1-DAY'), color: FINCEPT.RED },
    { label: 'CACHE', value: cacheStats ? `${cacheStats.entries}` : '--', sub: cacheStats ? `${cacheStats.sizeMB}MB` : '--', color: FINCEPT.CYAN, icon: Database },
  ];

  return (
    <div id="portfolio-summary" style={{
      height: '52px', flexShrink: 0,
      backgroundColor: '#0A0A0A',
      borderBottom: `1px solid ${FINCEPT.BORDER}`,
      display: 'flex', alignItems: 'stretch',
      overflow: 'hidden',
    }}>
      {cells.map((m, i) => (
        <div key={i} style={{
          flex: 1, minWidth: '90px',
          padding: '6px 8px',
          borderRight: '1px solid #141414',
          display: 'flex', flexDirection: 'column', justifyContent: 'center',
        }}>
          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, fontWeight: 700, letterSpacing: '0.5px', lineHeight: 1, marginBottom: '3px', display: 'flex', alignItems: 'center', gap: '4px' }}>
            {'icon' in m && m.icon && <m.icon size={8} />}
            {m.label}
          </div>
          <div style={{ fontSize: '14px', fontWeight: 800, color: m.color, lineHeight: 1 }}>
            {m.value}
          </div>
          <div style={{ fontSize: '8px', color: FINCEPT.GRAY, marginTop: '2px', lineHeight: 1 }}>
            {m.sub}
          </div>
        </div>
      ))}
    </div>
  );
};

export default StatsRibbon;
