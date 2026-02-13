/**
 * Performance Chart Component
 *
 * Displays portfolio value over time for all competing models.
 */

import React, { useMemo } from 'react';
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  Tooltip,
  Legend,
  ResponsiveContainer,
  CartesianGrid,
} from 'recharts';
import { TrendingUp } from 'lucide-react';
import { withErrorBoundary } from '@/components/common/ErrorBoundary';
import type { PerformanceSnapshot, ChartData } from '../types';
import { formatCurrency, MODEL_COLORS } from '../types';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
};

const TERMINAL_FONT = '"IBM Plex Mono", "Consolas", monospace';

interface PerformanceChartProps {
  snapshots: PerformanceSnapshot[];
  height?: number;
  showLegend?: boolean;
}

const PerformanceChart: React.FC<PerformanceChartProps> = ({
  snapshots,
  height = 300,
  showLegend = true,
}) => {
  // Transform snapshots into chart data
  const { chartData, modelNames } = useMemo(() => {
    // Defensive guard: ensure snapshots is a valid array
    if (!Array.isArray(snapshots) || !snapshots.length) {
      return { chartData: [], modelNames: [] };
    }

    // Group by cycle
    const cycleData = new Map<number, ChartData[number]>();
    const models = new Set<string>();

    snapshots.forEach((snap) => {
      models.add(snap.model_name);

      const cycleNum = Number.isFinite(snap.cycle_number) ? snap.cycle_number : 0;
      if (!cycleData.has(cycleNum)) {
        cycleData.set(cycleNum, {
          cycle: cycleNum,
          timestamp: snap.timestamp,
        });
      }

      const data = cycleData.get(cycleNum)!;
      data[snap.model_name] = Number.isFinite(snap.portfolio_value) ? snap.portfolio_value : 0;
    });

    // Sort by cycle and convert to array
    const sorted = Array.from(cycleData.entries())
      .sort((a, b) => a[0] - b[0])
      .map(([, data]) => data);

    return {
      chartData: sorted,
      modelNames: Array.from(models),
    };
  }, [snapshots]);

  if (chartData.length === 0) {
    return (
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        padding: '16px',
        fontFamily: TERMINAL_FONT,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
          <TrendingUp size={16} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ color: FINCEPT.WHITE, fontWeight: 600, fontSize: '13px' }}>
            PERFORMANCE CHART
          </span>
        </div>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          justifyContent: 'center',
          color: FINCEPT.GRAY,
          fontSize: '12px',
          height,
        }}>
          Run cycles to see performance data
        </div>
      </div>
    );
  }

  return (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      padding: '16px',
      fontFamily: TERMINAL_FONT,
    }}>
      <div style={{ display: 'flex', alignItems: 'center', gap: '8px', marginBottom: '16px' }}>
        <TrendingUp size={16} style={{ color: FINCEPT.ORANGE }} />
        <span style={{ color: FINCEPT.WHITE, fontWeight: 600, fontSize: '13px' }}>
          PERFORMANCE CHART
        </span>
      </div>

      <ResponsiveContainer width="100%" height={height}>
        <LineChart
          data={chartData}
          margin={{ top: 5, right: 30, left: 20, bottom: 5 }}
        >
          <CartesianGrid strokeDasharray="3 3" stroke={FINCEPT.BORDER} />
          <XAxis
            dataKey="cycle"
            stroke={FINCEPT.GRAY}
            fontSize={10}
            fontFamily={TERMINAL_FONT}
            tickFormatter={(value) => `#${value}`}
          />
          <YAxis
            stroke={FINCEPT.GRAY}
            fontSize={10}
            fontFamily={TERMINAL_FONT}
            tickFormatter={(value) => `$${(value / 1000).toFixed(1)}k`}
            domain={['dataMin - 500', 'dataMax + 500']}
          />
          <Tooltip
            contentStyle={{
              backgroundColor: FINCEPT.HEADER_BG,
              border: `1px solid ${FINCEPT.BORDER}`,
              borderRadius: 0,
              fontFamily: TERMINAL_FONT,
              fontSize: '11px',
            }}
            labelStyle={{ color: FINCEPT.WHITE, fontFamily: TERMINAL_FONT }}
            formatter={(value: number, name: string) => [
              formatCurrency(value),
              name,
            ]}
            labelFormatter={(label) => `Cycle ${label}`}
          />
          {showLegend && (
            <Legend
              wrapperStyle={{ paddingTop: '10px', fontFamily: TERMINAL_FONT, fontSize: '10px' }}
              formatter={(value) => (
                <span style={{ color: FINCEPT.WHITE, fontFamily: TERMINAL_FONT }}>{value}</span>
              )}
            />
          )}

          {modelNames.map((modelName) => (
            <Line
              key={modelName}
              type="monotone"
              dataKey={modelName}
              stroke={MODEL_COLORS[modelName] || '#6B7280'}
              strokeWidth={2}
              dot={{ fill: MODEL_COLORS[modelName] || '#6B7280', r: 3 }}
              activeDot={{ r: 5 }}
            />
          ))}
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
};

export default withErrorBoundary(PerformanceChart, { name: 'AlphaArena.PerformanceChart' });
