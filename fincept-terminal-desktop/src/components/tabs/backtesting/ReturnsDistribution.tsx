/**
 * ReturnsDistribution - Histogram of daily returns
 */

import React from 'react';
import {
  XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer,
  Bar, BarChart, ReferenceLine,
} from 'recharts';
import { F } from './backtestingStyles';

interface ReturnsDistributionProps {
  histogram: { bin: number; count: number }[];
}

const ReturnsDistribution: React.FC<ReturnsDistributionProps> = ({ histogram }) => {
  if (!histogram || histogram.length === 0) {
    return (
      <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', height: '100%', color: F.MUTED, fontSize: '11px' }}>
        No returns data available
      </div>
    );
  }

  const chartData = histogram.map(h => ({
    bin: (h.bin * 100).toFixed(2),
    count: h.count,
    fill: h.bin >= 0 ? F.GREEN : F.RED,
  }));

  return (
    <ResponsiveContainer width="100%" height="100%">
      <BarChart data={chartData} margin={{ top: 10, right: 10, left: 0, bottom: 0 }}>
        <CartesianGrid strokeDasharray="3 3" stroke={F.BORDER} />
        <XAxis
          dataKey="bin" tick={{ fontSize: 9, fill: F.GRAY }}
          stroke={F.BORDER} tickLine={false}
          label={{ value: 'Daily Return (%)', position: 'insideBottom', offset: -5, fontSize: 9, fill: F.GRAY }}
        />
        <YAxis
          tick={{ fontSize: 9, fill: F.GRAY }} stroke={F.BORDER}
          tickLine={false}
          label={{ value: 'Frequency', angle: -90, position: 'insideLeft', fontSize: 9, fill: F.GRAY }}
        />
        <Tooltip
          contentStyle={{
            backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`,
            borderRadius: '2px', fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace',
          }}
          labelStyle={{ color: F.GRAY }}
          formatter={(value: number) => [value, 'Count']}
          labelFormatter={(label: string) => `Return: ${label}%`}
        />
        <ReferenceLine x="0.00" stroke={F.GRAY} strokeDasharray="3 3" />
        <Bar dataKey="count" fill={F.ORANGE} radius={[1, 1, 0, 0]} />
      </BarChart>
    </ResponsiveContainer>
  );
};

export default ReturnsDistribution;
