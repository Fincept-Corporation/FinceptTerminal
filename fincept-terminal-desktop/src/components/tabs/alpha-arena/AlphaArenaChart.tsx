/**
 * Alpha Arena Performance Chart
 * Bloomberg-style portfolio value chart showing all models' performance over time
 */

import React, { useMemo } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer } from 'recharts';
import { AlphaPerformanceSnapshot } from '@/services/sqliteService';

interface AlphaArenaChartProps {
  snapshots: AlphaPerformanceSnapshot[];
  initialCapital?: number;
}

// Bloomberg color palette for models
const MODEL_COLORS = [
  '#FF8800', // Orange (primary)
  '#00D66F', // Green
  '#0088FF', // Blue
  '#9D4EDD', // Purple
  '#FFD700', // Gold
  '#00E5FF', // Cyan
  '#FF3B3B', // Red
  '#4ADE80', // Light Green
  '#F59E0B', // Amber
  '#EC4899', // Pink
];

const BLOOMBERG = {
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  BORDER: '#2A2A2A',
  GRAY: '#787878',
  WHITE: '#FFFFFF',
  ORANGE: '#FF8800',
  GREEN: '#00D66F',
};

export const AlphaArenaChart: React.FC<AlphaArenaChartProps> = ({
  snapshots,
  initialCapital = 10000
}) => {
  // Transform snapshots into chart data
  const chartData = useMemo(() => {
    if (!snapshots || snapshots.length === 0) return [];

    // Group by cycle number
    const cycleMap = new Map<number, any>();

    snapshots.forEach(snap => {
      if (!cycleMap.has(snap.cycle_number)) {
        cycleMap.set(snap.cycle_number, {
          cycle: snap.cycle_number,
          timestamp: snap.timestamp,
        });
      }

      const cycleData = cycleMap.get(snap.cycle_number)!;
      cycleData[snap.model_name] = snap.portfolio_value;
    });

    // Convert to array and sort by cycle
    const data = Array.from(cycleMap.values()).sort((a, b) => a.cycle - b.cycle);

    return data;
  }, [snapshots]);

  // Get unique model names for lines
  const modelNames = useMemo(() => {
    const names = new Set<string>();
    snapshots.forEach(snap => names.add(snap.model_name));
    return Array.from(names);
  }, [snapshots]);

  // Custom tooltip
  const CustomTooltip = ({ active, payload, label }: any) => {
    if (!active || !payload || payload.length === 0) return null;

    return (
      <div style={{
        backgroundColor: BLOOMBERG.PANEL_BG,
        border: `1px solid ${BLOOMBERG.BORDER}`,
        padding: '12px',
        fontSize: '10px',
        fontFamily: '"IBM Plex Mono", monospace',
      }}>
        <div style={{ color: BLOOMBERG.GRAY, marginBottom: '8px', fontSize: '9px' }}>
          CYCLE #{label}
        </div>
        {payload.map((entry: any, index: number) => (
          <div key={index} style={{
            display: 'flex',
            justifyContent: 'space-between',
            gap: '16px',
            marginBottom: '4px',
            alignItems: 'center'
          }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: '6px' }}>
              <div style={{
                width: '8px',
                height: '8px',
                backgroundColor: entry.color,
                borderRadius: '50%',
              }} />
              <span style={{ color: BLOOMBERG.WHITE, fontWeight: 600 }}>
                {entry.name}:
              </span>
            </div>
            <span style={{
              color: entry.value >= initialCapital ? BLOOMBERG.GREEN : BLOOMBERG.ORANGE,
              fontWeight: 700,
              fontFamily: '"IBM Plex Mono", monospace',
            }}>
              ${entry.value.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}
            </span>
          </div>
        ))}
      </div>
    );
  };

  // Custom legend
  const CustomLegend = ({ payload }: any) => {
    return (
      <div style={{
        display: 'flex',
        flexWrap: 'wrap',
        gap: '16px',
        justifyContent: 'center',
        padding: '12px 0 0 0',
      }}>
        {payload.map((entry: any, index: number) => (
          <div key={index} style={{
            display: 'flex',
            alignItems: 'center',
            gap: '6px',
            fontSize: '10px',
            fontFamily: '"IBM Plex Mono", monospace',
          }}>
            <div style={{
              width: '12px',
              height: '3px',
              backgroundColor: entry.color,
            }} />
            <span style={{ color: BLOOMBERG.WHITE, fontWeight: 600 }}>
              {entry.value}
            </span>
          </div>
        ))}
      </div>
    );
  };

  if (chartData.length === 0) {
    return (
      <div style={{
        height: '100%',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'center',
        color: BLOOMBERG.GRAY,
        fontSize: '11px',
        fontFamily: '"IBM Plex Mono", monospace',
      }}>
        No performance data yet. Start the competition to see the chart.
      </div>
    );
  }

  return (
    <ResponsiveContainer width="100%" height="100%">
      <LineChart
        data={chartData}
        margin={{ top: 10, right: 30, left: 20, bottom: 10 }}
      >
        <CartesianGrid
          strokeDasharray="3 3"
          stroke={BLOOMBERG.BORDER}
          vertical={false}
        />
        <XAxis
          dataKey="cycle"
          stroke={BLOOMBERG.GRAY}
          tick={{ fill: BLOOMBERG.GRAY, fontSize: 10, fontFamily: '"IBM Plex Mono", monospace' }}
          label={{
            value: 'CYCLE',
            position: 'insideBottom',
            offset: -5,
            fill: BLOOMBERG.GRAY,
            fontSize: 10,
            fontFamily: '"IBM Plex Mono", monospace',
          }}
        />
        <YAxis
          stroke={BLOOMBERG.GRAY}
          tick={{ fill: BLOOMBERG.GRAY, fontSize: 10, fontFamily: '"IBM Plex Mono", monospace' }}
          tickFormatter={(value) => `$${(value / 1000).toFixed(1)}k`}
          domain={['dataMin - 500', 'dataMax + 500']}
          label={{
            value: 'PORTFOLIO VALUE',
            angle: -90,
            position: 'insideLeft',
            fill: BLOOMBERG.GRAY,
            fontSize: 10,
            fontFamily: '"IBM Plex Mono", monospace',
          }}
        />
        <Tooltip content={<CustomTooltip />} />
        <Legend content={<CustomLegend />} />

        {/* Baseline reference line at initial capital */}
        <Line
          type="monotone"
          dataKey={() => initialCapital}
          stroke={BLOOMBERG.BORDER}
          strokeWidth={1}
          strokeDasharray="5 5"
          dot={false}
          activeDot={false}
          name="Initial Capital"
        />

        {/* Model performance lines */}
        {modelNames.map((modelName, index) => (
          <Line
            key={modelName}
            type="monotone"
            dataKey={modelName}
            stroke={MODEL_COLORS[index % MODEL_COLORS.length]}
            strokeWidth={2}
            dot={{
              r: 3,
              fill: MODEL_COLORS[index % MODEL_COLORS.length],
              strokeWidth: 0,
            }}
            activeDot={{
              r: 5,
              fill: MODEL_COLORS[index % MODEL_COLORS.length],
              stroke: BLOOMBERG.WHITE,
              strokeWidth: 2,
            }}
            name={modelName}
          />
        ))}
      </LineChart>
    </ResponsiveContainer>
  );
};

export default AlphaArenaChart;
