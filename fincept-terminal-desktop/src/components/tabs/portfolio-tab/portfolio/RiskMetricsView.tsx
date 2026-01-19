import React, { useState, useEffect } from 'react';
import { PortfolioSummary, portfolioService } from '../../../../services/portfolio/portfolioService';
import {
  FINCEPT_COLORS,
  formatPercent,
  calculateSharpeRatio,
  calculateBeta,
  calculateVolatility,
  calculateMaxDrawdown,
  calculateVaR
} from './utils';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../finceptStyles';

interface RiskMetricsViewProps {
  portfolioSummary: PortfolioSummary;
}

const RiskMetricsView: React.FC<RiskMetricsViewProps> = ({ portfolioSummary }) => {
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
        padding: SPACING.XLARGE,
        textAlign: 'center',
        fontFamily: TYPOGRAPHY.MONO
      }}>
        <div style={{ color: FINCEPT.ORANGE, fontSize: TYPOGRAPHY.SUBHEADING, marginBottom: SPACING.SMALL }}>
          Loading risk metrics...
        </div>
        <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.BODY }}>
          Calculating portfolio risk from historical data
        </div>
      </div>
    );
  }

  return (
    <div style={{
      height: '100%',
      backgroundColor: FINCEPT.DARK_BG,
      padding: SPACING.DEFAULT,
      overflow: 'auto',
      fontFamily: TYPOGRAPHY.MONO
    }}>
      <div style={{
        ...COMMON_STYLES.sectionHeader,
        marginBottom: SPACING.LARGE
      }}>
        RISK METRICS & ANALYTICS {hasInsufficientData && '(LIMITED DATA)'}
      </div>

      {hasInsufficientData && (
        <div style={{
          padding: SPACING.MEDIUM,
          marginBottom: SPACING.DEFAULT,
          backgroundColor: 'rgba(255,136,0,0.1)',
          border: BORDERS.ORANGE,
          fontSize: TYPOGRAPHY.BODY,
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
        gap: SPACING.DEFAULT,
        marginBottom: SPACING.XLARGE
      }}>
        {/* Overall Risk Level */}
        <div style={{
          padding: SPACING.DEFAULT,
          backgroundColor: FINCEPT.PANEL_BG,
          border: `2px solid ${riskLevel.color}`
        }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.SMALL, marginBottom: SPACING.SMALL }}>OVERALL RISK</div>
          <div style={{
            color: riskLevel.color,
            fontSize: TYPOGRAPHY.LARGE,
            fontWeight: TYPOGRAPHY.BOLD,
            marginBottom: SPACING.TINY
          }}>
            {riskLevel.level}
          </div>
          <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY }}>
            Based on Sharpe Ratio: {sharpeRatio.toFixed(2)}
          </div>
        </div>

        {/* Beta Rating */}
        <div style={{
          padding: SPACING.DEFAULT,
          backgroundColor: FINCEPT.PANEL_BG,
          border: `2px solid ${betaRating.color}`
        }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.SMALL, marginBottom: SPACING.SMALL }}>MARKET SENSITIVITY</div>
          <div style={{
            color: betaRating.color,
            fontSize: TYPOGRAPHY.LARGE,
            fontWeight: TYPOGRAPHY.BOLD,
            marginBottom: SPACING.TINY
          }}>
            {betaRating.rating}
          </div>
          <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY }}>
            Beta: {beta.toFixed(2)}
          </div>
        </div>

        {/* Volatility Rating */}
        <div style={{
          padding: SPACING.DEFAULT,
          backgroundColor: FINCEPT.PANEL_BG,
          border: `2px solid ${volatilityRating.color}`
        }}>
          <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.SMALL, marginBottom: SPACING.SMALL }}>VOLATILITY</div>
          <div style={{
            color: volatilityRating.color,
            fontSize: TYPOGRAPHY.LARGE,
            fontWeight: TYPOGRAPHY.BOLD,
            marginBottom: SPACING.TINY
          }}>
            {volatilityRating.rating}
          </div>
          <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY }}>
            Annualized: {volatility.toFixed(2)}%
          </div>
        </div>
      </div>

      {/* Detailed Metrics Grid */}
      <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.DEFAULT, marginBottom: SPACING.XLARGE }}>
        {/* Left Column - Return Metrics */}
        <div>
          <div style={{
            color: FINCEPT.ORANGE,
            fontSize: TYPOGRAPHY.DEFAULT,
            fontWeight: TYPOGRAPHY.BOLD,
            marginBottom: SPACING.MEDIUM,
            paddingBottom: SPACING.SMALL,
            borderBottom: BORDERS.ORANGE
          }}>
            RISK-ADJUSTED RETURNS
          </div>

          {/* Sharpe Ratio */}
          <div style={{
            padding: SPACING.MEDIUM,
            marginBottom: SPACING.MEDIUM,
            backgroundColor: FINCEPT.PANEL_BG,
            border: BORDERS.STANDARD
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.DEFAULT, fontWeight: TYPOGRAPHY.BOLD }}>SHARPE RATIO</div>
                <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY, marginTop: SPACING.TINY }}>
                  Risk-adjusted return measure
                </div>
              </div>
              <div style={{
                color: sharpeRatio > 1 ? FINCEPT.GREEN : sharpeRatio > 0 ? FINCEPT.YELLOW : FINCEPT.RED,
                fontSize: TYPOGRAPHY.LARGE,
                fontWeight: TYPOGRAPHY.BOLD
              }}>
                {sharpeRatio.toFixed(3)}
              </div>
            </div>
            <div style={{ marginTop: SPACING.SMALL, fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
              {'> 2: Excellent | 1-2: Good | 0-1: Fair | < 0: Poor'}
            </div>
          </div>

          {/* Beta */}
          <div style={{
            padding: SPACING.MEDIUM,
            marginBottom: SPACING.MEDIUM,
            backgroundColor: FINCEPT.PANEL_BG,
            border: BORDERS.STANDARD
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.DEFAULT, fontWeight: TYPOGRAPHY.BOLD }}>BETA (β)</div>
                <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY, marginTop: SPACING.TINY }}>
                  Market correlation coefficient
                </div>
              </div>
              <div style={{ color: betaRating.color, fontSize: TYPOGRAPHY.LARGE, fontWeight: TYPOGRAPHY.BOLD }}>
                {beta.toFixed(3)}
              </div>
            </div>
            <div style={{ marginTop: SPACING.SMALL, fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
              {'< 1: Less volatile | 1: Market neutral | > 1: More volatile'}
            </div>
          </div>

          {/* Volatility */}
          <div style={{
            padding: SPACING.MEDIUM,
            backgroundColor: FINCEPT.PANEL_BG,
            border: BORDERS.STANDARD
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.DEFAULT, fontWeight: TYPOGRAPHY.BOLD }}>VOLATILITY</div>
                <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY, marginTop: SPACING.TINY }}>
                  Annualized standard deviation
                </div>
              </div>
              <div style={{ color: volatilityRating.color, fontSize: TYPOGRAPHY.LARGE, fontWeight: TYPOGRAPHY.BOLD }}>
                {volatility.toFixed(2)}%
              </div>
            </div>
            <div style={{ marginTop: SPACING.SMALL, fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
              Lower volatility indicates more stable returns
            </div>
          </div>
        </div>

        {/* Right Column - Risk Metrics */}
        <div>
          <div style={{
            color: FINCEPT.ORANGE,
            fontSize: TYPOGRAPHY.DEFAULT,
            fontWeight: TYPOGRAPHY.BOLD,
            marginBottom: SPACING.MEDIUM,
            paddingBottom: SPACING.SMALL,
            borderBottom: BORDERS.ORANGE
          }}>
            DOWNSIDE RISK METRICS
          </div>

          {/* Maximum Drawdown */}
          <div style={{
            padding: SPACING.MEDIUM,
            marginBottom: SPACING.MEDIUM,
            backgroundColor: FINCEPT.PANEL_BG,
            border: BORDERS.STANDARD
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.DEFAULT, fontWeight: TYPOGRAPHY.BOLD }}>MAX DRAWDOWN</div>
                <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY, marginTop: SPACING.TINY }}>
                  Largest peak-to-trough decline
                </div>
              </div>
              <div style={{
                color: maxDrawdown > 20 ? FINCEPT.RED : maxDrawdown > 10 ? FINCEPT.ORANGE : FINCEPT.GREEN,
                fontSize: TYPOGRAPHY.LARGE,
                fontWeight: TYPOGRAPHY.BOLD
              }}>
                -{maxDrawdown.toFixed(2)}%
              </div>
            </div>
            <div style={{ marginTop: SPACING.SMALL, fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
              Maximum loss from peak value
            </div>
          </div>

          {/* Value at Risk */}
          <div style={{
            padding: SPACING.MEDIUM,
            marginBottom: SPACING.MEDIUM,
            backgroundColor: FINCEPT.PANEL_BG,
            border: BORDERS.STANDARD
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{ color: FINCEPT.CYAN, fontSize: TYPOGRAPHY.DEFAULT, fontWeight: TYPOGRAPHY.BOLD }}>VALUE AT RISK (95%)</div>
                <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY, marginTop: SPACING.TINY }}>
                  Expected loss at 95% confidence
                </div>
              </div>
              <div style={{
                color: var95 < -0.03 ? FINCEPT.RED : var95 < -0.02 ? FINCEPT.ORANGE : FINCEPT.YELLOW,
                fontSize: TYPOGRAPHY.LARGE,
                fontWeight: TYPOGRAPHY.BOLD
              }}>
                {(var95 * 100).toFixed(2)}%
              </div>
            </div>
            <div style={{ marginTop: SPACING.SMALL, fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
              5% chance of losing more than this in a day
            </div>
          </div>

          {/* Risk Score */}
          <div style={{
            padding: SPACING.MEDIUM,
            backgroundColor: 'rgba(255,59,59,0.05)',
            border: `2px solid ${riskLevel.color}`
          }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
              <div>
                <div style={{ color: riskLevel.color, fontSize: TYPOGRAPHY.DEFAULT, fontWeight: TYPOGRAPHY.BOLD }}>COMPOSITE RISK SCORE</div>
                <div style={{ color: FINCEPT.GRAY, fontSize: TYPOGRAPHY.TINY, marginTop: SPACING.TINY }}>
                  Weighted risk assessment
                </div>
              </div>
              <div style={{ color: riskLevel.color, fontSize: TYPOGRAPHY.LARGE, fontWeight: TYPOGRAPHY.BOLD }}>
                {((1 / (sharpeRatio + 1)) * 100).toFixed(0)}/100
              </div>
            </div>
            <div style={{ marginTop: SPACING.SMALL }}>
              <div style={{
                width: '100%',
                height: '8px',
                backgroundColor: FINCEPT.HOVER,
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
        padding: SPACING.DEFAULT,
        backgroundColor: 'rgba(255,136,0,0.05)',
        border: BORDERS.ORANGE
      }}>
        <div style={{
          color: FINCEPT.ORANGE,
          fontSize: TYPOGRAPHY.DEFAULT,
          fontWeight: TYPOGRAPHY.BOLD,
          marginBottom: SPACING.MEDIUM
        }}>
          RISK MANAGEMENT RECOMMENDATIONS
        </div>

        <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr', gap: SPACING.MEDIUM }}>
          {sharpeRatio < 1 && (
            <div style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.WHITE, padding: SPACING.SMALL, backgroundColor: 'rgba(255,59,59,0.1)' }}>
              [WARN] <strong>LOW SHARPE RATIO:</strong> Consider diversifying to improve risk-adjusted returns
            </div>
          )}
          {beta > 1.5 && (
            <div style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.WHITE, padding: SPACING.SMALL, backgroundColor: 'rgba(255,136,0,0.1)' }}>
              [WARN] <strong>HIGH BETA:</strong> Portfolio is more volatile than market. Consider defensive assets
            </div>
          )}
          {volatility > 25 && (
            <div style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.WHITE, padding: SPACING.SMALL, backgroundColor: 'rgba(255,59,59,0.1)' }}>
              [WARN] <strong>HIGH VOLATILITY:</strong> Consider adding bonds or stable assets to reduce swings
            </div>
          )}
          {maxDrawdown > 20 && (
            <div style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.WHITE, padding: SPACING.SMALL, backgroundColor: 'rgba(255,59,59,0.1)' }}>
              [WARN] <strong>LARGE DRAWDOWN:</strong> Review stop-loss strategies and position sizing
            </div>
          )}
          {sharpeRatio > 2 && (
            <div style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.WHITE, padding: SPACING.SMALL, backgroundColor: 'rgba(0,214,111,0.1)' }}>
              [OK] <strong>EXCELLENT SHARPE:</strong> Portfolio shows strong risk-adjusted performance
            </div>
          )}
          {beta >= 0.8 && beta <= 1.2 && (
            <div style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.WHITE, padding: SPACING.SMALL, backgroundColor: 'rgba(0,229,255,0.1)' }}>
              [OK] <strong>BALANCED BETA:</strong> Good market correlation, neither too aggressive nor defensive
            </div>
          )}
        </div>
      </div>

      {/* Disclaimer */}
      <div style={{
        marginTop: SPACING.DEFAULT,
        padding: SPACING.MEDIUM,
        backgroundColor: FINCEPT.HOVER,
        border: BORDERS.STANDARD,
        fontSize: TYPOGRAPHY.TINY,
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
