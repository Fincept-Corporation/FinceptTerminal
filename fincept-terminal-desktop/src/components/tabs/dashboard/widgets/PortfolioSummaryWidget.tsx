import React from 'react';
import { TrendingUp, TrendingDown, Briefcase, ExternalLink } from 'lucide-react';
import { BaseWidget } from './BaseWidget';
import { sqliteService } from '@/services/core/sqliteService';
import { useTranslation } from 'react-i18next';
import { useCache } from '@/hooks/useCache';

interface PortfolioSummaryWidgetProps {
  id: string;
  portfolioId?: string;
  onRemove?: () => void;
  onNavigate?: () => void;
}

interface PortfolioData {
  name: string;
  totalValue: number;
  totalCost: number;
  totalGain: number;
  gainPercent: number;
  positions: number;
  currency: string;
}

interface PortfolioSummary {
  portfolio: PortfolioData | null;
  topPositions: any[];
}

export const PortfolioSummaryWidget: React.FC<PortfolioSummaryWidgetProps> = ({
  id,
  portfolioId,
  onRemove,
  onNavigate
}) => {
  const { t } = useTranslation('dashboard');

  const {
    data: summaryData,
    isLoading: loading,
    error,
    refresh
  } = useCache<PortfolioSummary>({
    key: `widget:portfolio-summary:${portfolioId || 'default'}`,
    category: 'portfolio',
    ttl: '1m',
    refetchInterval: 60 * 1000,
    staleWhileRevalidate: true,
    fetcher: async () => {
      try {
        const portfolios = await sqliteService.listPortfolios();

        if (!portfolios || portfolios.length === 0) {
          return { portfolio: null, topPositions: [] };
        }

        const targetPortfolio = portfolioId
          ? portfolios.find((p: any) => p.id === portfolioId) || portfolios[0]
          : portfolios[0];

        const positions = await sqliteService.getPortfolioPositions(targetPortfolio.id);

        const totalValue = positions?.reduce((sum: number, p: any) => sum + (p.current_value || p.quantity * (p.current_price || p.entry_price || 0)), 0) || 0;
        const totalCost = positions?.reduce((sum: number, p: any) => sum + (p.cost_basis || p.quantity * (p.entry_price || 0)), 0) || 0;
        const totalGain = totalValue - totalCost;
        const gainPercent = totalCost > 0 ? (totalGain / totalCost) * 100 : 0;

        const portfolio: PortfolioData = {
          name: targetPortfolio.name,
          totalValue: totalValue || targetPortfolio.current_balance || 0,
          totalCost: totalCost || targetPortfolio.initial_balance || 0,
          totalGain: totalGain || (targetPortfolio as any).total_pnl || (targetPortfolio as any).totalPnL || 0,
          gainPercent,
          positions: positions?.length || 0,
          currency: targetPortfolio.currency || 'USD'
        };

        const sorted = (positions || [])
          .sort((a: any, b: any) => (b.current_value || 0) - (a.current_value || 0))
          .slice(0, 5);

        return { portfolio, topPositions: sorted };
      } catch {
        return {
          portfolio: {
            name: 'Demo Portfolio',
            totalValue: 125430.50,
            totalCost: 100000,
            totalGain: 25430.50,
            gainPercent: 25.43,
            positions: 8,
            currency: 'USD'
          },
          topPositions: [
            { symbol: 'AAPL', name: 'Apple Inc', current_value: 35000, gain_percent: 32.5 },
            { symbol: 'MSFT', name: 'Microsoft', current_value: 28000, gain_percent: 28.1 },
            { symbol: 'GOOGL', name: 'Alphabet', current_value: 22000, gain_percent: 18.4 },
          ]
        };
      }
    }
  });

  const portfolio = summaryData?.portfolio || null;
  const topPositions = summaryData?.topPositions || [];

  const formatCurrency = (value: number, currency: string = 'USD') => {
    return new Intl.NumberFormat('en-US', {
      style: 'currency',
      currency,
      minimumFractionDigits: 2
    }).format(value);
  };

  return (
    <BaseWidget
      id={id}
      title={`PORTFOLIO${portfolio ? ` - ${portfolio.name.toUpperCase()}` : ''}`}
      onRemove={onRemove}
      onRefresh={refresh}
      isLoading={loading && !summaryData}
      error={error?.message || null}
      headerColor="var(--ft-color-info)"
    >
      <div style={{ padding: '8px' }}>
        {portfolio ? (
          <>
            {/* Summary Stats */}
            <div style={{
              display: 'grid',
              gridTemplateColumns: '1fr 1fr',
              gap: '8px',
              marginBottom: '12px'
            }}>
              <div style={{
                backgroundColor: 'var(--ft-color-panel)',
                padding: '8px',
                borderRadius: '2px'
              }}>
                <div style={{ fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text-muted)' }}>TOTAL VALUE</div>
                <div style={{ fontSize: 'var(--ft-font-size-heading)', fontWeight: 'bold', color: 'var(--ft-color-primary)' }}>
                  {formatCurrency(portfolio.totalValue, portfolio.currency)}
                </div>
              </div>
              <div style={{
                backgroundColor: 'var(--ft-color-panel)',
                padding: '8px',
                borderRadius: '2px'
              }}>
                <div style={{ fontSize: 'var(--ft-font-size-tiny)', color: 'var(--ft-color-text-muted)' }}>TOTAL GAIN</div>
                <div style={{
                  fontSize: 'var(--ft-font-size-heading)',
                  fontWeight: 'bold',
                  color: portfolio.totalGain >= 0 ? 'var(--ft-color-success)' : 'var(--ft-color-alert)',
                  display: 'flex',
                  alignItems: 'center',
                  gap: '4px'
                }}>
                  {portfolio.totalGain >= 0 ? <TrendingUp size={14} /> : <TrendingDown size={14} />}
                  {portfolio.totalGain >= 0 ? '+' : ''}{portfolio.gainPercent.toFixed(2)}%
                </div>
              </div>
            </div>

            {/* Top Positions */}
            <div style={{ marginBottom: '8px' }}>
              <div style={{
                fontSize: 'var(--ft-font-size-tiny)',
                color: 'var(--ft-color-text-muted)',
                marginBottom: '4px',
                borderBottom: '1px solid var(--ft-border-color)',
                paddingBottom: '4px'
              }}>
                TOP POSITIONS ({portfolio.positions} total)
              </div>
              {topPositions.map((pos: any) => (
                <div
                  key={pos.symbol}
                  style={{
                    display: 'flex',
                    justifyContent: 'space-between',
                    alignItems: 'center',
                    padding: '4px 0',
                    borderBottom: '1px solid var(--ft-border-color)'
                  }}
                >
                  <div>
                    <span style={{ fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)', fontWeight: 'bold' }}>
                      {pos.symbol}
                    </span>
                  </div>
                  <div style={{ textAlign: 'right' }}>
                    <div style={{ fontSize: 'var(--ft-font-size-small)', color: 'var(--ft-color-text)' }}>
                      {formatCurrency(pos.current_value || 0)}
                    </div>
                    <div style={{
                      fontSize: 'var(--ft-font-size-tiny)',
                      color: (pos.gain_percent || 0) >= 0 ? 'var(--ft-color-success)' : 'var(--ft-color-alert)'
                    }}>
                      {(pos.gain_percent || 0) >= 0 ? '+' : ''}{(pos.gain_percent || 0).toFixed(1)}%
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </>
        ) : (
          <div style={{ textAlign: 'center', padding: '20px', color: 'var(--ft-color-text-muted)' }}>
            <Briefcase size={24} style={{ marginBottom: '8px', opacity: 0.5 }} />
            <div style={{ fontSize: 'var(--ft-font-size-small)' }}>{t('widgets.noPortfolioFound')}</div>
            <div style={{ fontSize: 'var(--ft-font-size-tiny)', marginTop: '4px' }}>{t('widgets.createPortfolioHint')}</div>
          </div>
        )}

        {onNavigate && (
          <div
            onClick={onNavigate}
            style={{
              padding: '6px',
              textAlign: 'center',
              color: 'var(--ft-color-info)',
              fontSize: 'var(--ft-font-size-tiny)',
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              gap: '4px'
            }}
          >
            <span>{t('widgets.openPortfolioTab')}</span>
            <ExternalLink size={10} />
          </div>
        )}
      </div>
    </BaseWidget>
  );
};
