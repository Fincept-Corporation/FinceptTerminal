// Peer Comparison Panel - Helper components

import React from 'react';
import type { CompanyMetrics } from '@/types/peer';
import { formatMetricValue, getMetricValue, getAllMetricNames } from '../../../../types/peer';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';

// Comparison Table Component
interface ComparisonTableProps {
  target: CompanyMetrics;
  peers: CompanyMetrics[];
  selectedMetrics: string[];
  onToggleMetric: (metric: string) => void;
}

export const ComparisonTable: React.FC<ComparisonTableProps> = ({
  target,
  peers,
  selectedMetrics,
  onToggleMetric,
}) => {
  const allMetrics = getAllMetricNames(target);

  return (
    <div style={{
      ...COMMON_STYLES.panel,
      padding: 0,
      overflow: 'hidden',
    }}>
      {/* Metric Selector */}
      <div style={{
        padding: SPACING.LARGE,
        borderBottom: BORDERS.STANDARD,
      }}>
        <h4 style={{
          ...COMMON_STYLES.dataLabel,
          marginBottom: SPACING.MEDIUM,
        }}>SELECT METRICS</h4>
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: SPACING.SMALL }}>
          {allMetrics.map((metric) => (
            <button
              key={metric}
              onClick={() => onToggleMetric(metric)}
              style={{
                padding: `${SPACING.SMALL} ${SPACING.MEDIUM}`,
                fontSize: TYPOGRAPHY.BODY,
                backgroundColor: selectedMetrics.includes(metric) ? FINCEPT.ORANGE : FINCEPT.DARK_BG,
                border: `1px solid ${selectedMetrics.includes(metric) ? FINCEPT.ORANGE : FINCEPT.BORDER}`,
                color: selectedMetrics.includes(metric) ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                cursor: 'pointer',
                transition: 'all 0.2s',
                fontWeight: selectedMetrics.includes(metric) ? TYPOGRAPHY.BOLD : TYPOGRAPHY.REGULAR,
              }}
            >
              {metric}
            </button>
          ))}
        </div>
      </div>

      {/* Table */}
      <div style={{ overflowX: 'auto' }}>
        <table style={{ width: '100%', borderCollapse: 'collapse' }}>
          <thead>
            <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
              <th style={{
                ...COMMON_STYLES.tableHeader,
                padding: SPACING.MEDIUM,
              }}>
                METRIC
              </th>
              <th style={{
                ...COMMON_STYLES.tableHeader,
                padding: SPACING.MEDIUM,
                textAlign: 'right',
                color: FINCEPT.ORANGE,
              }}>
                {target.symbol} (TARGET)
              </th>
              {peers.map((peer) => (
                <th
                  key={peer.symbol}
                  style={{
                    ...COMMON_STYLES.tableHeader,
                    padding: SPACING.MEDIUM,
                    textAlign: 'right',
                  }}
                >
                  {peer.symbol}
                </th>
              ))}
            </tr>
          </thead>
          <tbody>
            {selectedMetrics.map((metric, idx) => {
              const targetValue = getMetricValue(target, metric);
              const peerValues = peers.map((p) => getMetricValue(p, metric));
              const allValues = [targetValue, ...peerValues].filter((v) => v !== undefined) as number[];
              const max = Math.max(...allValues);
              const min = Math.min(...allValues);

              return (
                <tr
                  key={metric}
                  style={{
                    borderBottom: BORDERS.STANDARD,
                    backgroundColor: idx % 2 === 0 ? 'transparent' : 'rgba(255,255,255,0.02)',
                    transition: 'background-color 0.2s',
                  }}
                >
                  <td style={{
                    padding: SPACING.MEDIUM,
                    whiteSpace: 'nowrap',
                    fontSize: TYPOGRAPHY.BODY,
                    fontWeight: TYPOGRAPHY.BOLD,
                    color: FINCEPT.WHITE,
                  }}>
                    {metric}
                  </td>
                  <td style={{
                    padding: SPACING.MEDIUM,
                    whiteSpace: 'nowrap',
                    fontSize: TYPOGRAPHY.BODY,
                    textAlign: 'right',
                    fontWeight: TYPOGRAPHY.SEMIBOLD,
                    fontFamily: TYPOGRAPHY.MONO,
                    color: targetValue === max ? FINCEPT.GREEN : targetValue === min ? FINCEPT.RED : FINCEPT.CYAN,
                  }}>
                    {formatMetricValue(targetValue, 'ratio')}
                  </td>
                  {peerValues.map((value, index) => (
                    <td key={index} style={{
                      padding: SPACING.MEDIUM,
                      whiteSpace: 'nowrap',
                      fontSize: TYPOGRAPHY.BODY,
                      textAlign: 'right',
                      fontFamily: TYPOGRAPHY.MONO,
                      color: FINCEPT.GRAY,
                    }}>
                      {formatMetricValue(value, 'ratio')}
                    </td>
                  ))}
                </tr>
              );
            })}
          </tbody>
        </table>
      </div>
    </div>
  );
};
