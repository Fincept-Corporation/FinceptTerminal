import React, { useState, useEffect } from 'react';
import { PortfolioSummary, portfolioService } from '../../../../services/portfolio/portfolioService';
import { formatCurrency, formatPercent, formatLargeNumber } from './utils';
import { TrendingUp, Activity, PieChart, Target } from 'lucide-react';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, GRID_TEMPLATES, EFFECTS } from '../finceptStyles';

interface AnalyticsViewProps {
  portfolioSummary: PortfolioSummary;
}

const AnalyticsView: React.FC<AnalyticsViewProps> = ({ portfolioSummary }) => {
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
      backgroundColor: FINCEPT.DARK_BG,
      padding: SPACING.DEFAULT,
      overflow: 'auto',
      fontFamily: TYPOGRAPHY.MONO
    }}>
      {/* Section Header */}
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        marginBottom: SPACING.LARGE
      }}>
        PORTFOLIO ANALYTICS DASHBOARD
      </div>

      {/* Error Message */}
      {analyticsError && (
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: BORDERS.RED,
          borderRadius: '2px',
          padding: SPACING.DEFAULT,
          marginBottom: SPACING.LARGE,
          color: FINCEPT.RED,
          fontSize: '10px'
        }}>
          [WARN] {analyticsError}
        </div>
      )}

      {/* Advanced Analytics Section */}
      {advancedMetrics && (
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: BORDERS.CYAN,
          borderRadius: '2px',
          padding: SPACING.LARGE,
          marginBottom: SPACING.LARGE,
        }}>
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: SPACING.DEFAULT
          }}>
            <div style={{
              color: FINCEPT.CYAN,
              fontSize: '11px',
              fontWeight: 700,
              display: 'flex',
              alignItems: 'center',
              gap: SPACING.SMALL,
              letterSpacing: '0.5px',
              textTransform: 'uppercase' as const
            }}>
              <Activity size={14} color={FINCEPT.CYAN} />
              ADVANCED PORTFOLIO METRICS (Python-Powered)
            </div>
            <button
              onClick={loadOptimization}
              disabled={loadingOptimization}
              onMouseEnter={() => setHoveredOptimize(true)}
              onMouseLeave={() => setHoveredOptimize(false)}
              style={{
                ...COMMON_STYLES.buttonPrimary,
                background: loadingOptimization ? FINCEPT.MUTED : (hoveredOptimize ? '#FF9922' : FINCEPT.ORANGE),
                cursor: loadingOptimization ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: SPACING.SMALL,
                opacity: loadingOptimization ? 0.6 : 1,
              }}
            >
              <Target size={10} />
              {loadingOptimization ? 'OPTIMIZING...' : 'OPTIMIZE WEIGHTS'}
            </button>
          </div>

          <div style={{ ...GRID_TEMPLATES.autoFit('120px'), marginTop: SPACING.DEFAULT }}>
            <div style={{ textAlign: 'center' }}>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>SHARPE RATIO</div>
              <div style={{ color: FINCEPT.GREEN, fontSize: '15px', fontWeight: 700 }}>
                {advancedMetrics.sharpe_ratio?.toFixed(2) || 'N/A'}
              </div>
            </div>
            <div style={{ textAlign: 'center' }}>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>VOLATILITY</div>
              <div style={{ color: FINCEPT.YELLOW, fontSize: '15px', fontWeight: 700 }}>
                {(advancedMetrics.portfolio_volatility * 100)?.toFixed(2)}%
              </div>
            </div>
            <div style={{ textAlign: 'center' }}>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>VaR (95%)</div>
              <div style={{ color: FINCEPT.RED, fontSize: '15px', fontWeight: 700 }}>
                {(advancedMetrics.value_at_risk_95 * 100)?.toFixed(2)}%
              </div>
            </div>
            <div style={{ textAlign: 'center' }}>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>MAX DRAWDOWN</div>
              <div style={{ color: FINCEPT.RED, fontSize: '15px', fontWeight: 700 }}>
                {(advancedMetrics.max_drawdown * 100)?.toFixed(2)}%
              </div>
            </div>
            <div style={{ textAlign: 'center' }}>
              <div style={{ ...COMMON_STYLES.dataLabel, marginBottom: SPACING.SMALL }}>ANNUAL RETURN</div>
              <div style={{ color: FINCEPT.GREEN, fontSize: '15px', fontWeight: 700 }}>
                {(advancedMetrics.portfolio_return * 100)?.toFixed(2)}%
              </div>
            </div>
          </div>

          {/* Optimized Portfolio Section */}
          {optimizedPortfolio && (
            <div style={{
              marginTop: SPACING.DEFAULT,
              paddingTop: SPACING.DEFAULT,
              borderTop: BORDERS.STANDARD
            }}>
              <div style={{
                color: FINCEPT.ORANGE,
                fontSize: '10px',
                fontWeight: 700,
                letterSpacing: '0.5px',
                textTransform: 'uppercase' as const,
                marginBottom: SPACING.MEDIUM
              }}>
                OPTIMIZED ALLOCATION (Max Sharpe Ratio)
              </div>
              <div style={{ ...GRID_TEMPLATES.autoFit('120px') }}>
                {portfolioSummary.holdings.map((holding, idx) => {
                  const currentWeight = holding.weight;
                  const optimalWeight = (optimizedPortfolio.optimal_weights[idx] * 100);
                  const diff = optimalWeight - currentWeight;

                  return (
                    <div key={holding.id} style={{
                      padding: SPACING.SMALL,
                      backgroundColor: FINCEPT.DARK_BG,
                      border: BORDERS.STANDARD,
                      borderRadius: '2px',
                    }}>
                      <div style={{
                        color: FINCEPT.CYAN,
                        fontSize: '10px',
                        fontWeight: 700,
                        marginBottom: SPACING.TINY
                      }}>
                        {holding.symbol}
                      </div>
                      <div style={{ fontSize: '9px', color: FINCEPT.GRAY, letterSpacing: '0.5px' }}>
                        Current: {currentWeight.toFixed(1)}%
                      </div>
                      <div style={{ fontSize: '9px', color: FINCEPT.YELLOW }}>
                        Optimal: {optimalWeight.toFixed(1)}%
                      </div>
                      <div style={{
                        fontSize: '9px',
                        color: diff > 0 ? FINCEPT.GREEN : diff < 0 ? FINCEPT.RED : FINCEPT.GRAY
                      }}>
                        {diff > 0 ? '\u2191' : diff < 0 ? '\u2193' : '='} {Math.abs(diff).toFixed(1)}%
                      </div>
                    </div>
                  );
                })}
              </div>
              <div style={{
                marginTop: SPACING.MEDIUM,
                fontSize: '10px',
                color: FINCEPT.GRAY
              }}>
                Optimized Sharpe: <span style={{ color: FINCEPT.GREEN, fontWeight: 700 }}>
                  {optimizedPortfolio.sharpe_ratio?.toFixed(2)}
                </span> |
                Expected Return: <span style={{ color: FINCEPT.GREEN, fontWeight: 700 }}>
                  {(optimizedPortfolio.expected_return * 100)?.toFixed(2)}%
                </span> |
                Volatility: <span style={{ color: FINCEPT.YELLOW, fontWeight: 700 }}>
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
          backgroundColor: FINCEPT.PANEL_BG,
          border: BORDERS.CYAN,
          borderRadius: '2px',
          padding: SPACING.LARGE,
          marginBottom: SPACING.LARGE,
          textAlign: 'center',
          color: FINCEPT.CYAN,
          fontSize: '10px'
        }}>
          Loading advanced analytics...
        </div>
      )}

      {/* Key Metrics Grid */}
      <div style={{
        ...GRID_TEMPLATES.fourColumn,
        marginBottom: SPACING.LARGE
      }}>
        {/* Total Value Card */}
        <div style={{
          ...COMMON_STYLES.metricCard,
          border: BORDERS.ORANGE
        }}>
          <div style={{ ...COMMON_STYLES.dataLabel }}>TOTAL VALUE</div>
          <div style={{
            color: FINCEPT.CYAN,
            fontSize: '16px',
            fontWeight: 700
          }}>
            {formatLargeNumber(totalValue, currency)}
          </div>
          <div style={{
            color: FINCEPT.GRAY,
            fontSize: '9px',
            letterSpacing: '0.5px',
            marginTop: SPACING.TINY
          }}>
            {formatCurrency(totalValue, currency)}
          </div>
        </div>

        {/* Total P&L Card */}
        <div style={{
          ...COMMON_STYLES.metricCard,
          border: totalPnL >= 0 ? BORDERS.GREEN : BORDERS.RED
        }}>
          <div style={{ ...COMMON_STYLES.dataLabel }}>TOTAL P&L</div>
          <div style={{
            color: totalPnL >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
            fontSize: '16px',
            fontWeight: 700
          }}>
            {totalPnL >= 0 ? '+' : ''}{formatLargeNumber(totalPnL, currency)}
          </div>
          <div style={{
            ...(totalPnL >= 0 ? COMMON_STYLES.badgeSuccess : COMMON_STYLES.badgeError),
            marginTop: SPACING.TINY,
            display: 'inline-block'
          }}>
            {formatPercent(totalPnLPercent)}
          </div>
        </div>

        {/* Day Change Card */}
        <div style={{
          ...COMMON_STYLES.metricCard,
          border: dayChange >= 0 ? BORDERS.GREEN : BORDERS.RED
        }}>
          <div style={{ ...COMMON_STYLES.dataLabel }}>DAY CHANGE</div>
          <div style={{
            color: dayChange >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
            fontSize: '16px',
            fontWeight: 700
          }}>
            {dayChange >= 0 ? '+' : ''}{formatLargeNumber(dayChange, currency)}
          </div>
          <div style={{
            ...(dayChange >= 0 ? COMMON_STYLES.badgeSuccess : COMMON_STYLES.badgeError),
            marginTop: SPACING.TINY,
            display: 'inline-block'
          }}>
            {formatPercent(dayChangePercent)}
          </div>
        </div>

        {/* Positions Card */}
        <div style={{
          ...COMMON_STYLES.metricCard,
          border: BORDERS.CYAN
        }}>
          <div style={{ ...COMMON_STYLES.dataLabel }}>POSITIONS</div>
          <div style={{
            color: FINCEPT.CYAN,
            fontSize: '16px',
            fontWeight: 700
          }}>
            {positionsCount}
          </div>
          <div style={{
            ...COMMON_STYLES.badgeInfo,
            marginTop: SPACING.TINY,
            display: 'inline-block'
          }}>
            Active Holdings
          </div>
        </div>
      </div>

      {/* Performance Section */}
      <div style={{ ...GRID_TEMPLATES.threeColumn, marginBottom: SPACING.LARGE }}>
        {/* Top Gainers */}
        <div>
          <div style={{
            ...COMMON_STYLES.sectionHeader,
            color: FINCEPT.GREEN,
            borderBottom: BORDERS.GREEN,
            marginBottom: SPACING.MEDIUM,
          }}>
            TOP GAINERS
          </div>
          {topGainers.length === 0 ? (
            <div style={{ color: FINCEPT.GRAY, fontSize: '10px', padding: SPACING.MEDIUM }}>No data</div>
          ) : (
            topGainers.map(holding => (
              <div
                key={holding.id}
                onMouseEnter={() => setHoveredGainer(holding.id)}
                onMouseLeave={() => setHoveredGainer(null)}
                style={{
                  padding: SPACING.MEDIUM,
                  marginBottom: SPACING.SMALL,
                  backgroundColor: hoveredGainer === holding.id ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
                  borderLeft: `3px solid ${FINCEPT.GREEN}`,
                  borderRadius: '2px',
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  transition: EFFECTS.TRANSITION_STANDARD,
                  cursor: 'default'
                }}
              >
                <div>
                  <div style={{ color: FINCEPT.CYAN, fontSize: '10px', fontWeight: 700 }}>
                    {holding.symbol}
                  </div>
                  <div style={{ color: FINCEPT.GRAY, fontSize: '9px', letterSpacing: '0.5px' }}>
                    {formatCurrency(holding.market_value, currency)}
                  </div>
                </div>
                <div style={{ textAlign: 'right' }}>
                  <div style={{ ...COMMON_STYLES.badgeSuccess }}>
                    +{formatPercent(holding.unrealized_pnl_percent)}
                  </div>
                  <div style={{ color: FINCEPT.GREEN, fontSize: '9px', marginTop: SPACING.TINY }}>
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
            ...COMMON_STYLES.sectionHeader,
            color: FINCEPT.RED,
            borderBottom: BORDERS.RED,
            marginBottom: SPACING.MEDIUM,
          }}>
            TOP LOSERS
          </div>
          {topLosers.length === 0 ? (
            <div style={{ color: FINCEPT.GRAY, fontSize: '10px', padding: SPACING.MEDIUM }}>No data</div>
          ) : (
            topLosers.map(holding => (
              <div
                key={holding.id}
                onMouseEnter={() => setHoveredLoser(holding.id)}
                onMouseLeave={() => setHoveredLoser(null)}
                style={{
                  padding: SPACING.MEDIUM,
                  marginBottom: SPACING.SMALL,
                  backgroundColor: hoveredLoser === holding.id ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
                  borderLeft: `3px solid ${FINCEPT.RED}`,
                  borderRadius: '2px',
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  transition: EFFECTS.TRANSITION_STANDARD,
                  cursor: 'default'
                }}
              >
                <div>
                  <div style={{ color: FINCEPT.CYAN, fontSize: '10px', fontWeight: 700 }}>
                    {holding.symbol}
                  </div>
                  <div style={{ color: FINCEPT.GRAY, fontSize: '9px', letterSpacing: '0.5px' }}>
                    {formatCurrency(holding.market_value, currency)}
                  </div>
                </div>
                <div style={{ textAlign: 'right' }}>
                  <div style={{ ...COMMON_STYLES.badgeError }}>
                    {formatPercent(holding.unrealized_pnl_percent)}
                  </div>
                  <div style={{ color: FINCEPT.RED, fontSize: '9px', marginTop: SPACING.TINY }}>
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
            ...COMMON_STYLES.sectionHeader,
            color: FINCEPT.CYAN,
            borderBottom: BORDERS.CYAN,
            marginBottom: SPACING.MEDIUM,
          }}>
            LARGEST POSITIONS
          </div>
          {largestPositions.length === 0 ? (
            <div style={{ color: FINCEPT.GRAY, fontSize: '10px', padding: SPACING.MEDIUM }}>No data</div>
          ) : (
            largestPositions.map(holding => (
              <div
                key={holding.id}
                onMouseEnter={() => setHoveredPosition(holding.id)}
                onMouseLeave={() => setHoveredPosition(null)}
                style={{
                  padding: SPACING.MEDIUM,
                  marginBottom: SPACING.SMALL,
                  backgroundColor: hoveredPosition === holding.id ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
                  borderLeft: `3px solid ${FINCEPT.CYAN}`,
                  borderRadius: '2px',
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center',
                  transition: EFFECTS.TRANSITION_STANDARD,
                  cursor: 'default'
                }}
              >
                <div>
                  <div style={{ color: FINCEPT.CYAN, fontSize: '10px', fontWeight: 700 }}>
                    {holding.symbol}
                  </div>
                  <div style={{ color: FINCEPT.GRAY, fontSize: '9px', letterSpacing: '0.5px' }}>
                    Weight: {holding.weight.toFixed(1)}%
                  </div>
                </div>
                <div style={{ textAlign: 'right' }}>
                  <div style={{ color: FINCEPT.CYAN, fontSize: '10px', fontWeight: 700 }}>
                    {formatLargeNumber(holding.market_value, currency)}
                  </div>
                  <div style={{
                    ...(holding.unrealized_pnl >= 0 ? COMMON_STYLES.badgeSuccess : COMMON_STYLES.badgeError),
                    marginTop: SPACING.TINY,
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
          ...COMMON_STYLES.sectionHeader,
          color: FINCEPT.ORANGE,
          borderBottom: BORDERS.ORANGE,
          marginBottom: SPACING.DEFAULT,
        }}>
          ALLOCATION BREAKDOWN
        </div>

        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))',
          gap: SPACING.MEDIUM
        }}>
          {portfolioSummary.holdings.map(holding => (
            <div
              key={holding.id}
              onMouseEnter={() => setHoveredAllocation(holding.id)}
              onMouseLeave={() => setHoveredAllocation(null)}
              style={{
                padding: SPACING.MEDIUM,
                backgroundColor: hoveredAllocation === holding.id ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
                border: BORDERS.STANDARD,
                borderRadius: '2px',
                transition: EFFECTS.TRANSITION_STANDARD,
                cursor: 'default'
              }}
            >
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                marginBottom: SPACING.SMALL
              }}>
                <span style={{ color: FINCEPT.CYAN, fontSize: '10px', fontWeight: 700 }}>
                  {holding.symbol}
                </span>
                <span style={{ color: FINCEPT.YELLOW, fontSize: '10px', fontWeight: 700 }}>
                  {holding.weight.toFixed(1)}%
                </span>
              </div>
              <div style={{
                width: '100%',
                height: '4px',
                backgroundColor: FINCEPT.BORDER,
                borderRadius: '2px',
                overflow: 'hidden'
              }}>
                <div style={{
                  width: `${holding.weight}%`,
                  height: '100%',
                  backgroundColor: FINCEPT.YELLOW,
                  transition: 'width 0.3s',
                  borderRadius: '2px'
                }} />
              </div>
              <div style={{
                marginTop: SPACING.SMALL,
                display: 'flex',
                justifyContent: 'space-between',
                fontSize: '9px',
                color: FINCEPT.GRAY,
                letterSpacing: '0.5px'
              }}>
                <span>{formatLargeNumber(holding.market_value, currency)}</span>
                <span style={{
                  ...(holding.unrealized_pnl >= 0 ? COMMON_STYLES.badgeSuccess : COMMON_STYLES.badgeError),
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
