import React, { useState, useEffect } from 'react';
import { PortfolioSummary, portfolioService } from '../../../services/portfolioService';
import { BLOOMBERG_COLORS, formatCurrency, formatPercent, formatLargeNumber } from './utils';
import { TrendingUp, Activity, PieChart, Target } from 'lucide-react';

interface AnalyticsViewProps {
  portfolioSummary: PortfolioSummary;
}

const AnalyticsView: React.FC<AnalyticsViewProps> = ({ portfolioSummary }) => {
  const { ORANGE, WHITE, RED, GREEN, GRAY, CYAN, YELLOW, BLUE } = BLOOMBERG_COLORS;
  const currency = portfolioSummary.portfolio.currency;

  // Advanced analytics state
  const [advancedMetrics, setAdvancedMetrics] = useState<any>(null);
  const [optimizedPortfolio, setOptimizedPortfolio] = useState<any>(null);
  const [loadingMetrics, setLoadingMetrics] = useState(false);
  const [loadingOptimization, setLoadingOptimization] = useState(false);
  const [analyticsError, setAnalyticsError] = useState<string | null>(null);

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
    <div>
      <div style={{
        color: ORANGE,
        fontSize: '12px',
        fontWeight: 'bold',
        marginBottom: '16px'
      }}>
        PORTFOLIO ANALYTICS DASHBOARD
      </div>

      {/* Advanced Analytics Section */}
      {analyticsError && (
        <div style={{
          backgroundColor: 'rgba(255,0,0,0.1)',
          border: `1px solid ${RED}`,
          padding: '12px',
          marginBottom: '16px',
          borderRadius: '4px',
          color: RED,
          fontSize: '10px'
        }}>
          ⚠ {analyticsError}
        </div>
      )}

      {advancedMetrics && (
        <div style={{
          backgroundColor: 'rgba(100,150,250,0.05)',
          border: `1px solid ${BLUE}`,
          padding: '16px',
          marginBottom: '20px',
          borderRadius: '4px'
        }}>
          <div style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            marginBottom: '12px'
          }}>
            <div style={{ color: BLUE, fontSize: '11px', fontWeight: 'bold', display: 'flex', alignItems: 'center', gap: '6px' }}>
              <Activity size={14} />
              ADVANCED PORTFOLIO METRICS (Python-Powered)
            </div>
            <button
              onClick={loadOptimization}
              disabled={loadingOptimization}
              style={{
                background: loadingOptimization ? GRAY : ORANGE,
                color: 'black',
                border: 'none',
                padding: '4px 12px',
                fontSize: '9px',
                fontWeight: 'bold',
                cursor: loadingOptimization ? 'not-allowed' : 'pointer',
                display: 'flex',
                alignItems: 'center',
                gap: '4px'
              }}
            >
              <Target size={10} />
              {loadingOptimization ? 'OPTIMIZING...' : 'OPTIMIZE WEIGHTS'}
            </button>
          </div>

          <div style={{ display: 'grid', gridTemplateColumns: 'repeat(5, 1fr)', gap: '12px' }}>
            <div style={{ textAlign: 'center' }}>
              <div style={{ color: GRAY, fontSize: '8px', marginBottom: '4px' }}>SHARPE RATIO</div>
              <div style={{ color: GREEN, fontSize: '14px', fontWeight: 'bold' }}>
                {advancedMetrics.sharpe_ratio?.toFixed(2) || 'N/A'}
              </div>
            </div>
            <div style={{ textAlign: 'center' }}>
              <div style={{ color: GRAY, fontSize: '8px', marginBottom: '4px' }}>VOLATILITY</div>
              <div style={{ color: YELLOW, fontSize: '14px', fontWeight: 'bold' }}>
                {(advancedMetrics.portfolio_volatility * 100)?.toFixed(2)}%
              </div>
            </div>
            <div style={{ textAlign: 'center' }}>
              <div style={{ color: GRAY, fontSize: '8px', marginBottom: '4px' }}>VaR (95%)</div>
              <div style={{ color: RED, fontSize: '14px', fontWeight: 'bold' }}>
                {(advancedMetrics.value_at_risk_95 * 100)?.toFixed(2)}%
              </div>
            </div>
            <div style={{ textAlign: 'center' }}>
              <div style={{ color: GRAY, fontSize: '8px', marginBottom: '4px' }}>MAX DRAWDOWN</div>
              <div style={{ color: RED, fontSize: '14px', fontWeight: 'bold' }}>
                {(advancedMetrics.max_drawdown * 100)?.toFixed(2)}%
              </div>
            </div>
            <div style={{ textAlign: 'center' }}>
              <div style={{ color: GRAY, fontSize: '8px', marginBottom: '4px' }}>ANNUAL RETURN</div>
              <div style={{ color: GREEN, fontSize: '14px', fontWeight: 'bold' }}>
                {(advancedMetrics.portfolio_return * 100)?.toFixed(2)}%
              </div>
            </div>
          </div>

          {optimizedPortfolio && (
            <div style={{ marginTop: '12px', paddingTop: '12px', borderTop: `1px solid ${GRAY}` }}>
              <div style={{ color: ORANGE, fontSize: '10px', fontWeight: 'bold', marginBottom: '8px' }}>
                OPTIMIZED ALLOCATION (Max Sharpe Ratio)
              </div>
              <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fill, minmax(120px, 1fr))', gap: '8px' }}>
                {portfolioSummary.holdings.map((holding, idx) => {
                  const currentWeight = holding.weight;
                  const optimalWeight = (optimizedPortfolio.optimal_weights[idx] * 100);
                  const diff = optimalWeight - currentWeight;

                  return (
                    <div key={holding.id} style={{
                      padding: '6px',
                      backgroundColor: 'rgba(255,165,0,0.05)',
                      border: `1px solid ${ORANGE}`,
                      borderRadius: '2px'
                    }}>
                      <div style={{ color: CYAN, fontSize: '9px', fontWeight: 'bold', marginBottom: '2px' }}>
                        {holding.symbol}
                      </div>
                      <div style={{ fontSize: '8px', color: GRAY }}>
                        Current: {currentWeight.toFixed(1)}%
                      </div>
                      <div style={{ fontSize: '8px', color: YELLOW }}>
                        Optimal: {optimalWeight.toFixed(1)}%
                      </div>
                      <div style={{ fontSize: '8px', color: diff > 0 ? GREEN : diff < 0 ? RED : GRAY }}>
                        {diff > 0 ? '↑' : diff < 0 ? '↓' : '='} {Math.abs(diff).toFixed(1)}%
                      </div>
                    </div>
                  );
                })}
              </div>
              <div style={{ marginTop: '8px', fontSize: '9px', color: GRAY }}>
                Optimized Sharpe: <span style={{ color: GREEN, fontWeight: 'bold' }}>
                  {optimizedPortfolio.sharpe_ratio?.toFixed(2)}
                </span> |
                Expected Return: <span style={{ color: GREEN, fontWeight: 'bold' }}>
                  {(optimizedPortfolio.expected_return * 100)?.toFixed(2)}%
                </span> |
                Volatility: <span style={{ color: YELLOW, fontWeight: 'bold' }}>
                  {(optimizedPortfolio.volatility * 100)?.toFixed(2)}%
                </span>
              </div>
            </div>
          )}
        </div>
      )}

      {loadingMetrics && !advancedMetrics && (
        <div style={{
          backgroundColor: 'rgba(100,150,250,0.05)',
          border: `1px solid ${BLUE}`,
          padding: '16px',
          marginBottom: '20px',
          borderRadius: '4px',
          textAlign: 'center',
          color: CYAN,
          fontSize: '10px'
        }}>
          Loading advanced analytics...
        </div>
      )}

      {/* Key Metrics Grid */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(4, 1fr)',
        gap: '12px',
        marginBottom: '20px'
      }}>
        {/* Total Value Card */}
        <div style={{
          backgroundColor: 'rgba(255,165,0,0.1)',
          border: `1px solid ${ORANGE}`,
          padding: '12px',
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>TOTAL VALUE</div>
          <div style={{ color: YELLOW, fontSize: '16px', fontWeight: 'bold' }}>
            {formatLargeNumber(totalValue, currency)}
          </div>
          <div style={{ color: GRAY, fontSize: '8px', marginTop: '2px' }}>
            {formatCurrency(totalValue, currency)}
          </div>
        </div>

        {/* Total P&L Card */}
        <div style={{
          backgroundColor: totalPnL >= 0 ? 'rgba(0,200,0,0.1)' : 'rgba(255,0,0,0.1)',
          border: `1px solid ${totalPnL >= 0 ? GREEN : RED}`,
          padding: '12px',
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>TOTAL P&L</div>
          <div style={{ color: totalPnL >= 0 ? GREEN : RED, fontSize: '16px', fontWeight: 'bold' }}>
            {formatLargeNumber(totalPnL, currency)}
          </div>
          <div style={{ color: totalPnL >= 0 ? GREEN : RED, fontSize: '10px', marginTop: '2px' }}>
            {formatPercent(totalPnLPercent)}
          </div>
        </div>

        {/* Day Change Card */}
        <div style={{
          backgroundColor: dayChange >= 0 ? 'rgba(0,200,0,0.1)' : 'rgba(255,0,0,0.1)',
          border: `1px solid ${dayChange >= 0 ? GREEN : RED}`,
          padding: '12px',
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>DAY CHANGE</div>
          <div style={{ color: dayChange >= 0 ? GREEN : RED, fontSize: '16px', fontWeight: 'bold' }}>
            {formatLargeNumber(dayChange, currency)}
          </div>
          <div style={{ color: dayChange >= 0 ? GREEN : RED, fontSize: '10px', marginTop: '2px' }}>
            {formatPercent(dayChangePercent)}
          </div>
        </div>

        {/* Positions Card */}
        <div style={{
          backgroundColor: 'rgba(0,255,255,0.1)',
          border: `1px solid ${CYAN}`,
          padding: '12px',
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '4px' }}>POSITIONS</div>
          <div style={{ color: CYAN, fontSize: '16px', fontWeight: 'bold' }}>
            {positionsCount}
          </div>
          <div style={{ color: GRAY, fontSize: '8px', marginTop: '2px' }}>
            Active Holdings
          </div>
        </div>
      </div>

      {/* Performance Section */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: '16px' }}>
        {/* Top Gainers */}
        <div>
          <div style={{
            color: GREEN,
            fontSize: '11px',
            fontWeight: 'bold',
            marginBottom: '8px',
            paddingBottom: '4px',
            borderBottom: `1px solid ${GREEN}`
          }}>
            TOP GAINERS
          </div>
          {topGainers.length === 0 ? (
            <div style={{ color: GRAY, fontSize: '10px', padding: '8px' }}>No data</div>
          ) : (
            topGainers.map(holding => (
              <div
                key={holding.id}
                style={{
                  padding: '8px',
                  marginBottom: '4px',
                  backgroundColor: 'rgba(0,200,0,0.05)',
                  borderLeft: `3px solid ${GREEN}`,
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center'
                }}
              >
                <div>
                  <div style={{ color: CYAN, fontSize: '10px', fontWeight: 'bold' }}>
                    {holding.symbol}
                  </div>
                  <div style={{ color: GRAY, fontSize: '8px' }}>
                    {formatCurrency(holding.market_value, currency)}
                  </div>
                </div>
                <div style={{ textAlign: 'right' }}>
                  <div style={{ color: GREEN, fontSize: '10px', fontWeight: 'bold' }}>
                    {formatPercent(holding.unrealized_pnl_percent)}
                  </div>
                  <div style={{ color: GREEN, fontSize: '8px' }}>
                    {formatCurrency(holding.unrealized_pnl, currency)}
                  </div>
                </div>
              </div>
            ))
          )}
        </div>

        {/* Top Losers */}
        <div>
          <div style={{
            color: RED,
            fontSize: '11px',
            fontWeight: 'bold',
            marginBottom: '8px',
            paddingBottom: '4px',
            borderBottom: `1px solid ${RED}`
          }}>
            TOP LOSERS
          </div>
          {topLosers.length === 0 ? (
            <div style={{ color: GRAY, fontSize: '10px', padding: '8px' }}>No data</div>
          ) : (
            topLosers.map(holding => (
              <div
                key={holding.id}
                style={{
                  padding: '8px',
                  marginBottom: '4px',
                  backgroundColor: 'rgba(255,0,0,0.05)',
                  borderLeft: `3px solid ${RED}`,
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center'
                }}
              >
                <div>
                  <div style={{ color: CYAN, fontSize: '10px', fontWeight: 'bold' }}>
                    {holding.symbol}
                  </div>
                  <div style={{ color: GRAY, fontSize: '8px' }}>
                    {formatCurrency(holding.market_value, currency)}
                  </div>
                </div>
                <div style={{ textAlign: 'right' }}>
                  <div style={{ color: RED, fontSize: '10px', fontWeight: 'bold' }}>
                    {formatPercent(holding.unrealized_pnl_percent)}
                  </div>
                  <div style={{ color: RED, fontSize: '8px' }}>
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
            color: BLUE,
            fontSize: '11px',
            fontWeight: 'bold',
            marginBottom: '8px',
            paddingBottom: '4px',
            borderBottom: `1px solid ${BLUE}`
          }}>
            LARGEST POSITIONS
          </div>
          {largestPositions.length === 0 ? (
            <div style={{ color: GRAY, fontSize: '10px', padding: '8px' }}>No data</div>
          ) : (
            largestPositions.map(holding => (
              <div
                key={holding.id}
                style={{
                  padding: '8px',
                  marginBottom: '4px',
                  backgroundColor: 'rgba(100,150,250,0.05)',
                  borderLeft: `3px solid ${BLUE}`,
                  display: 'flex',
                  justifyContent: 'space-between',
                  alignItems: 'center'
                }}
              >
                <div>
                  <div style={{ color: CYAN, fontSize: '10px', fontWeight: 'bold' }}>
                    {holding.symbol}
                  </div>
                  <div style={{ color: GRAY, fontSize: '8px' }}>
                    Weight: {holding.weight.toFixed(1)}%
                  </div>
                </div>
                <div style={{ textAlign: 'right' }}>
                  <div style={{ color: WHITE, fontSize: '10px', fontWeight: 'bold' }}>
                    {formatLargeNumber(holding.market_value, currency)}
                  </div>
                  <div style={{
                    color: holding.unrealized_pnl >= 0 ? GREEN : RED,
                    fontSize: '8px'
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
      <div style={{ marginTop: '20px' }}>
        <div style={{
          color: ORANGE,
          fontSize: '11px',
          fontWeight: 'bold',
          marginBottom: '12px',
          paddingBottom: '4px',
          borderBottom: `1px solid ${ORANGE}`
        }}>
          ALLOCATION BREAKDOWN
        </div>

        <div style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(auto-fill, minmax(200px, 1fr))',
          gap: '8px'
        }}>
          {portfolioSummary.holdings.map(holding => (
            <div
              key={holding.id}
              style={{
                padding: '8px',
                backgroundColor: 'rgba(255,255,255,0.02)',
                border: `1px solid rgba(120,120,120,0.3)`,
                borderRadius: '2px'
              }}
            >
              <div style={{
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
                marginBottom: '4px'
              }}>
                <span style={{ color: CYAN, fontSize: '10px', fontWeight: 'bold' }}>
                  {holding.symbol}
                </span>
                <span style={{ color: YELLOW, fontSize: '10px', fontWeight: 'bold' }}>
                  {holding.weight.toFixed(1)}%
                </span>
              </div>
              <div style={{
                width: '100%',
                height: '4px',
                backgroundColor: 'rgba(120,120,120,0.3)',
                borderRadius: '2px',
                overflow: 'hidden'
              }}>
                <div style={{
                  width: `${holding.weight}%`,
                  height: '100%',
                  backgroundColor: YELLOW,
                  transition: 'width 0.3s'
                }} />
              </div>
              <div style={{
                marginTop: '4px',
                display: 'flex',
                justifyContent: 'space-between',
                fontSize: '8px',
                color: GRAY
              }}>
                <span>{formatLargeNumber(holding.market_value, currency)}</span>
                <span style={{
                  color: holding.unrealized_pnl >= 0 ? GREEN : RED
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
