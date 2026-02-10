/**
 * ResultMetrics - Categorized metrics list for backtest results
 */

import React from 'react';
import { FINCEPT, FONT_FAMILY } from '../constants';
import { formatMetricValue, getMetricColor, getMetricDescription } from './metricUtils';

interface ResultMetricsProps {
  result: any;
}

const METRIC_CATEGORIES: Record<string, string[]> = {
  'Returns': ['totalReturn', 'annualizedReturn', 'CAGR', 'expectancy', 'buyAndHoldReturn', 'exposureTime'],
  'Risk Metrics': ['sharpeRatio', 'sortinoRatio', 'calmarRatio', 'volatility', 'maxDrawdown'],
  'Value at Risk': ['VaR', 'CVaR', 'tailRatio', 'commonSenseRatio'],
  'Capture Ratios': ['upCapture', 'downCapture'],
  'Trade Statistics': ['totalTrades', 'winningTrades', 'losingTrades', 'winRate', 'lossRate', 'profitFactor'],
  'Trade Performance': ['averageWin', 'averageLoss', 'largestWin', 'largestLoss', 'averageTradeReturn'],
  'Extended Stats': ['SQN', 'kellyCriterion', 'payoffRatio', 'skewness', 'kurtosis'],
  'Drawdown Detail': ['avgDrawdown', 'maxDrawdownDuration', 'avgDrawdownDuration', 'avgRecoveryDuration', 'activeDrawdown'],
  'Advanced Metrics': ['alpha', 'beta', 'informationRatio', 'treynorRatio'],
};

export const ResultMetrics: React.FC<ResultMetricsProps> = ({ result }) => {
  if (!result?.data?.performance) return null;
  const perf = result.data.performance;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {Object.entries(METRIC_CATEGORIES).map(([category, metrics]) => {
        const availableMetrics = metrics.filter(key => key in perf);

        // Skip optional categories if all values are null/undefined
        const OPTIONAL_CATEGORIES = ['Advanced Metrics', 'Value at Risk', 'Capture Ratios', 'Extended Stats', 'Drawdown Detail'];
        if (OPTIONAL_CATEGORIES.includes(category)) {
          const hasData = availableMetrics.some(key =>
            perf[key as keyof typeof perf] !== null &&
            perf[key as keyof typeof perf] !== undefined
          );
          if (!hasData) return null;
        }

        return (
          <div key={category}>
            {/* Category Header */}
            <div style={{
              fontSize: '8px',
              fontWeight: 700,
              color: FINCEPT.ORANGE,
              marginBottom: '8px',
              paddingBottom: '4px',
              borderBottom: `2px solid ${FINCEPT.ORANGE}40`,
              letterSpacing: '0.5px',
              fontFamily: FONT_FAMILY,
            }}>
              {category.toUpperCase()}
            </div>

            {/* Metrics List */}
            <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
              {availableMetrics.map(key => {
                const value = perf[key as keyof typeof perf];

                if ((value === null || value === undefined) && OPTIONAL_CATEGORIES.includes(category)) {
                  return null;
                }

                return (
                  <div key={key} style={{
                    padding: '6px 8px',
                    backgroundColor: FINCEPT.PANEL_BG,
                    border: `1px solid ${FINCEPT.BORDER}`,
                    borderRadius: '2px',
                    display: 'flex',
                    flexDirection: 'column',
                    gap: '2px',
                  }}>
                    <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                      <span style={{
                        color: FINCEPT.WHITE,
                        fontSize: '8px',
                        fontWeight: 600,
                        fontFamily: FONT_FAMILY,
                      }}>
                        {key.replace(/([A-Z])/g, ' $1').trim().split(' ').map(w =>
                          w.charAt(0).toUpperCase() + w.slice(1)
                        ).join(' ')}
                      </span>
                      <span style={{
                        color: getMetricColor(key, value),
                        fontFamily: FONT_FAMILY,
                        fontSize: '10px',
                        fontWeight: 700,
                      }}>
                        {formatMetricValue(key, value)}
                      </span>
                    </div>
                    <div style={{
                      fontSize: '7px',
                      color: FINCEPT.MUTED,
                      lineHeight: 1.3,
                      fontFamily: FONT_FAMILY,
                    }}>
                      {getMetricDescription(key)}
                    </div>
                  </div>
                );
              })}
            </div>
          </div>
        );
      })}

      {/* Note about missing advanced metrics */}
      {(!perf.alpha && !perf.beta && !perf.informationRatio && !perf.treynorRatio) && (
        <div style={{
          padding: '8px',
          backgroundColor: `${FINCEPT.GRAY}10`,
          border: `1px solid ${FINCEPT.GRAY}40`,
          borderRadius: '2px',
          fontSize: '7px',
          color: FINCEPT.MUTED,
          lineHeight: 1.4,
          fontFamily: FONT_FAMILY,
        }}>
          <div style={{ color: FINCEPT.GRAY, fontWeight: 600, marginBottom: '4px' }}>
            BENCHMARK METRICS
          </div>
          Advanced metrics (Alpha, Beta, Information Ratio, Treynor Ratio) require a benchmark symbol.
          Enable benchmark in settings and run again to see these metrics.
        </div>
      )}
    </div>
  );
};
