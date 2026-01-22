// Percentile Rank Chart Component
// Visualizes percentile rankings with bar charts

import React from 'react';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, Cell } from 'recharts';
import type { PercentileRanking } from '../../../../types/peer';
import { getPercentileColor } from '../../../../types/peer';
import { FINCEPT, TYPOGRAPHY, SPACING, BORDERS, COMMON_STYLES } from '../../portfolio-tab/finceptStyles';

interface PercentileRankChartProps {
  rankings: PercentileRanking[];
  orientation?: 'horizontal' | 'vertical';
  title?: string;
  height?: number;
}

export const PercentileRankChart: React.FC<PercentileRankChartProps> = ({
  rankings,
  orientation = 'horizontal',
  title = 'Percentile Rankings',
  height = 400,
}) => {
  // Prepare data for chart
  const chartData = rankings.map((ranking) => ({
    metric: ranking.metricName,
    percentile: ranking.percentile,
    value: ranking.value,
    median: ranking.peerMedian,
    mean: ranking.peerMean,
    zScore: ranking.zScore,
  }));

  const CustomTooltip = ({ active, payload }: any) => {
    if (active && payload && payload.length) {
      const data = payload[0].payload;
      return (
        <div style={{
          backgroundColor: FINCEPT.PANEL_BG,
          border: `1px solid ${FINCEPT.BORDER}`,
          padding: SPACING.MEDIUM,
        }}>
          <p style={{ color: FINCEPT.WHITE, fontWeight: TYPOGRAPHY.BOLD, marginBottom: SPACING.SMALL }}>{data.metric}</p>
          <div style={{ display: 'flex', flexDirection: 'column', gap: SPACING.TINY, fontSize: TYPOGRAPHY.BODY }}>
            <p style={{ color: FINCEPT.CYAN }}>
              Percentile: <span style={{ fontWeight: TYPOGRAPHY.BOLD }}>{data.percentile.toFixed(1)}%</span>
            </p>
            <p style={{ color: FINCEPT.GREEN }}>
              Value: <span style={{ fontWeight: TYPOGRAPHY.BOLD }}>{data.value.toFixed(2)}</span>
            </p>
            <p style={{ color: FINCEPT.YELLOW }}>
              Peer Median: <span style={{ fontWeight: TYPOGRAPHY.BOLD }}>{data.median.toFixed(2)}</span>
            </p>
            <p style={{ color: FINCEPT.PURPLE }}>
              Z-Score: <span style={{ fontWeight: TYPOGRAPHY.BOLD }}>{data.zScore.toFixed(2)}</span>
            </p>
          </div>
        </div>
      );
    }
    return null;
  };

  if (orientation === 'horizontal') {
    return (
      <div style={{ width: '100%' }}>
        {title && <h3 style={{
          fontSize: TYPOGRAPHY.SUBHEADING,
          fontWeight: TYPOGRAPHY.BOLD,
          color: FINCEPT.WHITE,
          marginBottom: SPACING.LARGE,
          letterSpacing: TYPOGRAPHY.WIDE,
          textTransform: 'uppercase',
        }}>{title}</h3>}
        <ResponsiveContainer width="100%" height={height}>
          <BarChart
            data={chartData}
            layout="vertical"
            margin={{ top: 5, right: 30, left: 100, bottom: 5 }}
          >
            <CartesianGrid strokeDasharray="3 3" stroke={FINCEPT.BORDER} />
            <XAxis type="number" domain={[0, 100]} stroke={FINCEPT.GRAY} />
            <YAxis dataKey="metric" type="category" stroke={FINCEPT.GRAY} width={90} />
            <Tooltip content={<CustomTooltip />} />
            <Legend />
            <Bar dataKey="percentile" name="Percentile Rank" radius={[0, 2, 2, 0]}>
              {chartData.map((entry, index) => (
                <Cell key={`cell-${index}`} fill={getPercentileColor(entry.percentile)} />
              ))}
            </Bar>
          </BarChart>
        </ResponsiveContainer>
      </div>
    );
  }

  // Vertical orientation
  return (
    <div style={{ width: '100%' }}>
      {title && <h3 style={{
        fontSize: TYPOGRAPHY.SUBHEADING,
        fontWeight: TYPOGRAPHY.BOLD,
        color: FINCEPT.WHITE,
        marginBottom: SPACING.LARGE,
        letterSpacing: TYPOGRAPHY.WIDE,
        textTransform: 'uppercase',
      }}>{title}</h3>}
      <ResponsiveContainer width="100%" height={height}>
        <BarChart
          data={chartData}
          margin={{ top: 5, right: 30, left: 20, bottom: 100 }}
        >
          <CartesianGrid strokeDasharray="3 3" stroke={FINCEPT.BORDER} />
          <XAxis
            dataKey="metric"
            stroke={FINCEPT.GRAY}
            angle={-45}
            textAnchor="end"
            height={100}
          />
          <YAxis domain={[0, 100]} stroke={FINCEPT.GRAY} />
          <Tooltip content={<CustomTooltip />} />
          <Legend />
          <Bar dataKey="percentile" name="Percentile Rank" radius={[2, 2, 0, 0]}>
            {chartData.map((entry, index) => (
              <Cell key={`cell-${index}`} fill={getPercentileColor(entry.percentile)} />
            ))}
          </Bar>
        </BarChart>
      </ResponsiveContainer>
    </div>
  );
};

