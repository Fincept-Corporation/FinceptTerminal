/**
 * VBDrawdownChart - Drawdown area chart using Recharts
 */

import React, { useMemo } from 'react';
import {
  ResponsiveContainer, AreaChart, Area, XAxis, YAxis,
  Tooltip, ReferenceLine, CartesianGrid,
} from 'recharts';
import { FINCEPT, TYPOGRAPHY } from '../../../portfolio-tab/finceptStyles';

interface VBDrawdownChartProps {
  equity: any;
}

export const VBDrawdownChart: React.FC<VBDrawdownChartProps> = ({ equity }) => {
  const drawdownData = useMemo(() => {
    if (!equity) return [];

    let points: { date: string; value: number }[] = [];

    if (Array.isArray(equity)) {
      points = equity.map((pt: any) => ({
        date: pt.time || pt.date || '',
        value: pt.value ?? pt.equity ?? 0,
      }));
    } else if (typeof equity === 'object') {
      points = Object.entries(equity).map(([date, value]) => ({
        date,
        value: Number(value),
      }));
    }

    if (points.length === 0) return [];
    points.sort((a, b) => a.date.localeCompare(b.date));

    // Compute running drawdown
    let peak = points[0].value;
    return points.map(pt => {
      if (pt.value > peak) peak = pt.value;
      const dd = peak > 0 ? ((pt.value - peak) / peak) * 100 : 0;
      return {
        date: pt.date,
        drawdown: parseFloat(dd.toFixed(2)),
      };
    });
  }, [equity]);

  if (drawdownData.length === 0) {
    return (
      <div style={{ padding: '24px', textAlign: 'center', color: FINCEPT.MUTED, fontSize: '10px' }}>
        No data for drawdown chart
      </div>
    );
  }

  const maxDD = Math.min(...drawdownData.map(d => d.drawdown));

  return (
    <div style={{ width: '100%', height: '100%', padding: '4px 8px' }}>
      <div style={{ fontSize: '10px', color: FINCEPT.GRAY, marginBottom: '4px' }}>
        Max Drawdown: <span style={{ color: '#FF3B3B', fontWeight: 700 }}>{maxDD.toFixed(2)}%</span>
      </div>
      <ResponsiveContainer width="100%" height="85%">
        <AreaChart data={drawdownData} margin={{ top: 4, right: 8, bottom: 4, left: 8 }}>
          <CartesianGrid strokeDasharray="3 3" stroke="#1a1a1a" />
          <XAxis
            dataKey="date"
            tick={{ fontSize: 7, fill: '#787878' }}
            tickLine={false}
            axisLine={{ stroke: '#2A2A2A' }}
            tickFormatter={(d: string) => {
              const parts = d.split('-');
              return parts.length >= 2 ? `${parts[1]}/${parts[0]?.slice(2)}` : d;
            }}
            interval="preserveStartEnd"
            minTickGap={40}
          />
          <YAxis
            tick={{ fontSize: 7, fill: '#787878' }}
            tickLine={false}
            axisLine={{ stroke: '#2A2A2A' }}
            tickFormatter={(v: number) => `${v}%`}
            domain={['dataMin', 0]}
          />
          <Tooltip
            contentStyle={{
              backgroundColor: '#0F0F0F',
              border: '1px solid #2A2A2A',
              borderRadius: '2px',
              fontSize: '8px',
              color: '#FFFFFF',
            }}
            labelStyle={{ color: '#787878', fontSize: '8px' }}
            formatter={(value: number) => [`${value.toFixed(2)}%`, 'Drawdown']}
          />
          <ReferenceLine y={0} stroke="#2A2A2A" />
          <Area
            type="monotone"
            dataKey="drawdown"
            stroke="#FF3B3B"
            fill="#FF3B3B"
            fillOpacity={0.15}
            strokeWidth={1.5}
          />
        </AreaChart>
      </ResponsiveContainer>
    </div>
  );
};
