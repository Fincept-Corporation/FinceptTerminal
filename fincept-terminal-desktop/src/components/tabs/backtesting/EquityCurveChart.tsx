/**
 * EquityCurveChart - Equity curve with optional benchmark overlay
 */

import React from 'react';
import {
  XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer,
  Area, AreaChart, Line,
} from 'recharts';
import { F, ExtendedEquityPoint, formatCurrency } from './backtestingStyles';

interface EquityCurveChartProps {
  equity: ExtendedEquityPoint[];
  initialCapital: number;
}

const EquityCurveChart: React.FC<EquityCurveChartProps> = ({ equity, initialCapital }) => {
  if (!equity || equity.length === 0) {
    return (
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', color: F.MUTED, fontSize: '11px' }}>
        No equity data available
      </div>
    );
  }

  // Downsample for performance if too many points
  const maxPoints = 500;
  const data = equity.length > maxPoints
    ? equity.filter((_, i) => i % Math.ceil(equity.length / maxPoints) === 0 || i === equity.length - 1)
    : equity;

  const hasBenchmark = data.some(d => d.benchmark != null && d.benchmark > 0);

  // Benchmark is already in absolute equity scale from Python
  const chartData = data.map(d => ({
    date: d.date.split('T')[0],
    equity: d.equity,
    benchmark: hasBenchmark && d.benchmark ? d.benchmark : undefined,
  }));

  return (
    <ResponsiveContainer width="100%" height="100%">
      <AreaChart data={chartData} margin={{ top: 10, right: 10, left: 0, bottom: 0 }}>
        <defs>
          <linearGradient id="equityGradient" x1="0" y1="0" x2="0" y2="1">
            <stop offset="5%" stopColor={F.ORANGE} stopOpacity={0.3} />
            <stop offset="95%" stopColor={F.ORANGE} stopOpacity={0} />
          </linearGradient>
        </defs>
        <CartesianGrid strokeDasharray="3 3" stroke={F.BORDER} />
        <XAxis
          dataKey="date" tick={{ fontSize: 9, fill: F.GRAY }}
          stroke={F.BORDER} tickLine={false}
          interval={Math.floor(chartData.length / 6)}
        />
        <YAxis
          tick={{ fontSize: 9, fill: F.GRAY }} stroke={F.BORDER}
          tickLine={false} tickFormatter={(v: number) => `$${(v / 1000).toFixed(0)}k`}
        />
        <Tooltip
          contentStyle={{
            backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`,
            borderRadius: '2px', fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace',
          }}
          labelStyle={{ color: F.GRAY }}
          formatter={(value: number, name: string) => [formatCurrency(value), name === 'equity' ? 'Strategy' : 'Benchmark']}
        />
        <Area
          type="monotone" dataKey="equity" stroke={F.ORANGE}
          fill="url(#equityGradient)" strokeWidth={1.5}
        />
        {hasBenchmark && (
          <Line
            type="monotone" dataKey="benchmark" stroke={F.BLUE}
            strokeWidth={1} strokeDasharray="4 2" dot={false}
          />
        )}
      </AreaChart>
    </ResponsiveContainer>
  );
};

export default EquityCurveChart;
