import React, { useState, useEffect } from 'react';
import { PortfolioSummary, portfolioService } from '../../../../services/portfolio/portfolioService';
import {
  formatPercent,
  calculateSharpeRatio,
  calculateBeta,
  calculateVolatility,
  calculateMaxDrawdown,
  calculateVaR
} from './utils';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES, EFFECTS } from '../finceptStyles';

interface RiskMetricsViewProps {
  portfolioSummary: PortfolioSummary;
}

const RiskMetricsView: React.FC<RiskMetricsViewProps> = ({ portfolioSummary }) => {
  const [loading, setLoading] = useState(true);
  const [portfolioReturns, setPortfolioReturns] = useState<number[]>([]);
  const [portfolioValues, setPortfolioValues] = useState<number[]>([]);
  const [hoveredCard, setHoveredCard] = useState<string | null>(null);

  useEffect(() => {
    const loadRiskData = async () => {
      try {
        // Fetch portfolio snapshots for risk calculations
        const snapshots = await portfolioService.getPortfolioPerformanceHistory(
          portfolioSummary.portfolio.id,
          252 // Last ~1 year of data
        );

        if (snapshots.length > 1) {
          // Calculate daily returns from snapshots
          const returns: number[] = [];
          const values: number[] = [];

          for (let i = snapshots.length - 1; i >= 0; i--) {
            values.push(snapshots[i].total_value);

            if (i < snapshots.length - 1) {
              const prevValue = snapshots[i + 1].total_value;
              const currentValue = snapshots[i].total_value;
              const dailyReturn = (currentValue - prevValue) / prevValue;
              returns.push(dailyReturn);
            }
          }

          setPortfolioReturns(returns);
          setPortfolioValues(values);
        } else {
          // Not enough historical data
          setPortfolioReturns([]);
          setPortfolioValues([portfolioSummary.total_market_value]);
        }
      } catch (error) {
        console.error('Failed to load risk metrics data:', error);
        setPortfolioReturns([]);
        setPortfolioValues([portfolioSummary.total_market_value]);
      } finally {
        setLoading(false);
      }
    };

    loadRiskData();
  }, [portfolioSummary.portfolio.id]);

  // Calculate risk metrics from real data
  const sharpeRatio = portfolioReturns.length > 0 ? calculateSharpeRatio(portfolioReturns) : 0;
  const beta = portfolioReturns.length > 0 ? calculateBeta(portfolioReturns, portfolioReturns) : 1; // Using portfolio returns as market proxy until we have real market data
  const volatility = portfolioReturns.length > 0 ? calculateVolatility(portfolioReturns) : 0;
  const maxDrawdown = portfolioValues.length > 1 ? calculateMaxDrawdown(portfolioValues) : 0;
  const var95 = portfolioReturns.length > 0 ? calculateVaR(portfolioReturns, 0.95) : 0;

  const hasInsufficientData = portfolioReturns.length < 5;

  // Risk level determination
  const getRiskLevel = (sharpe: number) => {
    if (sharpe > 2) return { level: 'LOW', color: FINCEPT.GREEN };
    if (sharpe > 1) return { level: 'MODERATE', color: FINCEPT.YELLOW };
    if (sharpe > 0) return { level: 'HIGH', color: FINCEPT.ORANGE };
    return { level: 'VERY HIGH', color: FINCEPT.RED };
  };

  const riskLevel = getRiskLevel(sharpeRatio);

  const getBetaRating = (beta: number) => {
    if (beta < 0.8) return { rating: 'DEFENSIVE', color: FINCEPT.BLUE };
    if (beta < 1.2) return { rating: 'NEUTRAL', color: FINCEPT.CYAN };
    return { rating: 'AGGRESSIVE', color: FINCEPT.ORANGE };
  };

  const betaRating = getBetaRating(beta);

  const getVolatilityRating = (vol: number) => {
    if (vol < 10) return { rating: 'LOW', color: FINCEPT.GREEN };
    if (vol < 20) return { rating: 'MODERATE', color: FINCEPT.YELLOW };
    if (vol < 30) return { rating: 'HIGH', color: FINCEPT.ORANGE };
    return { rating: 'VERY HIGH', color: FINCEPT.RED };
  };

  const volatilityRating = getVolatilityRating(volatility);

  if (loading) {
    return (
      <div style={{
        height: '100%',
        backgroundColor: FINCEPT.DARK_BG,
        padding: '24px',
        textAlign: 'center',
        fontFamily: TYPOGRAPHY.MONO
      }}>
        <div style={{ color: FINCEPT.ORANGE, fontSize: '11px', marginBottom: '4px' }}>
          Loading risk metrics...
        </div>
        <div style={{ color: FINCEPT.GRAY, fontSize: '10px' }}>
          Calculating portfolio risk from historical data
        </div>
      </div>
    );
  }

  return (
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      padding: '12px',
      overflow: 'auto',
      fontFamily: TYPOGRAPHY.MONO
    }}>
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        padding: '12px',
        backgroundColor: FINCEPT.HEADER_BG,
        borderBottom: BORDERS.STANDARD,
        marginBottom: '16px',
        borderRadius: '2px'
      }}>
        RISK METRICS & ANALYTICS {hasInsufficientData && '(LIMITED DATA)'}
      </div>

      {hasInsufficientData && (
        <div style={{
          padding: '8px',
          marginBottom: '12px',
          backgroundColor: 'rgba(255,136,0,0.1)',
          border: `1px solid ${FINCEPT.ORANGE}`,
          borderRadius: '2px',
          fontSize: '10px',
          color: FINCEPT.ORANGE
        }}>
          [WARN] <strong>INSUFFICIENT HISTORICAL DATA:</strong> Risk metrics require at least 5 days of portfolio snapshots for accurate calculations.
          Current data points: {portfolioReturns.length}. Metrics shown are preliminary.
        </div>
      )}

      {/* Risk Overview Cards */}
      <div style={{
        display: 'grid',
        gridTemplateColumns: 'repeat(3, 1fr)',
        gap: '12px',
        marginBottom: '24px'
      }}>
        {/* Overall Risk Level */}
        <div
          style={{
            padding: '12px',
            backgroundColor: hoveredCard === 'risk' ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
            border: `2px solid ${riskLevel.color}`,
            borderRadius: '2px',
            transition: EFFECTS.TRANSITION_STANDARD
          }}
          onMouseEnter={() => setHoveredCard('risk')}
          onMouseLeave={() => setHoveredCard(null)}
        >
          <div style={{
            color: FINCEPT.GRAY,
            fontSize: '9px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            marginBottom: '4px'
          }}>OVERALL RISK</div>
          <div style={{
            color: riskLevel.color,
            fontSize: '16px',
            fontWeight: 700,
            marginBottom: '2px'
          }}>
            {riskLevel.level}
          </div>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px' }}>
            Based on Sharpe Ratio: {sharpeRatio.toFixed(2)}
          </div>
        </div>

        {/* Beta Rating */}
        <div
          style={{
            padding: '12px',
            backgroundColor: hoveredCard === 'beta-overview' ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
            border: `2px solid ${betaRating.color}`,
            borderRadius: '2px',
            transition: EFFECTS.TRANSITION_STANDARD
          }}
          onMouseEnter={() => setHoveredCard('beta-overview')}
          onMouseLeave={() => setHoveredCard(null)}
        >
          <div style={{
            color: FINCEPT.GRAY,
            fontSize: '9px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            marginBottom: '4px'
          }}>MARKET SENSITIVITY</div>
          <div style={{
            color: betaRating.color,
            fontSize: '16px',
            fontWeight: 700,
            marginBottom: '2px'
          }}>
            {betaRating.rating}
          </div>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px' }}>
            Beta: {beta.toFixed(2)}
          </div>
        </div>

        {/* Volatility Rating */}
        <div
          style={{
            padding: '12px',
            backgroundColor: hoveredCard === 'vol-overview' ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
            border: `2px solid ${volatilityRating.color}`,
            borderRadius: '2px',
            transition: EFFECTS.TRANSITION_STANDARD
          }}
          onMouseEnter={() => setHoveredCard('vol-overview')}
          onMouseLeave={() => setHoveredCard(null)}
        >
          <div style={{
            color: FINCEPT.GRAY,
            fontSize: '9px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            marginBottom: '4px'
          }}>VOLATILITY</div>
          <div style={{
            color: volatilityRating.color,
            fontSize: '16px',
            fontWeight: 700,
            marginBottom: '2px'
          }}>
            {volatilityRating.rating}
          </div>
          <div style={{ color: FINCEPT.GRAY, fontSize: '9px' }}>
            Annualized: {volatility.toFixed(2)}%
          </div>
        </div>
      </div>

      {/* Detailed Metrics Grid */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px', marginBottom: '24px' }}>
        {/* Left Column - Return Metrics */}
        <div>
          <div style={{
            color: FINCEPT.ORANGE,
            fontSize: '11px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            marginBottom: '8px',
            paddingBottom: '4px',
            borderBottom: `1px solid ${FINCEPT.ORANGE}`
          }}>
            RISK-ADJUSTED RETURNS
          </div>

          {/* Sharpe Ratio */}
          <div
            style={{
              padding: '8px',
              marginBottom: '8px',
              backgroundColor: hoveredCard === 'sharpe' ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
              border: BORDERS.STANDARD,
              borderRadius: '2px',
              transition: EFFECTS.TRANSITION_STANDARD
            }}
            onMouseEnter={() => setHoveredCard('sharpe')}
            onMouseLeave={() => setHoveredCard(null)}
          >
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{
                  color: FINCEPT.CYAN,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>SHARPE RATIO</div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '2px' }}>
                  Risk-adjusted return measure
                </div>
              </div>
              <div style={{
                color: sharpeRatio > 1 ? FINCEPT.GREEN : sharpeRatio > 0 ? FINCEPT.YELLOW : FINCEPT.RED,
                fontSize: '16px',
                fontWeight: 700
              }}>
                {sharpeRatio.toFixed(3)}
              </div>
            </div>
            <div style={{ marginTop: '4px', fontSize: '9px', color: FINCEPT.GRAY }}>
              {'> 2: Excellent | 1-2: Good | 0-1: Fair | < 0: Poor'}
            </div>
          </div>

          {/* Beta */}
          <div
            style={{
              padding: '8px',
              marginBottom: '8px',
              backgroundColor: hoveredCard === 'beta' ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
              border: BORDERS.STANDARD,
              borderRadius: '2px',
              transition: EFFECTS.TRANSITION_STANDARD
            }}
            onMouseEnter={() => setHoveredCard('beta')}
            onMouseLeave={() => setHoveredCard(null)}
          >
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{
                  color: FINCEPT.CYAN,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>BETA ({'\u03B2'})</div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '2px' }}>
                  Market correlation coefficient
                </div>
              </div>
              <div style={{ color: betaRating.color, fontSize: '16px', fontWeight: 700 }}>
                {beta.toFixed(3)}
              </div>
            </div>
            <div style={{ marginTop: '4px', fontSize: '9px', color: FINCEPT.GRAY }}>
              {'< 1: Less volatile | 1: Market neutral | > 1: More volatile'}
            </div>
          </div>

          {/* Volatility */}
          <div
            style={{
              padding: '8px',
              backgroundColor: hoveredCard === 'volatility' ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
              border: BORDERS.STANDARD,
              borderRadius: '2px',
              transition: EFFECTS.TRANSITION_STANDARD
            }}
            onMouseEnter={() => setHoveredCard('volatility')}
            onMouseLeave={() => setHoveredCard(null)}
          >
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{
                  color: FINCEPT.CYAN,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>VOLATILITY</div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '2px' }}>
                  Annualized standard deviation
                </div>
              </div>
              <div style={{ color: volatilityRating.color, fontSize: '16px', fontWeight: 700 }}>
                {volatility.toFixed(2)}%
              </div>
            </div>
            <div style={{ marginTop: '4px', fontSize: '9px', color: FINCEPT.GRAY }}>
              Lower volatility indicates more stable returns
            </div>
          </div>
        </div>

        {/* Right Column - Risk Metrics */}
        <div>
          <div style={{
            color: FINCEPT.ORANGE,
            fontSize: '11px',
            fontWeight: 700,
            letterSpacing: '0.5px',
            textTransform: 'uppercase',
            marginBottom: '8px',
            paddingBottom: '4px',
            borderBottom: `1px solid ${FINCEPT.ORANGE}`
          }}>
            DOWNSIDE RISK METRICS
          </div>

          {/* Maximum Drawdown */}
          <div
            style={{
              padding: '8px',
              marginBottom: '8px',
              backgroundColor: hoveredCard === 'drawdown' ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
              border: BORDERS.STANDARD,
              borderRadius: '2px',
              transition: EFFECTS.TRANSITION_STANDARD
            }}
            onMouseEnter={() => setHoveredCard('drawdown')}
            onMouseLeave={() => setHoveredCard(null)}
          >
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{
                  color: FINCEPT.CYAN,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>MAX DRAWDOWN</div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '2px' }}>
                  Largest peak-to-trough decline
                </div>
              </div>
              <div style={{
                color: maxDrawdown > 20 ? FINCEPT.RED : maxDrawdown > 10 ? FINCEPT.ORANGE : FINCEPT.GREEN,
                fontSize: '16px',
                fontWeight: 700
              }}>
                -{maxDrawdown.toFixed(2)}%
              </div>
            </div>
            <div style={{ marginTop: '4px', fontSize: '9px', color: FINCEPT.GRAY }}>
              Maximum loss from peak value
            </div>
          </div>

          {/* Value at Risk */}
          <div
            style={{
              padding: '8px',
              marginBottom: '8px',
              backgroundColor: hoveredCard === 'var' ? FINCEPT.HOVER : FINCEPT.PANEL_BG,
              border: BORDERS.STANDARD,
              borderRadius: '2px',
              transition: EFFECTS.TRANSITION_STANDARD
            }}
            onMouseEnter={() => setHoveredCard('var')}
            onMouseLeave={() => setHoveredCard(null)}
          >
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{
                  color: FINCEPT.CYAN,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>VALUE AT RISK (95%)</div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '2px' }}>
                  Expected loss at 95% confidence
                </div>
              </div>
              <div style={{
                color: var95 < -0.03 ? FINCEPT.RED : var95 < -0.02 ? FINCEPT.ORANGE : FINCEPT.YELLOW,
                fontSize: '16px',
                fontWeight: 700
              }}>
                {(var95 * 100).toFixed(2)}%
              </div>
            </div>
            <div style={{ marginTop: '4px', fontSize: '9px', color: FINCEPT.GRAY }}>
              5% chance of losing more than this in a day
            </div>
          </div>

          {/* Risk Score */}
          <div
            style={{
              padding: '8px',
              backgroundColor: hoveredCard === 'composite' ? 'rgba(255,59,59,0.08)' : 'rgba(255,59,59,0.05)',
              border: `2px solid ${riskLevel.color}`,
              borderRadius: '2px',
              transition: EFFECTS.TRANSITION_STANDARD
            }}
            onMouseEnter={() => setHoveredCard('composite')}
            onMouseLeave={() => setHoveredCard(null)}
          >
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{
                  color: riskLevel.color,
                  fontSize: '10px',
                  fontWeight: 700,
                  letterSpacing: '0.5px',
                  textTransform: 'uppercase'
                }}>COMPOSITE RISK SCORE</div>
                <div style={{ color: FINCEPT.GRAY, fontSize: '9px', marginTop: '2px' }}>
                  Weighted risk assessment
                </div>
              </div>
              <div style={{ color: riskLevel.color, fontSize: '16px', fontWeight: 700 }}>
                {((1 / (sharpeRatio + 1)) * 100).toFixed(0)}/100
              </div>
            </div>
            <div style={{ marginTop: '4px' }}>
              <div style={{
                width: '100%',
                height: '8px',
                backgroundColor: FINCEPT.HOVER,
                borderRadius: '2px',
                overflow: 'hidden'
              }}>
                <div style={{
                  width: `${(1 / (sharpeRatio + 1)) * 100}%`,
                  height: '100%',
                  backgroundColor: riskLevel.color,
                  transition: 'width 0.3s'
                }} />
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Risk Recommendations */}
      <div style={{
        padding: '12px',
        backgroundColor: 'rgba(255,136,0,0.05)',
        border: `1px solid ${FINCEPT.ORANGE}`,
        borderRadius: '2px'
      }}>
        <div style={{
          color: FINCEPT.ORANGE,
          fontSize: '11px',
          fontWeight: 700,
          letterSpacing: '0.5px',
          textTransform: 'uppercase',
          marginBottom: '8px'
        }}>
          RISK MANAGEMENT RECOMMENDATIONS
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '8px' }}>
          {sharpeRatio < 1 && (
            <div style={{
              fontSize: '10px',
              color: FINCEPT.WHITE,
              padding: '4px',
              backgroundColor: 'rgba(255,59,59,0.1)',
              borderRadius: '2px'
            }}>
              [WARN] <strong>LOW SHARPE RATIO:</strong> Consider diversifying to improve risk-adjusted returns
            </div>
          )}
          {beta > 1.5 && (
            <div style={{
              fontSize: '10px',
              color: FINCEPT.WHITE,
              padding: '4px',
              backgroundColor: 'rgba(255,136,0,0.1)',
              borderRadius: '2px'
            }}>
              [WARN] <strong>HIGH BETA:</strong> Portfolio is more volatile than market. Consider defensive assets
            </div>
          )}
          {volatility > 25 && (
            <div style={{
              fontSize: '10px',
              color: FINCEPT.WHITE,
              padding: '4px',
              backgroundColor: 'rgba(255,59,59,0.1)',
              borderRadius: '2px'
            }}>
              [WARN] <strong>HIGH VOLATILITY:</strong> Consider adding bonds or stable assets to reduce swings
            </div>
          )}
          {maxDrawdown > 20 && (
            <div style={{
              fontSize: '10px',
              color: FINCEPT.WHITE,
              padding: '4px',
              backgroundColor: 'rgba(255,59,59,0.1)',
              borderRadius: '2px'
            }}>
              [WARN] <strong>LARGE DRAWDOWN:</strong> Review stop-loss strategies and position sizing
            </div>
          )}
          {sharpeRatio > 2 && (
            <div style={{
              fontSize: '10px',
              color: FINCEPT.WHITE,
              padding: '4px',
              backgroundColor: 'rgba(0,214,111,0.1)',
              borderRadius: '2px'
            }}>
              [OK] <strong>EXCELLENT SHARPE:</strong> Portfolio shows strong risk-adjusted performance
            </div>
          )}
          {beta >= 0.8 && beta <= 1.2 && (
            <div style={{
              fontSize: '10px',
              color: FINCEPT.WHITE,
              padding: '4px',
              backgroundColor: 'rgba(0,229,255,0.1)',
              borderRadius: '2px'
            }}>
              [OK] <strong>BALANCED BETA:</strong> Good market correlation, neither too aggressive nor defensive
            </div>
          )}
        </div>
      </div>

      {/* Disclaimer */}
      <div style={{
        marginTop: '12px',
        padding: '8px',
        backgroundColor: FINCEPT.HOVER,
        border: BORDERS.STANDARD,
        borderRadius: '2px',
        fontSize: '9px',
        color: FINCEPT.GRAY
      }}>
        <strong style={{ color: FINCEPT.ORANGE }}>DISCLAIMER:</strong> Risk metrics are calculated from portfolio snapshot history.
        {hasInsufficientData && ' More data points are needed for statistically significant results.'}
        {' '}Beta calculations use portfolio returns as market proxy until real market benchmarks are integrated.
        Past performance does not guarantee future results. Consult a financial advisor before making investment decisions.
      </div>
    </div>
  );
};

export default RiskMetricsView;
