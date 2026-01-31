/**
 * IndexPerformanceChart - Historical performance chart for custom indices
 * Displays index value over time with interactive line chart
 */

import React, { useMemo } from 'react';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
  ReferenceLine,
} from 'recharts';
import { IndexSnapshot } from './types';
import { FINCEPT, TYPOGRAPHY, SPACING } from '../finceptStyles';

interface IndexPerformanceChartProps {
  snapshots: IndexSnapshot[];
  baseValue: number;
  currency: string;
}

const IndexPerformanceChart: React.FC<IndexPerformanceChartProps> = ({
  snapshots,
  baseValue,
  currency,
}) => {
  // Transform snapshots into chart data
  const chartData = useMemo(() => {
    if (!snapshots || snapshots.length === 0) return [];

    return snapshots
      .map(snapshot => ({
        date: snapshot.snapshot_date,
        value: snapshot.index_value,
        change: snapshot.day_change,
        changePercent: snapshot.day_change_percent,
      }))
      .sort((a, b) => new Date(a.date).getTime() - new Date(b.date).getTime());
  }, [snapshots]);

  // Calculate statistics
  const stats = useMemo(() => {
    if (chartData.length === 0) {
      return {
        totalReturn: 0,
        totalReturnPercent: 0,
        highest: baseValue,
        lowest: baseValue,
        highestDate: '',
        lowestDate: '',
      };
    }

    const values = chartData.map(d => d.value);
    const highest = Math.max(...values);
    const lowest = Math.min(...values);
    const startValue = chartData[0].value;
    const endValue = chartData[chartData.length - 1].value;
    const totalReturn = endValue - startValue;
    const totalReturnPercent = startValue > 0 ? (totalReturn / startValue) * 100 : 0;

    const highestPoint = chartData.find(d => d.value === highest);
    const lowestPoint = chartData.find(d => d.value === lowest);

    return {
      totalReturn,
      totalReturnPercent,
      highest,
      lowest,
      highestDate: highestPoint?.date || '',
      lowestDate: lowestPoint?.date || '',
    };
  }, [chartData, baseValue]);

  // Custom tooltip
  const CustomTooltip = ({ active, payload }: any) => {
    if (!active || !payload || !payload[0]) return null;

    const data = payload[0].payload;

    return (
      <div
        style={{
          backgroundColor: FINCEPT.CHARCOAL,
          border: `1px solid ${FINCEPT.CYAN}`,
          padding: SPACING.DEFAULT,
          borderRadius: '4px',
        }}
      >
        <p style={{ margin: 0, fontSize: TYPOGRAPHY.SMALL, color: FINCEPT.GRAY }}>
          {new Date(data.date).toLocaleDateString('en-US', {
            year: 'numeric',
            month: 'short',
            day: 'numeric',
          })}
        </p>
        <p style={{ margin: '4px 0 0 0', fontSize: TYPOGRAPHY.DEFAULT, color: FINCEPT.WHITE }}>
          <strong>{data.value.toFixed(2)}</strong>
        </p>
        <p
          style={{
            margin: '2px 0 0 0',
            fontSize: TYPOGRAPHY.SMALL,
            color: data.change >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
          }}
        >
          {data.change >= 0 ? '+' : ''}
          {data.change.toFixed(2)} ({data.changePercent.toFixed(2)}%)
        </p>
      </div>
    );
  };

  // Format date for X-axis
  const formatXAxis = (dateStr: string) => {
    const date = new Date(dateStr);
    return date.toLocaleDateString('en-US', { month: 'short', day: 'numeric' });
  };

  // Format value for Y-axis
  const formatYAxis = (value: number) => {
    return value.toFixed(0);
  };

  if (chartData.length === 0) {
    return (
      <div
        style={{
          padding: SPACING.EXTRA_LARGE,
          textAlign: 'center',
          color: FINCEPT.GRAY,
        }}
      >
        <p style={{ margin: 0 }}>No historical data available</p>
        <p style={{ margin: '8px 0 0 0', fontSize: TYPOGRAPHY.SMALL }}>
          Save snapshots to see performance over time
        </p>
      </div>
    );
  }

  return (
    <div style={{ width: '100%', height: '100%' }}>
      {/* Statistics Row */}
      <div
        style={{
          display: 'grid',
          gridTemplateColumns: 'repeat(4, 1fr)',
          gap: SPACING.DEFAULT,
          marginBottom: SPACING.LARGE,
          padding: SPACING.DEFAULT,
          backgroundColor: `${FINCEPT.CHARCOAL}40`,
          borderRadius: '4px',
        }}
      >
        <div>
          <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginBottom: '4px' }}>
            TOTAL RETURN
          </div>
          <div
            style={{
              fontSize: TYPOGRAPHY.DEFAULT,
              color: stats.totalReturn >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
              fontWeight: TYPOGRAPHY.BOLD,
            }}
          >
            {stats.totalReturn >= 0 ? '+' : ''}
            {stats.totalReturn.toFixed(2)} ({stats.totalReturnPercent.toFixed(2)}%)
          </div>
        </div>

        <div>
          <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginBottom: '4px' }}>
            HIGHEST
          </div>
          <div style={{ fontSize: TYPOGRAPHY.DEFAULT, color: FINCEPT.WHITE }}>
            {stats.highest.toFixed(2)}
          </div>
          <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
            {stats.highestDate
              ? new Date(stats.highestDate).toLocaleDateString('en-US', {
                  month: 'short',
                  day: 'numeric',
                })
              : ''}
          </div>
        </div>

        <div>
          <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginBottom: '4px' }}>
            LOWEST
          </div>
          <div style={{ fontSize: TYPOGRAPHY.DEFAULT, color: FINCEPT.WHITE }}>
            {stats.lowest.toFixed(2)}
          </div>
          <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY }}>
            {stats.lowestDate
              ? new Date(stats.lowestDate).toLocaleDateString('en-US', {
                  month: 'short',
                  day: 'numeric',
                })
              : ''}
          </div>
        </div>

        <div>
          <div style={{ fontSize: TYPOGRAPHY.TINY, color: FINCEPT.GRAY, marginBottom: '4px' }}>
            DATA POINTS
          </div>
          <div style={{ fontSize: TYPOGRAPHY.DEFAULT, color: FINCEPT.WHITE }}>
            {chartData.length} days
          </div>
        </div>
      </div>

      {/* Chart */}
      <ResponsiveContainer width="100%" height={400}>
        <LineChart data={chartData} margin={{ top: 5, right: 30, left: 20, bottom: 5 }}>
          <CartesianGrid strokeDasharray="3 3" stroke={`${FINCEPT.GRAY}30`} />
          <XAxis
            dataKey="date"
            tickFormatter={formatXAxis}
            stroke={FINCEPT.GRAY}
            style={{ fontSize: TYPOGRAPHY.TINY }}
            tick={{ fill: FINCEPT.GRAY }}
          />
          <YAxis
            tickFormatter={formatYAxis}
            stroke={FINCEPT.GRAY}
            style={{ fontSize: TYPOGRAPHY.TINY }}
            tick={{ fill: FINCEPT.GRAY }}
            domain={['auto', 'auto']}
          />
          <Tooltip content={<CustomTooltip />} />
          <ReferenceLine
            y={baseValue}
            stroke={FINCEPT.GRAY}
            strokeDasharray="3 3"
            label={{
              value: `Base: ${baseValue}`,
              fill: FINCEPT.GRAY,
              fontSize: TYPOGRAPHY.TINY,
              position: 'right',
            }}
          />
          <Line
            type="monotone"
            dataKey="value"
            stroke={FINCEPT.CYAN}
            strokeWidth={2}
            dot={false}
            activeDot={{ r: 6, fill: FINCEPT.CYAN }}
          />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
};

export default IndexPerformanceChart;
