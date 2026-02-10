/**
 * ResultCharts - Equity curve + drawdown chart using Recharts
 * Design system: PANEL_BG container, 2px radius, FINCEPT colors
 */

import React, { useMemo } from 'react';
import {
  AreaChart, Area, XAxis, YAxis, Tooltip, ResponsiveContainer, CartesianGrid,
  ReferenceLine,
} from 'recharts';
import { FINCEPT, FONT_FAMILY } from '../constants';

interface ResultChartsProps {
  result: any;
}

export const ResultCharts: React.FC<ResultChartsProps> = ({ result }) => {
  const equityData = useMemo(() => {
    if (!result?.data?.equity) return [];
    return result.data.equity.map((item: any, idx: number) => ({
      date: item.date || `Day ${idx + 1}`,
      equity: typeof item === 'number' ? item : (item.equity ?? item.value ?? 0),
      returns: typeof item === 'number' ? 0 : (item.returns ?? 0),
      drawdown: typeof item === 'number' ? 0 : (item.drawdown ?? 0),
      benchmark: typeof item === 'number' ? undefined : item.benchmark,
    }));
  }, [result]);

  if (equityData.length === 0) {
    return (
      <div style={{
        padding: '16px',
        color: FINCEPT.MUTED,
        fontSize: '9px',
        fontFamily: FONT_FAMILY,
        textAlign: 'center',
      }}>
        No equity curve data available. Run a backtest to see charts.
      </div>
    );
  }

  const tooltipStyle = {
    backgroundColor: FINCEPT.HEADER_BG,
    border: `1px solid ${FINCEPT.BORDER}`,
    borderRadius: '2px',
    fontSize: '8px',
    fontFamily: FONT_FAMILY,
    color: FINCEPT.WHITE,
  };

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: '12px' }}>
      {/* Equity Curve */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        padding: '12px',
      }}>
        <div style={{
          fontSize: '8px',
          fontWeight: 700,
          color: FINCEPT.ORANGE,
          marginBottom: '8px',
          letterSpacing: '0.5px',
          fontFamily: FONT_FAMILY,
        }}>
          EQUITY CURVE
        </div>
        <ResponsiveContainer width="100%" height={180}>
          <AreaChart data={equityData} margin={{ top: 4, right: 4, bottom: 0, left: 4 }}>
            <CartesianGrid stroke={FINCEPT.BORDER} strokeDasharray="3 3" />
            <XAxis
              dataKey="date"
              tick={{ fontSize: 7, fill: FINCEPT.MUTED, fontFamily: FONT_FAMILY }}
              tickLine={false}
              axisLine={{ stroke: FINCEPT.BORDER }}
              interval="preserveStartEnd"
            />
            <YAxis
              tick={{ fontSize: 7, fill: FINCEPT.MUTED, fontFamily: FONT_FAMILY }}
              tickLine={false}
              axisLine={{ stroke: FINCEPT.BORDER }}
              width={50}
            />
            <Tooltip contentStyle={tooltipStyle} />
            <Area
              type="monotone"
              dataKey="equity"
              stroke={FINCEPT.GREEN}
              fill={`${FINCEPT.GREEN}30`}
              strokeWidth={1.5}
              dot={false}
            />
            {equityData[0]?.benchmark !== undefined && (
              <Area
                type="monotone"
                dataKey="benchmark"
                stroke={FINCEPT.MUTED}
                fill="transparent"
                strokeWidth={1}
                strokeDasharray="4 2"
                dot={false}
              />
            )}
          </AreaChart>
        </ResponsiveContainer>
      </div>

      {/* Drawdown Chart */}
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        borderRadius: '2px',
        padding: '12px',
      }}>
        <div style={{
          fontSize: '8px',
          fontWeight: 700,
          color: FINCEPT.RED,
          marginBottom: '8px',
          letterSpacing: '0.5px',
          fontFamily: FONT_FAMILY,
        }}>
          DRAWDOWN
        </div>
        <ResponsiveContainer width="100%" height={120}>
          <AreaChart data={equityData} margin={{ top: 4, right: 4, bottom: 0, left: 4 }}>
            <CartesianGrid stroke={FINCEPT.BORDER} strokeDasharray="3 3" />
            <XAxis
              dataKey="date"
              tick={{ fontSize: 7, fill: FINCEPT.MUTED, fontFamily: FONT_FAMILY }}
              tickLine={false}
              axisLine={{ stroke: FINCEPT.BORDER }}
              interval="preserveStartEnd"
            />
            <YAxis
              tick={{ fontSize: 7, fill: FINCEPT.MUTED, fontFamily: FONT_FAMILY }}
              tickLine={false}
              axisLine={{ stroke: FINCEPT.BORDER }}
              width={50}
            />
            <Tooltip contentStyle={tooltipStyle} />
            <ReferenceLine y={0} stroke={FINCEPT.MUTED} strokeDasharray="2 2" />
            <Area
              type="monotone"
              dataKey="drawdown"
              stroke={FINCEPT.RED}
              fill={`${FINCEPT.RED}30`}
              strokeWidth={1.5}
              dot={false}
            />
          </AreaChart>
        </ResponsiveContainer>
      </div>
    </div>
  );
};