// Compact version for dashboard widgets
export const PercentileRankMini: React.FC<{ ranking: PercentileRanking }> = ({ ranking }) => {
  const color = getPercentileColor(ranking.percentile);
  const width = `${ranking.percentile}%`;

  // Determine interpretation based on metric type
  const isValuationMetric = ranking.metricName.includes('Ratio') ||
                            ranking.metricName.includes('P/E') ||
                            ranking.metricName.includes('P/B') ||
                            ranking.metricName.includes('P/S') ||
                            ranking.metricName.includes('Debt');

  // For valuation and leverage metrics, lower is better
  // For profitability and growth metrics, higher is better
  let interpretation = '';
  if (ranking.percentile <= 25) {
    interpretation = isValuationMetric ? 'Lower than peers (better value)' : 'Lower than peers';
  } else if (ranking.percentile <= 50) {
    interpretation = 'Below median';
  } else if (ranking.percentile <= 75) {
    interpretation = 'Above median';
  } else {
    interpretation = isValuationMetric ? 'Higher than peers (expensive)' : 'Higher than peers (good)';
  }

  return (
    <div style={{
      backgroundColor: FINCEPT.DARK_BG,
      border: BORDERS.STANDARD,
      padding: SPACING.MEDIUM,
    }}>
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        fontSize: TYPOGRAPHY.BODY,
        marginBottom: SPACING.SMALL,
      }}>
        <span style={{ color: FINCEPT.WHITE, fontWeight: TYPOGRAPHY.BOLD }}>{ranking.metricName}</span>
        <div style={{ display: 'flex', alignItems: 'center', gap: SPACING.SMALL }}>
          <span style={{ color, fontWeight: TYPOGRAPHY.BOLD, fontFamily: TYPOGRAPHY.MONO }}>
            {ranking.percentile !== null && ranking.percentile !== undefined ? ranking.percentile.toFixed(1) : 'N/A'}%
          </span>
          <span style={{ fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GRAY }}>({interpretation})</span>
        </div>
      </div>
      <div style={{
        width: '100%',
        backgroundColor: FINCEPT.BORDER,
        height: '6px',
        marginBottom: SPACING.SMALL,
      }}>
        <div
          style={{
            height: '6px',
            transition: 'all 0.3s',
            width,
            backgroundColor: color,
          }}
        />
      </div>
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        fontSize: TYPOGRAPHY.SMALL,
        color: FINCEPT.GRAY,
      }}>
        <span>Value: <span style={{ color: FINCEPT.CYAN, fontFamily: TYPOGRAPHY.MONO }}>{ranking.value !== null && ranking.value !== undefined ? ranking.value.toFixed(2) : 'N/A'}</span></span>
        <span>Median: <span style={{ color: FINCEPT.YELLOW, fontFamily: TYPOGRAPHY.MONO }}>{ranking.peerMedian !== null && ranking.peerMedian !== undefined ? ranking.peerMedian.toFixed(2) : 'N/A'}</span></span>
      </div>
    </div>
  );
};

