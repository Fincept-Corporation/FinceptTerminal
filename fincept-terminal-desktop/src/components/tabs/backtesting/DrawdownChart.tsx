/**
 * DrawdownChart - Underwater equity chart showing drawdown periods
 */

import React from 'react';
import {
  XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer,
  Area, AreaChart,
} from 'recharts';
import { F, ExtendedEquityPoint, formatPercent } from './backtestingStyles';

interface DrawdownChartProps {
  equity: ExtendedEquityPoint[];
}

const DrawdownChart: React.FC<DrawdownChartProps> = ({ equity }) => {
  if (!equity || equity.length === 0) {
    return (
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', color: F.MUTED, fontSize: '11px' }}>
        No drawdown data available
      </div>
    );
  }

  const maxPoints = 500;
  const data = equity.length > maxPoints
    ? equity.filter((_, i) => i % Math.ceil(equity.length / maxPoints) === 0 || i === equity.length - 1)
    : equity;

  const chartData = data.map(d => ({
    date: d.date.split('T')[0],
    drawdown: -Math.abs(d.drawdown) * 100,  // Negative percentage for underwater chart
  }));

  return (
    <ResponsiveContainer width="100%" height="100%">
      <AreaChart data={chartData} margin={{ top: 10, right: 10, left: 0, bottom: 0 }}>
        <defs>
          <linearGradient id="drawdownGradient" x1="0" y1="0" x2="0" y2="1">
            <stop offset="5%" stopColor={F.RED} stopOpacity={0.4} />
            <stop offset="95%" stopColor={F.RED} stopOpacity={0.05} />
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
          tickLine={false} tickFormatter={(v: number) => `${v.toFixed(0)}%`}
          domain={['auto', 0]}
        />
        <Tooltip
          contentStyle={{
            backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`,
            borderRadius: '2px', fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace',
          }}
          labelStyle={{ color: F.GRAY }}
          formatter={(value: number) => [`${value.toFixed(2)}%`, 'Drawdown']}
        />
        <Area
          type="monotone" dataKey="drawdown" stroke={F.RED}
          fill="url(#drawdownGradient)" strokeWidth={1}
        />
      </AreaChart>
    </ResponsiveContainer>
  );
};

export default DrawdownChart;
