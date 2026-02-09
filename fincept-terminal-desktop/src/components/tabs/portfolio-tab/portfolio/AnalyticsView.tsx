import React, { useState, useEffect } from 'react';
import { useTranslation } from 'react-i18next';
import { useTerminalTheme } from '@/contexts/ThemeContext';
import { PortfolioSummary, portfolioService } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatPercent, formatLargeNumber } from './utils';
import { Activity, Target } from 'lucide-react';

interface AnalyticsViewProps {
  portfolioSummary: PortfolioSummary;
}

const AnalyticsView: React.FC<AnalyticsViewProps> = ({ portfolioSummary }) => {
  const { t } = useTranslation('portfolio');
  const { colors, fontSize, fontFamily } = useTerminalTheme();
  const currency = portfolioSummary.portfolio.currency;

  // Advanced analytics state
  const [advancedMetrics, setAdvancedMetrics] = useState<any>(null);
  const [optimizedPortfolio, setOptimizedPortfolio] = useState<any>(null);
  const [loadingMetrics, setLoadingMetrics] = useState(false);
  const [loadingOptimization, setLoadingOptimization] = useState(false);
  const [analyticsError, setAnalyticsError] = useState<string | null>(null);

  // Hover state tracking
  const [hoveredGainer, setHoveredGainer] = useState<string | null>(null);
  const [hoveredLoser, setHoveredLoser] = useState<string | null>(null);
  const [hoveredPosition, setHoveredPosition] = useState<string | null>(null);
  const [hoveredAllocation, setHoveredAllocation] = useState<string | null>(null);
  const [hoveredOptimize, setHoveredOptimize] = useState(false);

  // Calculate key metrics
  const totalValue = portfolioSummary.total_market_value;
  const totalPnL = portfolioSummary.total_unrealized_pnl;
  const totalPnLPercent = portfolioSummary.total_unrealized_pnl_percent;
  const dayChange = portfolioSummary.total_day_change;
  const dayChangePercent = portfolioSummary.total_day_change_percent;
  const positionsCount = portfolioSummary.total_positions;

  // Find top performers
  const topGainers = [...portfolioSummary.holdings]
    .sort((a, b) => b.unrealized_pnl_percent - a.unrealized_pnl_percent)
    .slice(0, 5);

  const topLosers = [...portfolioSummary.holdings]
    .sort((a, b) => a.unrealized_pnl_percent - b.unrealized_pnl_percent)
    .slice(0, 5);

  // Largest positions by value
  const largestPositions = [...portfolioSummary.holdings]
    .sort((a, b) => b.market_value - a.market_value)
    .slice(0, 5);

  // Load advanced analytics
  useEffect(() => {
    loadAdvancedAnalytics();
  }, [portfolioSummary.portfolio.id]);

  const loadAdvancedAnalytics = async () => {
    setLoadingMetrics(true);
    setAnalyticsError(null);
    try {
      const metrics = await portfolioService.calculateAdvancedMetrics(portfolioSummary.portfolio.id);
      setAdvancedMetrics(metrics);
    } catch (error) {
      console.error('Error loading advanced metrics:', error);
      setAnalyticsError(error instanceof Error ? error.message : 'Failed to load analytics');
    } finally {
      setLoadingMetrics(false);
    }
  };

  const loadOptimization = async () => {
    setLoadingOptimization(true);
    setAnalyticsError(null);
    try {
      const optimized = await portfolioService.optimizePortfolioWeights(portfolioSummary.portfolio.id, 'max_sharpe');
      setOptimizedPortfolio(optimized);
    } catch (error) {
      console.error('Error optimizing portfolio:', error);
      setAnalyticsError(error instanceof Error ? error.message : 'Failed to optimize portfolio');
    } finally {
      setLoadingOptimization(false);
    }
  };

  return (
    <div style={{
      height: '100%',
      backgroundColor: colors.background,
      padding: '12px',
      overflow: 'auto',
      fontFamily
    }}>
      {/* Section Header */}
      <div style={{
        padding: '12px',
        backgroundColor: 'var(--ft-color-header, #1A1A1A)',
        borderBottom: '1px solid var(--ft-color-border, #2A2A2A)',
        color: colors.primary,
        fontSize: fontSize.body,
        fontWeight: 700,
        letterSpacing: '0.5px',
        textTransform: 'uppercase' as const,
        marginBottom: '16px',
      }}>
        {t('analytics.dashboard', 'PORTFOLIO ANALYTICS DASHBOARD')}
      </div>

      {/* Error Message */}
      {analyticsError && (
        <div style={{
          backgroundColor: colors.panel,
          border: `1px solid ${colors.alert}`,
          borderRadius: '2px',
          padding: '12px',
          marginBottom: '16px',
          color: colors.alert,
          fontSize: fontSize.small
        }}>
          [WARN] {analyticsError}
        </div>
      )}

      {/* Advanced Analytics Section */}
      {advancedMetrics && (
        <div style={{
          backgroundColor: colors.panel,
          border: '1px solid var(--ft-color-accent, #00E5FF)',
          borderRadius: '2px',
          padding: '16px',
          marginBottom: '16px',
        }}>
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: '12px'
          }}>
            <div style={{
              color: 'var(--ft-color-accent, #00E5FF)',
              fontSize: fontSize.body,
              fontWeight: 700,
              display: 'flex',
              alignItems: 'center',
              gap: '4px',
              letterSpacing: '0.5px',
              textTransform: 'uppercase' as const
            }}>
              <Activity size={14} color="var(--ft-color-accent, #00E5FF)" />
              {t('analytics.advancedMetrics', 'ADVANCED PORTFOLIO METRICS (Python-Powered)')}
            </div>
            <button
              onClick={loadOptimization}
              disabled={loadingOptimization}
              onMouseEnter={() => setHoveredOptimize(true)}
              onMouseLeave={() => setHoveredOptimize(false)}
              style={{
                padding: '8px 16px',
                backgroundColor: loadingOptimization ? '#4A4A4A' : (hoveredOptimize ? '#FF9922' : colors.primary),
                color: colors.background,
                border: 'none',
                borderRadius: '2px',
                fontSize: fontSize.tiny,
                fontWeight: 700,
                cursor: loadingOptimization ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px',
                opacity: loadingOptimization ? 0.6 : 1,
                textTransform: 'uppercase',
                letterSpacing: '0.5px',
              }}
            >
              <Target size={10} />
              {loadingOptimization ? t('analytics.optimizing', 'OPTIMIZING...') : t('analytics.optimizeWeights', 'OPTIMIZE WEIGHTS')}
            </button>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(120px, 1fr))', gap: '12px', marginTop: '12px' }}>
            <div style={{ textAlign: 'center' }}>
              <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase', marginBottom: '4px' }}>{t('analytics.sharpeRatio', 'SHARPE RATIO')}</div>
              <div style={{ color: colors.success, fontSize: '15px', fontWeight: 700 }}>
                {advancedMetrics.sharpe_ratio?.toFixed(2) || 'N/A'}
              </div>
            </div>
            <div style={{ textAlign: 'center' }}>
              <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase', marginBottom: '4px' }}>{t('analytics.volatility', 'VOLATILITY')}</div>
              <div style={{ color: 'var(--ft-color-warning, #FFD700)', fontSize: '15px', fontWeight: 700 }}>
                {(advancedMetrics.portfolio_volatility * 100)?.toFixed(2)}%
              </div>
            </div>
            <div style={{ textAlign: 'center' }}>
              <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase', marginBottom: '4px' }}>{t('analytics.var95', 'VaR (95%)')}</div>
              <div style={{ color: colors.alert, fontSize: '15px', fontWeight: 700 }}>
                {(advancedMetrics.value_at_risk_95 * 100)?.toFixed(2)}%
              </div>
            </div>
            <div style={{ textAlign: 'center' }}>
              <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase', marginBottom: '4px' }}>{t('analytics.maxDrawdown', 'MAX DRAWDOWN')}</div>
              <div style={{ color: colors.alert, fontSize: '15px', fontWeight: 700 }}>
                {(advancedMetrics.max_drawdown * 100)?.toFixed(2)}%
              </div>
            </div>
            <div style={{ textAlign: 'center' }}>
              <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase', marginBottom: '4px' }}>{t('analytics.annualReturn', 'ANNUAL RETURN')}</div>
              <div style={{ color: colors.success, fontSize: '15px', fontWeight: 700 }}>
                {(advancedMetrics.portfolio_return * 100)?.toFixed(2)}%
              </div>
            </div>
          </div>

          {/* Optimized Portfolio Section */}
          {optimizedPortfolio && (
            <div style={{
              marginTop: '12px',
              paddingTop: '12px',
              borderTop: '1px solid var(--ft-color-border, #2A2A2A)'
            }}>
              <div style={{
                color: colors.primary,
                fontSize: fontSize.small,
                fontWeight: 700,
                letterSpacing: '0.5px',
                textTransform: 'uppercase' as const,
                marginBottom: '8px'
              }}>
                {t('analytics.optimizedAllocation', 'OPTIMIZED ALLOCATION (Max Sharpe Ratio)')}
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(120px, 1fr))', gap: '12px' }}>
                {portfolioSummary.holdings.map((holding, idx) => {
                  const currentWeight = holding.weight;
                  const optimalWeight = (optimizedPortfolio.optimal_weights[idx] * 100);
                  const diff = optimalWeight - currentWeight;

                  return (
                    <div key={holding.id} style={{
                      padding: '4px',
                      backgroundColor: colors.background,
                      border: '1px solid var(--ft-color-border, #2A2A2A)',
                      borderRadius: '2px',
                    }}>
                      <div style={{
                        color: 'var(--ft-color-accent, #00E5FF)',
                        fontSize: fontSize.small,
                        fontWeight: 700,
                        marginBottom: '2px'
                      }}>
                        {holding.symbol}
                      </div>
                      <div style={{ fontSize: fontSize.tiny, color: colors.textMuted, letterSpacing: '0.5px' }}>
                        Current: {currentWeight.toFixed(1)}%
                      </div>
                      <div style={{ fontSize: fontSize.tiny, color: 'var(--ft-color-warning, #FFD700)' }}>
                        Optimal: {optimalWeight.toFixed(1)}%
                      </div>
                      <div style={{
                        fontSize: fontSize.tiny,
                        color: diff > 0 ? colors.success : diff < 0 ? colors.alert : colors.textMuted
                      }}>
                        {diff > 0 ? '\u2191' : diff < 0 ? '\u2193' : '='} {Math.abs(diff).toFixed(1)}%
                      </div>
                    </div>
                  );
                })}
              </div>
              <div style={{
                marginTop: '8px',
                fontSize: fontSize.small,
                color: colors.textMuted
              }}>
                Optimized Sharpe: <span style={{ color: colors.success, fontWeight: 700 }}>
                  {optimizedPortfolio.sharpe_ratio?.toFixed(2)}
                </span> |
                Expected Return: <span style={{ color: colors.success, fontWeight: 700 }}>
                  {(optimizedPortfolio.expected_return * 100)?.toFixed(2)}%
                </span> |
                Volatility: <span style={{ color: 'var(--ft-color-warning, #FFD700)', fontWeight: 700 }}>
                  {(optimizedPortfolio.volatility * 100)?.toFixed(2)}%
                </span>
              </div>
            </div>
          )}
        </div>
      )}

      {/* Loading State */}
      {loadingMetrics && !advancedMetrics && (
        <div style={{
          backgroundColor: colors.panel,
          border: '1px solid var(--ft-color-accent, #00E5FF)',
          borderRadius: '2px',
          padding: '16px',
          marginBottom: '16px',
          textAlign: 'center',
          color: 'var(--ft-color-accent, #00E5FF)',
          fontSize: fontSize.small
        }}>
          Loading advanced analytics...
        </div>
      )}

      {/* Key Metrics Grid */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(4, 1fr)',
        gap: '12px',
        marginBottom: '16px'
      }}>
        {/* Total Value Card */}
        <div style={{
          backgroundColor: colors.panel,
          border: `1px solid ${colors.primary}`,
          borderRadius: '2px',
          padding: '12px',
          display: 'flex',
          flexDirection: 'column',
          gap: '4px',
        }}>
          <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>{t('analytics.totalValue', 'TOTAL VALUE')}</div>
          <div style={{
            color: 'var(--ft-color-accent, #00E5FF)',
            fontSize: '16px',
            fontWeight: 700
          }}>
            {formatLargeNumber(totalValue, currency)}
          </div>
          <div style={{
            color: colors.textMuted,
            fontSize: fontSize.tiny,
            letterSpacing: '0.5px',
            marginTop: '2px'
          }}>
            {formatCurrency(totalValue, currency)}
          </div>
        </div>

        {/* Total P&L Card */}
        <div style={{
          backgroundColor: colors.panel,
          border: `1px solid ${totalPnL >= 0 ? colors.success : colors.alert}`,
          borderRadius: '2px',
          padding: '12px',
          display: 'flex',
          flexDirection: 'column',
          gap: '4px',
        }}>
          <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>{t('analytics.totalPnl', 'TOTAL P&L')}</div>
          <div style={{
            color: totalPnL >= 0 ? colors.success : colors.alert,
            fontSize: '16px',
            fontWeight: 700
          }}>
            {totalPnL >= 0 ? '+' : ''}{formatLargeNumber(totalPnL, currency)}
          </div>
          <div style={{
            padding: '2px 6px',
            backgroundColor: totalPnL >= 0 ? `${colors.success}20` : `${colors.alert}20`,
            color: totalPnL >= 0 ? colors.success : colors.alert,
            fontSize: '8px',
            fontWeight: 700,
            borderRadius: '2px',
            marginTop: '2px',
            display: 'inline-block',
            width: 'fit-content'
          }}>
            {formatPercent(totalPnLPercent)}
          </div>
        </div>

        {/* Day Change Card */}
        <div style={{
          backgroundColor: colors.panel,
          border: `1px solid ${dayChange >= 0 ? colors.success : colors.alert}`,
          borderRadius: '2px',
          padding: '12px',
          display: 'flex',
          flexDirection: 'column',
          gap: '4px',
        }}>
          <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>{t('analytics.dayChange', 'DAY CHANGE')}</div>
          <div style={{
            color: dayChange >= 0 ? colors.success : colors.alert,
            fontSize: '16px',
            fontWeight: 700
          }}>
            {dayChange >= 0 ? '+' : ''}{formatLargeNumber(dayChange, currency)}
          </div>
          <div style={{
            padding: '2px 6px',
            backgroundColor: dayChange >= 0 ? `${colors.success}20` : `${colors.alert}20`,
            color: dayChange >= 0 ? colors.success : colors.alert,
            fontSize: '8px',
            fontWeight: 700,
            borderRadius: '2px',
            marginTop: '2px',
            display: 'inline-block',
            width: 'fit-content'
          }}>
            {formatPercent(dayChangePercent)}
          </div>
        </div>

        {/* Positions Card */}
        <div style={{
          backgroundColor: colors.panel,
          border: '1px solid var(--ft-color-accent, #00E5FF)',
          borderRadius: '2px',
          padding: '12px',
          display: 'flex',
          flexDirection: 'column',
          gap: '4px',
        }}>
          <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase' }}>{t('analytics.positions', 'POSITIONS')}</div>
          <div style={{
            color: 'var(--ft-color-accent, #00E5FF)',
            fontSize: '16px',
            fontWeight: 700
          }}>
            {positionsCount}
          </div>
          <div style={{
            padding: '2px 6px',
            backgroundColor: 'var(--ft-color-accent, #00E5FF)20',
            color: 'var(--ft-color-accent, #00E5FF)',
            fontSize: '8px',
            fontWeight: 700,
            borderRadius: '2px',
            marginTop: '2px',
            display: 'inline-block',
            width: 'fit-content'
          }}>
            Active Holdings
          </div>
        </div>
      </div>

      {/* Performance Section */}
      <div style={{ display: 'grid', gridTemplateColumns: 'repeat(3, 1fr)', gap: '12px', marginBottom: '16px' }}>
        {/* Top Gainers */}
        <div>
          <div style={{
            padding: '12px',
            backgroundColor: 'var(--ft-color-header, #1A1A1A)',
            borderBottom: `1px solid ${colors.success}`,
            color: colors.success,
            fontSize: fontSize.body,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const,
            marginBottom: '8px',
          }}>
            {t('analytics.topGainers', 'TOP GAINERS')}
          </div>
          {topGainers.length === 0 ? (
            <div style={{ color: colors.textMuted, fontSize: fontSize.small, padding: '8px' }}>{t('analytics.noData', 'No data')}</div>
          ) : (
            topGainers.map(holding => (
              <div
                key={holding.id}
                onMouseEnter={() => setHoveredGainer(holding.id)}
                onMouseLeave={() => setHoveredGainer(null)}
                style={{
                  padding: '8px',
                  marginBottom: '4px',
                  backgroundColor: hoveredGainer === holding.id ? 'var(--ft-color-hover, #1F1F1F)' : colors.panel,
                  borderLeft: `3px solid ${colors.success}`,
                  borderRadius: '2px',
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  transition: 'all 0.2s ease',
                  cursor: 'default'
                }}
              >
                <div>
                  <div style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.small, fontWeight: 700 }}>
                    {holding.symbol}
                  </div>
                  <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, letterSpacing: '0.5px' }}>
                    {formatCurrency(holding.market_value, currency)}
                  </div>
                </div>
                <div style={{ textAlign: 'right' }}>
                  <div style={{ padding: '2px 6px', backgroundColor: `${colors.success}20`, color: colors.success, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                    +{formatPercent(holding.unrealized_pnl_percent)}
                  </div>
                  <div style={{ color: colors.success, fontSize: fontSize.tiny, marginTop: '2px' }}>
                    +{formatCurrency(holding.unrealized_pnl, currency)}
                  </div>
                </div>
              </div>
            ))
          )}
        </div>

        {/* Top Losers */}
        <div>
          <div style={{
            padding: '12px',
            backgroundColor: 'var(--ft-color-header, #1A1A1A)',
            borderBottom: `1px solid ${colors.alert}`,
            color: colors.alert,
            fontSize: fontSize.body,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const,
            marginBottom: '8px',
          }}>
            {t('analytics.topLosers', 'TOP LOSERS')}
          </div>
          {topLosers.length === 0 ? (
            <div style={{ color: colors.textMuted, fontSize: fontSize.small, padding: '8px' }}>{t('analytics.noData', 'No data')}</div>
          ) : (
            topLosers.map(holding => (
              <div
                key={holding.id}
                onMouseEnter={() => setHoveredLoser(holding.id)}
                onMouseLeave={() => setHoveredLoser(null)}
                style={{
                  padding: '8px',
                  marginBottom: '4px',
                  backgroundColor: hoveredLoser === holding.id ? 'var(--ft-color-hover, #1F1F1F)' : colors.panel,
                  borderLeft: `3px solid ${colors.alert}`,
                  borderRadius: '2px',
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  transition: 'all 0.2s ease',
                  cursor: 'default'
                }}
              >
                <div>
                  <div style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.small, fontWeight: 700 }}>
                    {holding.symbol}
                  </div>
                  <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, letterSpacing: '0.5px' }}>
                    {formatCurrency(holding.market_value, currency)}
                  </div>
                </div>
                <div style={{ textAlign: 'right' }}>
                  <div style={{ padding: '2px 6px', backgroundColor: `${colors.alert}20`, color: colors.alert, fontSize: '8px', fontWeight: 700, borderRadius: '2px' }}>
                    {formatPercent(holding.unrealized_pnl_percent)}
                  </div>
                  <div style={{ color: colors.alert, fontSize: fontSize.tiny, marginTop: '2px' }}>
                    {formatCurrency(holding.unrealized_pnl, currency)}
                  </div>
                </div>
              </div>
            ))
          )}
        </div>

        {/* Largest Positions */}
        <div>
          <div style={{
            padding: '12px',
            backgroundColor: 'var(--ft-color-header, #1A1A1A)',
            borderBottom: '1px solid var(--ft-color-accent, #00E5FF)',
            color: 'var(--ft-color-accent, #00E5FF)',
            fontSize: fontSize.body,
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase' as const,
            marginBottom: '8px',
          }}>
            {t('analytics.largestPositions', 'LARGEST POSITIONS')}
          </div>
          {largestPositions.length === 0 ? (
            <div style={{ color: colors.textMuted, fontSize: fontSize.small, padding: '8px' }}>{t('analytics.noData', 'No data')}</div>
          ) : (
            largestPositions.map(holding => (
              <div
                key={holding.id}
                onMouseEnter={() => setHoveredPosition(holding.id)}
                onMouseLeave={() => setHoveredPosition(null)}
                style={{
                  padding: '8px',
                  marginBottom: '4px',
                  backgroundColor: hoveredPosition === holding.id ? 'var(--ft-color-hover, #1F1F1F)' : colors.panel,
                  borderLeft: '3px solid var(--ft-color-accent, #00E5FF)',
                  borderRadius: '2px',
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  transition: 'all 0.2s ease',
                  cursor: 'default'
                }}
              >
                <div>
                  <div style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.small, fontWeight: 700 }}>
                    {holding.symbol}
                  </div>
                  <div style={{ color: colors.textMuted, fontSize: fontSize.tiny, letterSpacing: '0.5px' }}>
                    Weight: {holding.weight.toFixed(1)}%
                  </div>
                </div>
                <div style={{ textAlign: 'right' }}>
                  <div style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.small, fontWeight: 700 }}>
                    {formatLargeNumber(holding.market_value, currency)}
                  </div>
                  <div style={{
                    padding: '2px 6px',
                    backgroundColor: holding.unrealized_pnl >= 0 ? `${colors.success}20` : `${colors.alert}20`,
                    color: holding.unrealized_pnl >= 0 ? colors.success : colors.alert,
                    fontSize: '8px',
                    fontWeight: 700,
                    borderRadius: '2px',
                    marginTop: '2px',
                    display: 'inline-block'
                  }}>
                    {formatPercent(holding.unrealized_pnl_percent)}
                  </div>
                </div>
              </div>
            ))
          )}
        </div>
      </div>

      {/* Allocation Summary */}
      <div>
        <div style={{
          padding: '12px',
          backgroundColor: 'var(--ft-color-header, #1A1A1A)',
          borderBottom: `1px solid ${colors.primary}`,
          color: colors.primary,
          fontSize: fontSize.body,
          fontWeight: 700,
          letterSpacing: '0.5px',
          textTransform: 'uppercase' as const,
          marginBottom: '12px',
        }}>
          {t('analytics.allocationBreakdown', 'ALLOCATION BREAKDOWN')}
        </div>

        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))',
          gap: '8px'
        }}>
          {portfolioSummary.holdings.map(holding => (
            <div
              key={holding.id}
              onMouseEnter={() => setHoveredAllocation(holding.id)}
              onMouseLeave={() => setHoveredAllocation(null)}
              style={{
                padding: '8px',
                backgroundColor: hoveredAllocation === holding.id ? 'var(--ft-color-hover, #1F1F1F)' : colors.panel,
                border: '1px solid var(--ft-color-border, #2A2A2A)',
                borderRadius: '2px',
                transition: 'all 0.2s ease',
                cursor: 'default'
              }}
            >
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                marginBottom: '4px'
              }}>
                <span style={{ color: 'var(--ft-color-accent, #00E5FF)', fontSize: fontSize.small, fontWeight: 700 }}>
                  {holding.symbol}
                </span>
                <span style={{ color: 'var(--ft-color-warning, #FFD700)', fontSize: fontSize.small, fontWeight: 700 }}>
                  {holding.weight.toFixed(1)}%
                </span>
              </div>
              <div style={{
                width: '100%',
                height: '4px',
                backgroundColor: 'var(--ft-color-border, #2A2A2A)',
                borderRadius: '2px',
                overflow: 'hidden'
              }}>
                <div style={{
                  width: `${holding.weight}%`,
                  height: '100%',
                  backgroundColor: 'var(--ft-color-warning, #FFD700)',
                  transition: 'width 0.3s',
                  borderRadius: '2px'
                }} />
              </div>
              <div style={{
                marginTop: '4px',
                display: 'flex',
                justifyContent: 'space-between',
                fontSize: fontSize.tiny,
                color: colors.textMuted,
                letterSpacing: '0.5px'
              }}>
                <span>{formatLargeNumber(holding.market_value, currency)}</span>
                <span style={{
                  padding: '2px 6px',
                  backgroundColor: holding.unrealized_pnl >= 0 ? `${colors.success}20` : `${colors.alert}20`,
                  color: holding.unrealized_pnl >= 0 ? colors.success : colors.alert,
                  fontSize: '8px',
                  fontWeight: 700,
                  borderRadius: '2px',
                }}>
                  {formatPercent(holding.unrealized_pnl_percent)}
                </span>
              </div>
            </div>
          ))}
        </div>
      </div>
    </div>
  );
};

export default AnalyticsView;
