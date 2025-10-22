import React, { useState, useEffect } from 'react';
import { PortfolioSummary, portfolioService } from '../../../services/portfolioService';
import {
  BLOOMBERG_COLORS,
  formatPercent,
  calculateSharpeRatio,
  calculateBeta,
  calculateVolatility,
  calculateMaxDrawdown,
  calculateVaR
} from './utils';

interface RiskMetricsViewProps {
  portfolioSummary: PortfolioSummary;
}

const RiskMetricsView: React.FC<RiskMetricsViewProps> = ({ portfolioSummary }) => {
  const { ORANGE, WHITE, RED, GREEN, GRAY, CYAN, YELLOW, BLUE } = BLOOMBERG_COLORS;
  const [loading, setLoading] = useState(true);
  const [portfolioReturns, setPortfolioReturns] = useState<number[]>([]);
  const [portfolioValues, setPortfolioValues] = useState<number[]>([]);

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
    if (sharpe > 2) return { level: 'LOW', color: GREEN };
    if (sharpe > 1) return { level: 'MODERATE', color: YELLOW };
    if (sharpe > 0) return { level: 'HIGH', color: ORANGE };
    return { level: 'VERY HIGH', color: RED };
  };

  const riskLevel = getRiskLevel(sharpeRatio);

  const getBetaRating = (beta: number) => {
    if (beta < 0.8) return { rating: 'DEFENSIVE', color: BLUE };
    if (beta < 1.2) return { rating: 'NEUTRAL', color: CYAN };
    return { rating: 'AGGRESSIVE', color: ORANGE };
  };

  const betaRating = getBetaRating(beta);

  const getVolatilityRating = (vol: number) => {
    if (vol < 10) return { rating: 'LOW', color: GREEN };
    if (vol < 20) return { rating: 'MODERATE', color: YELLOW };
    if (vol < 30) return { rating: 'HIGH', color: ORANGE };
    return { rating: 'VERY HIGH', color: RED };
  };

  const volatilityRating = getVolatilityRating(volatility);

  if (loading) {
    return (
      <div style={{
        padding: '40px',
        textAlign: 'center',
        backgroundColor: 'rgba(255,255,255,0.02)',
        border: `1px solid ${GRAY}`,
        borderRadius: '4px'
      }}>
        <div style={{ color: ORANGE, fontSize: '14px', marginBottom: '8px' }}>
          Loading risk metrics...
        </div>
        <div style={{ color: GRAY, fontSize: '10px' }}>
          Calculating portfolio risk from historical data
        </div>
      </div>
    );
  }

  return (
    <div>
      <div style={{
        color: ORANGE,
        fontSize: '12px',
        fontWeight: 'bold',
        marginBottom: '16px'
      }}>
        RISK METRICS & ANALYTICS {hasInsufficientData && '(LIMITED DATA)'}
      </div>

      {hasInsufficientData && (
        <div style={{
          padding: '12px',
          marginBottom: '16px',
          backgroundColor: 'rgba(255,165,0,0.1)',
          border: `1px solid ${ORANGE}`,
          borderRadius: '4px',
          fontSize: '10px',
          color: ORANGE
        }}>
          ⚠️ <strong>INSUFFICIENT HISTORICAL DATA:</strong> Risk metrics require at least 5 days of portfolio snapshots for accurate calculations.
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
        <div style={{
          padding: '16px',
          backgroundColor: 'rgba(255,165,0,0.1)',
          border: `2px solid ${riskLevel.color}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '8px' }}>OVERALL RISK</div>
          <div style={{
            color: riskLevel.color,
            fontSize: '20px',
            fontWeight: 'bold',
            marginBottom: '4px'
          }}>
            {riskLevel.level}
          </div>
          <div style={{ color: GRAY, fontSize: '8px' }}>
            Based on Sharpe Ratio: {sharpeRatio.toFixed(2)}
          </div>
        </div>

        {/* Beta Rating */}
        <div style={{
          padding: '16px',
          backgroundColor: 'rgba(255,255,255,0.02)',
          border: `2px solid ${betaRating.color}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '8px' }}>MARKET SENSITIVITY</div>
          <div style={{
            color: betaRating.color,
            fontSize: '20px',
            fontWeight: 'bold',
            marginBottom: '4px'
          }}>
            {betaRating.rating}
          </div>
          <div style={{ color: GRAY, fontSize: '8px' }}>
            Beta: {beta.toFixed(2)}
          </div>
        </div>

        {/* Volatility Rating */}
        <div style={{
          padding: '16px',
          backgroundColor: 'rgba(255,255,255,0.02)',
          border: `2px solid ${volatilityRating.color}`,
          borderRadius: '4px'
        }}>
          <div style={{ color: GRAY, fontSize: '9px', marginBottom: '8px' }}>VOLATILITY</div>
          <div style={{
            color: volatilityRating.color,
            fontSize: '20px',
            fontWeight: 'bold',
            marginBottom: '4px'
          }}>
            {volatilityRating.rating}
          </div>
          <div style={{ color: GRAY, fontSize: '8px' }}>
            Annualized: {volatility.toFixed(2)}%
          </div>
        </div>
      </div>

      {/* Detailed Metrics Grid */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '16px', marginBottom: '24px' }}>
        {/* Left Column - Return Metrics */}
        <div>
          <div style={{
            color: ORANGE,
            fontSize: '11px',
            fontWeight: 'bold',
            marginBottom: '12px',
            paddingBottom: '4px',
            borderBottom: `1px solid ${ORANGE}`
          }}>
            RISK-ADJUSTED RETURNS
          </div>

          {/* Sharpe Ratio */}
          <div style={{
            padding: '12px',
            marginBottom: '12px',
            backgroundColor: 'rgba(255,255,255,0.02)',
            border: `1px solid ${GRAY}`,
            borderRadius: '4px'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{ color: CYAN, fontSize: '11px', fontWeight: 'bold' }}>SHARPE RATIO</div>
                <div style={{ color: GRAY, fontSize: '8px', marginTop: '2px' }}>
                  Risk-adjusted return measure
                </div>
              </div>
              <div style={{
                color: sharpeRatio > 1 ? GREEN : sharpeRatio > 0 ? YELLOW : RED,
                fontSize: '18px',
                fontWeight: 'bold'
              }}>
                {sharpeRatio.toFixed(3)}
              </div>
            </div>
            <div style={{ marginTop: '8px', fontSize: '8px', color: GRAY }}>
              {'> 2: Excellent | 1-2: Good | 0-1: Fair | < 0: Poor'}
            </div>
          </div>

          {/* Beta */}
          <div style={{
            padding: '12px',
            marginBottom: '12px',
            backgroundColor: 'rgba(255,255,255,0.02)',
            border: `1px solid ${GRAY}`,
            borderRadius: '4px'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{ color: CYAN, fontSize: '11px', fontWeight: 'bold' }}>BETA (β)</div>
                <div style={{ color: GRAY, fontSize: '8px', marginTop: '2px' }}>
                  Market correlation coefficient
                </div>
              </div>
              <div style={{ color: betaRating.color, fontSize: '18px', fontWeight: 'bold' }}>
                {beta.toFixed(3)}
              </div>
            </div>
            <div style={{ marginTop: '8px', fontSize: '8px', color: GRAY }}>
              {'< 1: Less volatile | 1: Market neutral | > 1: More volatile'}
            </div>
          </div>

          {/* Volatility */}
          <div style={{
            padding: '12px',
            backgroundColor: 'rgba(255,255,255,0.02)',
            border: `1px solid ${GRAY}`,
            borderRadius: '4px'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{ color: CYAN, fontSize: '11px', fontWeight: 'bold' }}>VOLATILITY</div>
                <div style={{ color: GRAY, fontSize: '8px', marginTop: '2px' }}>
                  Annualized standard deviation
                </div>
              </div>
              <div style={{ color: volatilityRating.color, fontSize: '18px', fontWeight: 'bold' }}>
                {volatility.toFixed(2)}%
              </div>
            </div>
            <div style={{ marginTop: '8px', fontSize: '8px', color: GRAY }}>
              Lower volatility indicates more stable returns
            </div>
          </div>
        </div>

        {/* Right Column - Risk Metrics */}
        <div>
          <div style={{
            color: ORANGE,
            fontSize: '11px',
            fontWeight: 'bold',
            marginBottom: '12px',
            paddingBottom: '4px',
            borderBottom: `1px solid ${ORANGE}`
          }}>
            DOWNSIDE RISK METRICS
          </div>

          {/* Maximum Drawdown */}
          <div style={{
            padding: '12px',
            marginBottom: '12px',
            backgroundColor: 'rgba(255,255,255,0.02)',
            border: `1px solid ${GRAY}`,
            borderRadius: '4px'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{ color: CYAN, fontSize: '11px', fontWeight: 'bold' }}>MAX DRAWDOWN</div>
                <div style={{ color: GRAY, fontSize: '8px', marginTop: '2px' }}>
                  Largest peak-to-trough decline
                </div>
              </div>
              <div style={{
                color: maxDrawdown > 20 ? RED : maxDrawdown > 10 ? ORANGE : GREEN,
                fontSize: '18px',
                fontWeight: 'bold'
              }}>
                -{maxDrawdown.toFixed(2)}%
              </div>
            </div>
            <div style={{ marginTop: '8px', fontSize: '8px', color: GRAY }}>
              Maximum loss from peak value
            </div>
          </div>

          {/* Value at Risk */}
          <div style={{
            padding: '12px',
            marginBottom: '12px',
            backgroundColor: 'rgba(255,255,255,0.02)',
            border: `1px solid ${GRAY}`,
            borderRadius: '4px'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{ color: CYAN, fontSize: '11px', fontWeight: 'bold' }}>VALUE AT RISK (95%)</div>
                <div style={{ color: GRAY, fontSize: '8px', marginTop: '2px' }}>
                  Expected loss at 95% confidence
                </div>
              </div>
              <div style={{
                color: var95 < -0.03 ? RED : var95 < -0.02 ? ORANGE : YELLOW,
                fontSize: '18px',
                fontWeight: 'bold'
              }}>
                {(var95 * 100).toFixed(2)}%
              </div>
            </div>
            <div style={{ marginTop: '8px', fontSize: '8px', color: GRAY }}>
              5% chance of losing more than this in a day
            </div>
          </div>

          {/* Risk Score */}
          <div style={{
            padding: '12px',
            backgroundColor: 'rgba(255,0,0,0.05)',
            border: `2px solid ${riskLevel.color}`,
            borderRadius: '4px'
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{ color: riskLevel.color, fontSize: '11px', fontWeight: 'bold' }}>COMPOSITE RISK SCORE</div>
                <div style={{ color: GRAY, fontSize: '8px', marginTop: '2px' }}>
                  Weighted risk assessment
                </div>
              </div>
              <div style={{ color: riskLevel.color, fontSize: '18px', fontWeight: 'bold' }}>
                {((1 / (sharpeRatio + 1)) * 100).toFixed(0)}/100
              </div>
            </div>
            <div style={{ marginTop: '8px' }}>
              <div style={{
                width: '100%',
                height: '8px',
                backgroundColor: 'rgba(120,120,120,0.3)',
                borderRadius: '4px',
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
        padding: '16px',
        backgroundColor: 'rgba(255,165,0,0.05)',
        border: `1px solid ${ORANGE}`,
        borderRadius: '4px'
      }}>
        <div style={{
          color: ORANGE,
          fontSize: '11px',
          fontWeight: 'bold',
          marginBottom: '12px'
        }}>
          RISK MANAGEMENT RECOMMENDATIONS
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: '12px' }}>
          {sharpeRatio < 1 && (
            <div style={{ fontSize: '9px', color: WHITE, padding: '8px', backgroundColor: 'rgba(255,0,0,0.1)', borderRadius: '2px' }}>
              ⚠️ <strong>LOW SHARPE RATIO:</strong> Consider diversifying to improve risk-adjusted returns
            </div>
          )}
          {beta > 1.5 && (
            <div style={{ fontSize: '9px', color: WHITE, padding: '8px', backgroundColor: 'rgba(255,165,0,0.1)', borderRadius: '2px' }}>
              ⚠️ <strong>HIGH BETA:</strong> Portfolio is more volatile than market. Consider defensive assets
            </div>
          )}
          {volatility > 25 && (
            <div style={{ fontSize: '9px', color: WHITE, padding: '8px', backgroundColor: 'rgba(255,0,0,0.1)', borderRadius: '2px' }}>
              ⚠️ <strong>HIGH VOLATILITY:</strong> Consider adding bonds or stable assets to reduce swings
            </div>
          )}
          {maxDrawdown > 20 && (
            <div style={{ fontSize: '9px', color: WHITE, padding: '8px', backgroundColor: 'rgba(255,0,0,0.1)', borderRadius: '2px' }}>
              ⚠️ <strong>LARGE DRAWDOWN:</strong> Review stop-loss strategies and position sizing
            </div>
          )}
          {sharpeRatio > 2 && (
            <div style={{ fontSize: '9px', color: WHITE, padding: '8px', backgroundColor: 'rgba(0,200,0,0.1)', borderRadius: '2px' }}>
              ✓ <strong>EXCELLENT SHARPE:</strong> Portfolio shows strong risk-adjusted performance
            </div>
          )}
          {beta >= 0.8 && beta <= 1.2 && (
            <div style={{ fontSize: '9px', color: WHITE, padding: '8px', backgroundColor: 'rgba(0,255,255,0.1)', borderRadius: '2px' }}>
              ✓ <strong>BALANCED BETA:</strong> Good market correlation, neither too aggressive nor defensive
            </div>
          )}
        </div>
      </div>

      {/* Disclaimer */}
      <div style={{
        marginTop: '16px',
        padding: '12px',
        backgroundColor: 'rgba(120,120,120,0.1)',
        border: `1px solid ${GRAY}`,
        borderRadius: '4px',
        fontSize: '8px',
        color: GRAY
      }}>
        <strong style={{ color: ORANGE }}>DISCLAIMER:</strong> Risk metrics are calculated from portfolio snapshot history.
        {hasInsufficientData && ' More data points are needed for statistically significant results.'}
        {' '}Beta calculations use portfolio returns as market proxy until real market benchmarks are integrated.
        Past performance does not guarantee future results. Consult a financial advisor before making investment decisions.
      </div>
    </div>
  );
};

export default RiskMetricsView;
