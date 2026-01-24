/**
 * RollingMetricsChart - Rolling Sharpe, Volatility, and Drawdown line charts
 */

import React, { useState } from 'react';
import {
  XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer,
  Line, LineChart,
} from 'recharts';
import { F, RollingMetric, formatNumber } from './backtestingStyles';

interface RollingMetricsChartProps {
  rollingSharpe: RollingMetric[];
  rollingVolatility: RollingMetric[];
  rollingDrawdown: RollingMetric[];
}

type RollingView = 'sharpe' | 'volatility' | 'drawdown';

const RollingMetricsChart: React.FC<RollingMetricsChartProps> = ({
  rollingSharpe, rollingVolatility, rollingDrawdown,
}) => {
  const [view, setView] = useState<RollingView>('sharpe');

  const viewConfig: Record<RollingView, { data: RollingMetric[]; color: string; label: string; format: (v: number) => string }> = {
    sharpe: { data: rollingSharpe, color: F.CYAN, label: 'Rolling Sharpe (60d)', format: (v: number) => formatNumber(v) },
    volatility: { data: rollingVolatility, color: F.YELLOW, label: 'Rolling Volatility (20d)', format: (v: number) => `${(v * 100).toFixed(1)}%` },
    drawdown: { data: rollingDrawdown, color: F.RED, label: 'Rolling Drawdown', format: (v: number) => `${(v * 100).toFixed(1)}%` },
  };

  const current = viewConfig[view];

  if (!current.data || current.data.length === 0) {
    return (
      <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', height: '100%', color: F.MUTED, fontSize: '11px' }}>
        No rolling metrics data available
      </div>
    );
  }

  const maxPoints = 500;
  const raw = current.data;
  const data = raw.length > maxPoints
    ? raw.filter((_, i) => i % Math.ceil(raw.length / maxPoints) === 0 || i === raw.length - 1)
    : raw;

  const chartData = data.map(d => ({
    date: d.date.split('T')[0],
    value: d.value,
  }));

  const views: { key: RollingView; label: string }[] = [
    { key: 'sharpe', label: 'SHARPE' },
    { key: 'volatility', label: 'VOL' },
    { key: 'drawdown', label: 'DD' },
  ];

  return (
    <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
      <div style={{ display: 'flex', gap: '4px', marginBottom: '6px' }}>
        {views.map(v => (
          <button key={v.key} onClick={() => setView(v.key)} style={{
            padding: '3px 8px', fontSize: '8px', fontWeight: 700,
            fontFamily: '"IBM Plex Mono", monospace', textTransform: 'uppercase',
            color: view === v.key ? F.ORANGE : F.GRAY,
            backgroundColor: view === v.key ? F.HEADER_BG : 'transparent',
            border: `1px solid ${view === v.key ? F.ORANGE : F.BORDER}`,
            borderRadius: '2px', cursor: 'pointer',
          }}>
            {v.label}
          </button>
        ))}
        <span style={{ fontSize: '9px', color: F.MUTED, marginLeft: 'auto', alignSelf: 'center' }}>
          {current.label}
        </span>
      </div>
      <div style={{ flex: 1, minHeight: 0 }}>
        <ResponsiveContainer width="100%" height="100%">
          <LineChart data={chartData} margin={{ top: 10, right: 10, left: 0, bottom: 0 }}>
            <CartesianGrid strokeDasharray="3 3" stroke={F.BORDER} />
            <XAxis
              dataKey="date" tick={{ fontSize: 9, fill: F.GRAY }}
              stroke={F.BORDER} tickLine={false}
              interval={Math.floor(chartData.length / 6)}
            />
            <YAxis
              tick={{ fontSize: 9, fill: F.GRAY }} stroke={F.BORDER}
              tickLine={false} tickFormatter={(v: number) => current.format(v)}
            />
            <Tooltip
              contentStyle={{
                backgroundColor: F.PANEL_BG, border: `1px solid ${F.BORDER}`,
                borderRadius: '2px', fontSize: '10px', fontFamily: '"IBM Plex Mono", monospace',
              }}
              labelStyle={{ color: F.GRAY }}
              formatter={(value: number) => [current.format(value), current.label]}
            />
            <Line type="monotone" dataKey="value" stroke={current.color} strokeWidth={1.5} dot={false} />
            {view === 'sharpe' && (
              <Line type="monotone" dataKey={() => 0} stroke={F.MUTED} strokeDasharray="4 4" strokeWidth={0.5} dot={false} />
            )}
          </LineChart>
        </ResponsiveContainer>
      </div>
    </div>
  );
};

export default RollingMetricsChart;