// Percentile comparison table
interface PercentileTableProps {
  rankings: PercentileRanking[];
}

export const PercentileTable: React.FC<PercentileTableProps> = ({ rankings }) => {
  return (
    <div style={{ overflowX: 'auto' }}>
      <table style={{ width: '100%', borderCollapse: 'collapse' }}>
        <thead>
          <tr style={{ backgroundColor: FINCEPT.HEADER_BG }}>
            <th style={{ ...COMMON_STYLES.tableHeader, padding: SPACING.MEDIUM }}>
              METRIC
            </th>
            <th style={{ ...COMMON_STYLES.tableHeader, padding: SPACING.MEDIUM, textAlign: 'right' }}>
              VALUE
            </th>
            <th style={{ ...COMMON_STYLES.tableHeader, padding: SPACING.MEDIUM, textAlign: 'right' }}>
              PERCENTILE
            </th>
            <th style={{ ...COMMON_STYLES.tableHeader, padding: SPACING.MEDIUM, textAlign: 'right' }}>
              Z-SCORE
            </th>
            <th style={{ ...COMMON_STYLES.tableHeader, padding: SPACING.MEDIUM, textAlign: 'right' }}>
              PEER MEDIAN
            </th>
            <th style={{ ...COMMON_STYLES.tableHeader, padding: SPACING.MEDIUM, textAlign: 'right' }}>
              STATUS
            </th>
          </tr>
        </thead>
        <tbody>
          {rankings.map((ranking, index) => {
            const color = getPercentileColor(ranking.percentile);
            const status =
              ranking.percentile >= 75 ? 'EXCELLENT' :
              ranking.percentile >= 50 ? 'ABOVE AVG' :
              ranking.percentile >= 25 ? 'BELOW AVG' :
              'POOR';

            return (
              <tr
                key={index}
                style={{
                  borderBottom: BORDERS.STANDARD,
                  backgroundColor: index % 2 === 0 ? 'transparent' : 'rgba(255,255,255,0.02)',
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
                  {ranking.metricName}
                </td>
                <td style={{
                  padding: SPACING.MEDIUM,
                  whiteSpace: 'nowrap',
                  fontSize: TYPOGRAPHY.BODY,
                  textAlign: 'right',
                  color: FINCEPT.GRAY,
                  fontFamily: TYPOGRAPHY.MONO,
                }}>
                  {ranking.value.toFixed(2)}
                </td>
                <td style={{
                  padding: SPACING.MEDIUM,
                  whiteSpace: 'nowrap',
                  fontSize: TYPOGRAPHY.BODY,
                  textAlign: 'right',
                  fontWeight: TYPOGRAPHY.BOLD,
                  fontFamily: TYPOGRAPHY.MONO,
                  color,
                }}>
                  {ranking.percentile.toFixed(1)}%
                </td>
                <td style={{
                  padding: SPACING.MEDIUM,
                  whiteSpace: 'nowrap',
                  fontSize: TYPOGRAPHY.BODY,
                  textAlign: 'right',
                  color: FINCEPT.GRAY,
                  fontFamily: TYPOGRAPHY.MONO,
                }}>
                  {ranking.zScore.toFixed(2)}
                </td>
                <td style={{
                  padding: SPACING.MEDIUM,
                  whiteSpace: 'nowrap',
                  fontSize: TYPOGRAPHY.BODY,
                  textAlign: 'right',
                  color: FINCEPT.MUTED,
                  fontFamily: TYPOGRAPHY.MONO,
                }}>
                  {ranking.peerMedian.toFixed(2)}
                </td>
                <td style={{
                  padding: SPACING.MEDIUM,
                  whiteSpace: 'nowrap',
                  fontSize: TYPOGRAPHY.BODY,
                  textAlign: 'right',
                }}>
                  <span
                    style={{
                      padding: `${SPACING.TINY} ${SPACING.SMALL}`,
                      fontSize: TYPOGRAPHY.SMALL,
                      fontWeight: TYPOGRAPHY.BOLD,
                      backgroundColor: `${color}20`,
                      color: color,
                      border: `1px solid ${color}`,
                    }}
                  >
                    {status}
                  </span>
                </td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
};
