/**
 * ResultDrawdowns - Detailed drawdown metrics view
 * Shows all 23 drawdown-related metrics from backend
 */

import React from 'react';
import { FINCEPT, FONT_FAMILY } from '../constants';
import { formatMetricValue, getMetricColor } from './metricUtils';

interface ResultDrawdownsProps {
  result: any;
}

const DRAWDOWN_METRICS: { key: string; label: string; description: string }[] = [
  { key: 'maxDrawdown', label: 'Max Drawdown', description: 'Largest peak-to-trough decline' },
  { key: 'maxDrawdownDuration', label: 'Max DD Duration', description: 'Longest time in drawdown (days)' },
  { key: 'avgDrawdown', label: 'Avg Drawdown', description: 'Average drawdown depth' },
  { key: 'avgDrawdownDuration', label: 'Avg DD Duration', description: 'Average drawdown duration (days)' },
  { key: 'avgRecoveryDuration', label: 'Avg Recovery', description: 'Average time to recover (days)' },
  { key: 'activeDrawdown', label: 'Active Drawdown', description: 'Current unrecovered drawdown' },
  { key: 'calmarRatio', label: 'Calmar Ratio', description: 'Annualized return / max drawdown' },
  { key: 'ulcerIndex', label: 'Ulcer Index', description: 'Root mean square of drawdowns' },
  { key: 'painIndex', label: 'Pain Index', description: 'Mean drawdown over period' },
  { key: 'painRatio', label: 'Pain Ratio', description: 'Return / pain index' },
  { key: 'martinRatio', label: 'Martin Ratio', description: 'Return / ulcer index' },
  { key: 'burkeFactor', label: 'Burke Factor', description: 'Return adjusted by DD variability' },
  { key: 'drawdownRecoveryRatio', label: 'DD Recovery Ratio', description: 'Avg recovery / avg drawdown time' },
  { key: 'maxConsecutiveLosses', label: 'Max Consecutive Losses', description: 'Longest losing streak' },
  { key: 'maxConsecutiveWins', label: 'Max Consecutive Wins', description: 'Longest winning streak' },
];

export const ResultDrawdowns: React.FC<ResultDrawdownsProps> = ({ result }) => {
  const perf = result?.data?.performance;
  const drawdowns = result?.data?.drawdowns;

  if (!perf && !drawdowns) {
    return (
      <div style={{
        padding: '16px',
        color: FINCEPT.MUTED,
        fontSize: '9px',
        fontFamily: FONT_FAMILY,
        textAlign: 'center',
      }}>
        No drawdown data available.
      </div>
    );
  }

  const data = { ...perf, ...drawdowns };

  // Filter to metrics that exist in the data
  const availableMetrics = DRAWDOWN_METRICS.filter(m => data[m.key] !== undefined && data[m.key] !== null);

  // Max drawdown highlight
  const maxDD = data.maxDrawdown;

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Max Drawdown Highlight */}
      {maxDD !== undefined && (
        <div style={{
          padding: '12px',
          backgroundColor: `${FINCEPT.RED}15`,
          border: `1px solid ${FINCEPT.RED}40`,
          borderRadius: '2px',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
        }}>
          <div>
            <div style={{
              fontSize: '8px',
              fontWeight: 700,
              color: FINCEPT.RED,
              letterSpacing: '0.5px',
              fontFamily: FONT_FAMILY,
            }}>
              MAX DRAWDOWN
            </div>
            <div style={{
              fontSize: '7px',
              color: FINCEPT.MUTED,
              marginTop: '2px',
              fontFamily: FONT_FAMILY,
            }}>
              Largest peak-to-trough decline
            </div>
          </div>
          <span style={{
            fontSize: '14px',
            fontWeight: 700,
            color: FINCEPT.RED,
            fontFamily: FONT_FAMILY,
          }}>
            {formatMetricValue('maxDrawdown', maxDD)}
          </span>
        </div>
      )}

      {/* Metrics List */}
      <div style={{
        fontSize: '8px',
        fontWeight: 700,
        color: FINCEPT.ORANGE,
        paddingBottom: '4px',
        borderBottom: `2px solid ${FINCEPT.ORANGE}40`,
        letterSpacing: '0.5px',
        fontFamily: FONT_FAMILY,
      }}>
        DRAWDOWN METRICS
      </div>

      <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
        {availableMetrics.map(metric => (
          <div key={metric.key} style={{
            padding: '6px 8px',
            backgroundColor: FINCEPT.PANEL_BG,
            border: `1px solid ${FINCEPT.BORDER}`,
            borderRadius: '2px',
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
          }}>
            <div>
              <span style={{
                color: FINCEPT.WHITE,
                fontSize: '8px',
                fontWeight: 600,
                fontFamily: FONT_FAMILY,
              }}>
                {metric.label}
              </span>
              <div style={{
                fontSize: '7px',
                color: FINCEPT.MUTED,
                fontFamily: FONT_FAMILY,
              }}>
                {metric.description}
              </div>
            </div>
            <span style={{
              color: getMetricColor(metric.key, data[metric.key]),
              fontFamily: FONT_FAMILY,
              fontSize: '10px',
              fontWeight: 700,
            }}>
              {formatMetricValue(metric.key, data[metric.key])}
            </span>
          </div>
        ))}
      </div>

      {availableMetrics.length === 0 && (
        <div style={{
          padding: '12px',
          color: FINCEPT.MUTED,
          fontSize: '8px',
          fontFamily: FONT_FAMILY,
          textAlign: 'center',
        }}>
          No detailed drawdown metrics returned. Run a backtest with more data to see drawdown analysis.
        </div>
      )}
    </div>
  );
};
