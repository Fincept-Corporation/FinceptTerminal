/**
 * ReturnsResult - Returns analysis results display
 */

import React from 'react';
import { FINCEPT, FONT_FAMILY } from '../constants';
import { formatMetricValue, getMetricColor } from './metricUtils';

interface ReturnsResultProps {
  result: any;
}

const RETURNS_METRICS: { key: string; label: string }[] = [
  { key: 'totalReturn', label: 'Total Return' },
  { key: 'annualizedReturn', label: 'Annualized Return' },
  { key: 'volatility', label: 'Volatility' },
  { key: 'sharpeRatio', label: 'Sharpe Ratio' },
  { key: 'sortinoRatio', label: 'Sortino Ratio' },
  { key: 'calmarRatio', label: 'Calmar Ratio' },
  { key: 'VaR', label: 'Value at Risk (95%)' },
  { key: 'CVaR', label: 'Conditional VaR' },
  { key: 'upCapture', label: 'Up Capture Ratio' },
  { key: 'downCapture', label: 'Down Capture Ratio' },
  { key: 'skewness', label: 'Skewness' },
  { key: 'kurtosis', label: 'Kurtosis' },
  { key: 'CAGR', label: 'CAGR' },
  { key: 'tailRatio', label: 'Tail Ratio' },
  { key: 'commonSenseRatio', label: 'Common Sense Ratio' },
];

export const ReturnsResult: React.FC<ReturnsResultProps> = ({ result }) => {
  const data = result?.data;

  if (!data) {
    return (
      <div style={{ padding: '16px', color: FINCEPT.MUTED, fontSize: '9px', fontFamily: FONT_FAMILY, textAlign: 'center' }}>
        No returns data available.
      </div>
    );
  }

  const metrics = data.performance || data.returns || data;
  const rollingMetrics = data.rolling;
  const benchmarkComparison = data.benchmark;

  const availableMetrics = RETURNS_METRICS.filter(m => metrics[m.key] !== undefined && metrics[m.key] !== null);

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Summary Metrics */}
      {availableMetrics.length > 0 && (
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
        }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: FINCEPT.ORANGE,
            marginBottom: '8px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
          }}>
            RETURNS ANALYSIS
          </div>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            {availableMetrics.map(metric => (
              <div key={metric.key} style={{
                padding: '6px 8px',
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
              }}>
                <span style={{
                  color: FINCEPT.WHITE,
                  fontSize: '8px',
                  fontWeight: 600,
                  fontFamily: FONT_FAMILY,
                }}>
                  {metric.label}
                </span>
                <span style={{
                  color: getMetricColor(metric.key, metrics[metric.key]),
                  fontFamily: FONT_FAMILY,
                  fontSize: '10px',
                  fontWeight: 700,
                }}>
                  {formatMetricValue(metric.key, metrics[metric.key])}
                </span>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Rolling Metrics */}
      {rollingMetrics && (
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
        }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: FINCEPT.BLUE,
            marginBottom: '8px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
          }}>
            ROLLING METRICS
          </div>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            {Object.entries(rollingMetrics).map(([key, value]) => (
              <div key={key} style={{
                padding: '6px 8px',
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
              }}>
                <span style={{ color: FINCEPT.WHITE, fontSize: '8px', fontWeight: 600, fontFamily: FONT_FAMILY }}>
                  {key.replace(/([A-Z])/g, ' $1').trim()}
                </span>
                <span style={{ color: FINCEPT.CYAN, fontSize: '10px', fontWeight: 700, fontFamily: FONT_FAMILY }}>
                  {typeof value === 'number' ? value.toFixed(4) : String(value)}
                </span>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Benchmark Comparison */}
      {benchmarkComparison && (
        <div style={{
          padding: '12px',
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          borderRadius: '2px',
        }}>
          <div style={{
            fontSize: '8px', fontWeight: 700, color: FINCEPT.GREEN,
            marginBottom: '8px', letterSpacing: '0.5px', fontFamily: FONT_FAMILY,
          }}>
            BENCHMARK COMPARISON
          </div>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
            {Object.entries(benchmarkComparison).map(([key, value]) => (
              <div key={key} style={{
                padding: '6px 8px',
                backgroundColor: FINCEPT.DARK_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: '2px',
                display: 'flex',
                justifyContent: 'space-between',
                alignItems: 'center',
              }}>
                <span style={{ color: FINCEPT.WHITE, fontSize: '8px', fontWeight: 600, fontFamily: FONT_FAMILY }}>
                  {key.replace(/([A-Z])/g, ' $1').trim()}
                </span>
                <span style={{ color: FINCEPT.GREEN, fontSize: '10px', fontWeight: 700, fontFamily: FONT_FAMILY }}>
                  {typeof value === 'number' ? value.toFixed(4) : String(value)}
                </span>
              </div>
            ))}
          </div>
        </div>
      )}

      {availableMetrics.length === 0 && !rollingMetrics && !benchmarkComparison && (
        <div style={{
          padding: '16px',
          color: FINCEPT.MUTED,
          fontSize: '9px',
          fontFamily: FONT_FAMILY,
          textAlign: 'center',
        }}>
          Returns analysis returned no displayable metrics. Check raw output.
        </div>
      )}
    </div>
  );
};
