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
import type { PerformanceSnapshot, ChartData } from '../types';
import { formatCurrency, MODEL_COLORS } from '../types';

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
    if (!snapshots.length) {
      return { chartData: [], modelNames: [] };
    }

    // Group by cycle
    const cycleData = new Map<number, ChartData[number]>();
    const models = new Set<string>();

    snapshots.forEach((snap) => {
      models.add(snap.model_name);

      if (!cycleData.has(snap.cycle_number)) {
        cycleData.set(snap.cycle_number, {
          cycle: snap.cycle_number,
          timestamp: snap.timestamp,
        });
      }

      const data = cycleData.get(snap.cycle_number)!;
      data[snap.model_name] = snap.portfolio_value;
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
      <div className="bg-[#0F0F0F] rounded-lg p-4 border border-[#2A2A2A]">
        <div className="flex items-center gap-2 mb-4">
          <TrendingUp className="w-5 h-5 text-[#FF8800]" />
          <h3 className="text-white font-semibold">Performance Chart</h3>
        </div>
        <div
          className="flex items-center justify-center text-[#787878]"
          style={{ height }}
        >
          Run cycles to see performance data
        </div>
      </div>
    );
  }

  return (
    <div className="bg-[#0F0F0F] rounded-lg p-4 border border-[#2A2A2A]">
      <div className="flex items-center gap-2 mb-4">
        <TrendingUp className="w-5 h-5 text-[#FF8800]" />
        <h3 className="text-white font-semibold">Performance Chart</h3>
      </div>

      <ResponsiveContainer width="100%" height={height}>
        <LineChart
          data={chartData}
          margin={{ top: 5, right: 30, left: 20, bottom: 5 }}
        >
          <CartesianGrid strokeDasharray="3 3" stroke="#2A2A2A" />
          <XAxis
            dataKey="cycle"
            stroke="#787878"
            fontSize={12}
            tickFormatter={(value) => `#${value}`}
          />
          <YAxis
            stroke="#787878"
            fontSize={12}
            tickFormatter={(value) => `$${(value / 1000).toFixed(1)}k`}
            domain={['dataMin - 500', 'dataMax + 500']}
          />
          <Tooltip
            contentStyle={{
              backgroundColor: '#1A1A1A',
              border: '1px solid #2A2A2A',
              borderRadius: '8px',
            }}
            labelStyle={{ color: '#FFFFFF' }}
            formatter={(value: number, name: string) => [
              formatCurrency(value),
              name,
            ]}
            labelFormatter={(label) => `Cycle ${label}`}
          />
          {showLegend && (
            <Legend
              wrapperStyle={{ paddingTop: '10px' }}
              formatter={(value) => (
                <span style={{ color: '#FFFFFF' }}>{value}</span>
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

export default PerformanceChart;
