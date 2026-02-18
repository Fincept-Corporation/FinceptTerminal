/**
 * Performance Chart Component
 *
 * Displays portfolio value over time for all competing models.
 * Uses Area + Line for visual depth, proper axis scaling,
 * and ensures the graph plots correctly even with few data points.
 */

import React, { useId, useMemo, useState } from 'react';
import {
  AreaChart,
  Area,
  XAxis,
  YAxis,
  Tooltip,
  Legend,
  ResponsiveContainer,
  CartesianGrid,
  ReferenceLine,
} from 'recharts';
import { TrendingUp, BarChart3 } from 'lucide-react';
import { withErrorBoundary } from '@/components/common/ErrorBoundary';
import type { PerformanceSnapshot, ChartData } from '../types';
import { formatCurrency, MODEL_COLORS } from '../types';

const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  GRAY: '#787878',
  GREEN: '#00D66F',
  RED: '#FF3B3B',
  CYAN: '#00E5FF',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CARD_BG: '#0A0A0A',
  BORDER: '#2A2A2A',
  MUTED: '#4A4A4A',
};

const TERMINAL_FONT = '"IBM Plex Mono", "Consolas", monospace';

// Fallback colors for models not in MODEL_COLORS
const EXTRA_COLORS = [
  '#10B981', '#3B82F6', '#F97316', '#8B5CF6', '#EC4899',
  '#14B8A6', '#F59E0B', '#06B6D4', '#EF4444', '#84CC16',
  '#A855F7', '#22D3EE', '#FB923C', '#4ADE80', '#818CF8',
];

function getColor(modelName: string, index: number): string {
  return MODEL_COLORS[modelName] || EXTRA_COLORS[index % EXTRA_COLORS.length] || '#6B7280';
}

interface PerformanceChartProps {
  snapshots: PerformanceSnapshot[];
  height?: number;
  showLegend?: boolean;
}

type ViewMode = 'value' | 'return';

const PerformanceChart: React.FC<PerformanceChartProps> = ({
  snapshots,
  height = 300,
  showLegend = true,
}) => {
  const [viewMode, setViewMode] = useState<ViewMode>('value');
  const [hoveredModel, setHoveredModel] = useState<string | null>(null);
  // Unique ID prefix to avoid SVG gradient ID collisions across multiple chart instances
  const chartId = useId().replace(/:/g, '');

  // Transform snapshots into chart data
  const { chartData, modelNames, initialCapital, yDomain, returnChartData, returnYDomain } = useMemo(() => {
    if (!Array.isArray(snapshots) || !snapshots.length) {
      return { chartData: [], modelNames: [], initialCapital: 0, yDomain: [0, 0] as [number, number], returnChartData: [], returnYDomain: [0, 0] as [number, number] };
    }

    // Group by cycle
    const cycleData = new Map<number, ChartData[number]>();
    const returnCycleData = new Map<number, ChartData[number]>();
    const models = new Set<string>();
    let detectedInitialCapital = 0;

    // First pass: collect all models and find initial capital
    snapshots.forEach((snap) => {
      models.add(snap.model_name);
      if (snap.cycle_number === 1 && detectedInitialCapital === 0) {
        // Approximate initial capital from first cycle: portfolio_value - pnl
        detectedInitialCapital = snap.portfolio_value - (snap.pnl || 0);
      }
    });

    // If we couldn't detect initial capital, use the earliest snapshot value
    if (detectedInitialCapital === 0) {
      const earliest = snapshots.reduce((min, s) =>
        s.cycle_number < min.cycle_number ? s : min, snapshots[0]);
      detectedInitialCapital = earliest.portfolio_value;
    }

    // Add cycle 0 (initial capital baseline) for all models
    const cycle0: ChartData[number] = { cycle: 0, timestamp: '' };
    const returnCycle0: ChartData[number] = { cycle: 0, timestamp: '' };
    models.forEach(m => {
      cycle0[m] = detectedInitialCapital;
      returnCycle0[m] = 0;
    });
    cycleData.set(0, cycle0);
    returnCycleData.set(0, returnCycle0);

    // Track min/max for Y-axis domain
    let minVal = detectedInitialCapital;
    let maxVal = detectedInitialCapital;
    let minReturn = 0;
    let maxReturn = 0;

    // Second pass: populate data
    snapshots.forEach((snap) => {
      const cycleNum = Number.isFinite(snap.cycle_number) ? snap.cycle_number : 0;
      if (cycleNum === 0) return; // Skip, we already added the baseline

      if (!cycleData.has(cycleNum)) {
        cycleData.set(cycleNum, { cycle: cycleNum, timestamp: snap.timestamp });
        returnCycleData.set(cycleNum, { cycle: cycleNum, timestamp: snap.timestamp });
      }

      const data = cycleData.get(cycleNum)!;
      const returnData = returnCycleData.get(cycleNum)!;
      const val = Number.isFinite(snap.portfolio_value) ? snap.portfolio_value : 0;
      const retPct = Number.isFinite(snap.return_pct) ? snap.return_pct : 0;

      data[snap.model_name] = val;
      returnData[snap.model_name] = retPct;

      if (val < minVal) minVal = val;
      if (val > maxVal) maxVal = val;
      if (retPct < minReturn) minReturn = retPct;
      if (retPct > maxReturn) maxReturn = retPct;
    });

    // For cycles where a model has no data, carry forward the last known value
    const sortedCycles = Array.from(cycleData.keys()).sort((a, b) => a - b);
    const modelArray = Array.from(models);
    const lastKnown: Record<string, number> = {};
    const lastKnownReturn: Record<string, number> = {};
    modelArray.forEach(m => {
      lastKnown[m] = detectedInitialCapital;
      lastKnownReturn[m] = 0;
    });

    sortedCycles.forEach(cycle => {
      const data = cycleData.get(cycle)!;
      const returnData = returnCycleData.get(cycle)!;
      modelArray.forEach(m => {
        if (data[m] !== undefined) {
          lastKnown[m] = data[m] as number;
        } else {
          data[m] = lastKnown[m];
        }
        if (returnData[m] !== undefined) {
          lastKnownReturn[m] = returnData[m] as number;
        } else {
          returnData[m] = lastKnownReturn[m];
        }
      });
    });

    // Compute Y-axis domain with padding
    const valRange = maxVal - minVal;
    const padding = valRange > 0 ? valRange * 0.1 : detectedInitialCapital * 0.02;
    const computedYDomain: [number, number] = [
      Math.floor((minVal - padding) / 100) * 100,
      Math.ceil((maxVal + padding) / 100) * 100,
    ];

    const retRange = maxReturn - minReturn;
    const retPadding = retRange > 0 ? retRange * 0.15 : 1;
    const computedReturnYDomain: [number, number] = [
      Math.floor((minReturn - retPadding) * 10) / 10,
      Math.ceil((maxReturn + retPadding) * 10) / 10,
    ];

    const sorted = sortedCycles.map(cycle => cycleData.get(cycle)!);
    const returnSorted = sortedCycles.map(cycle => returnCycleData.get(cycle)!);

    return {
      chartData: sorted,
      modelNames: modelArray,
      initialCapital: detectedInitialCapital,
      yDomain: computedYDomain,
      returnChartData: returnSorted,
      returnYDomain: computedReturnYDomain,
    };
  }, [snapshots]);

  // Stat summary per model from the latest data point
  const modelStats = useMemo(() => {
    if (chartData.length === 0 || modelNames.length === 0) return [];
    const latest = chartData[chartData.length - 1];
    return modelNames.map((name, idx) => {
      const value = (latest[name] as number) || initialCapital;
      const pnl = value - initialCapital;
      const retPct = initialCapital > 0 ? (pnl / initialCapital) * 100 : 0;
      return { name, value, pnl, retPct, color: getColor(name, idx) };
    }).sort((a, b) => b.value - a.value);
  }, [chartData, modelNames, initialCapital]);

  if (chartData.length === 0) {
    return (
      <div style={{
        backgroundColor: FINCEPT.PANEL_BG,
        border: `1px solid ${FINCEPT.BORDER}`,
        fontFamily: TERMINAL_FONT,
      }}>
        <div style={{
          display: 'flex',
          alignItems: 'center',
          gap: '8px',
          padding: '8px 12px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          backgroundColor: FINCEPT.HEADER_BG,
        }}>
          <TrendingUp size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ color: FINCEPT.WHITE, fontWeight: 700, fontSize: '11px', letterSpacing: '0.5px' }}>
            PERFORMANCE
          </span>
        </div>
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          color: FINCEPT.GRAY,
          fontSize: '11px',
          height,
          gap: '8px',
        }}>
          <BarChart3 size={28} style={{ opacity: 0.3 }} />
          <span>Run cycles to see performance data</span>
        </div>
      </div>
    );
  }

  const activeData = viewMode === 'value' ? chartData : returnChartData;
  const activeDomain = viewMode === 'value' ? yDomain : returnYDomain;

  return (
    <div style={{
      backgroundColor: FINCEPT.PANEL_BG,
      border: `1px solid ${FINCEPT.BORDER}`,
      fontFamily: TERMINAL_FONT,
    }}>
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '8px 12px',
        borderBottom: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.HEADER_BG,
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <TrendingUp size={14} style={{ color: FINCEPT.ORANGE }} />
          <span style={{ color: FINCEPT.WHITE, fontWeight: 700, fontSize: '11px', letterSpacing: '0.5px' }}>
            PERFORMANCE
          </span>
          <span style={{
            fontSize: '9px',
            padding: '1px 6px',
            backgroundColor: FINCEPT.BORDER,
            color: FINCEPT.GRAY,
            fontWeight: 600,
          }}>
            {chartData.length - 1} CYCLES
          </span>
        </div>

        {/* View Mode Toggle */}
        <div style={{ display: 'flex', gap: '2px' }}>
          {(['value', 'return'] as ViewMode[]).map(mode => (
            <button
              key={mode}
              onClick={() => setViewMode(mode)}
              style={{
                padding: '3px 8px',
                fontSize: '9px',
                fontWeight: 600,
                fontFamily: TERMINAL_FONT,
                letterSpacing: '0.5px',
                cursor: 'pointer',
                border: 'none',
                backgroundColor: viewMode === mode ? FINCEPT.ORANGE : 'transparent',
                color: viewMode === mode ? FINCEPT.DARK_BG : FINCEPT.GRAY,
                transition: 'all 0.15s',
              }}
              onMouseEnter={e => {
                if (viewMode !== mode) {
                  e.currentTarget.style.backgroundColor = FINCEPT.BORDER;
                  e.currentTarget.style.color = FINCEPT.WHITE;
                }
              }}
              onMouseLeave={e => {
                if (viewMode !== mode) {
                  e.currentTarget.style.backgroundColor = 'transparent';
                  e.currentTarget.style.color = FINCEPT.GRAY;
                }
              }}
            >
              {mode === 'value' ? 'VALUE' : 'RETURN %'}
            </button>
          ))}
        </div>
      </div>

      {/* Model Stats Bar */}
      {modelStats.length > 0 && (
        <div style={{
          display: 'flex',
          gap: '8px',
          padding: '6px 12px',
          borderBottom: `1px solid ${FINCEPT.BORDER}`,
          overflowX: 'auto',
          flexWrap: 'nowrap',
        }}>
          {modelStats.map(stat => (
            <div
              key={stat.name}
              style={{
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                padding: '3px 8px',
                backgroundColor: hoveredModel === stat.name ? `${stat.color}15` : FINCEPT.CARD_BG,
                border: `1px solid ${hoveredModel === stat.name ? stat.color + '40' : FINCEPT.BORDER}`,
                cursor: 'pointer',
                transition: 'all 0.15s',
                flexShrink: 0,
              }}
              onMouseEnter={() => setHoveredModel(stat.name)}
              onMouseLeave={() => setHoveredModel(null)}
            >
              <div style={{ width: '8px', height: '8px', backgroundColor: stat.color, flexShrink: 0 }} />
              <span style={{ fontSize: '9px', color: FINCEPT.WHITE, fontWeight: 600, whiteSpace: 'nowrap' }}>
                {stat.name}
              </span>
              <span style={{
                fontSize: '9px',
                fontWeight: 700,
                color: stat.pnl >= 0 ? FINCEPT.GREEN : FINCEPT.RED,
                fontFamily: TERMINAL_FONT,
              }}>
                {stat.pnl >= 0 ? '+' : ''}{stat.retPct.toFixed(2)}%
              </span>
            </div>
          ))}
        </div>
      )}

      {/* Chart */}
      <div style={{ padding: '8px 4px 4px 4px' }}>
        <ResponsiveContainer width="100%" height={height}>
          <AreaChart
            data={activeData}
            margin={{ top: 10, right: 20, left: 10, bottom: 5 }}
          >
            <defs>
              {modelNames.map((modelName, idx) => {
                const color = getColor(modelName, idx);
                return (
                  <linearGradient key={modelName} id={`grad-${chartId}-${idx}`} x1="0" y1="0" x2="0" y2="1">
                    <stop offset="0%" stopColor={color} stopOpacity={hoveredModel === modelName ? 0.35 : 0.15} />
                    <stop offset="100%" stopColor={color} stopOpacity={0} />
                  </linearGradient>
                );
              })}
            </defs>

            <CartesianGrid
              strokeDasharray="3 3"
              stroke={FINCEPT.BORDER}
              vertical={false}
            />

            <XAxis
              dataKey="cycle"
              stroke={FINCEPT.MUTED}
              fontSize={9}
              fontFamily={TERMINAL_FONT}
              tickFormatter={(value) => `#${value}`}
              tick={{ fill: FINCEPT.GRAY }}
              axisLine={{ stroke: FINCEPT.BORDER }}
              tickLine={{ stroke: FINCEPT.BORDER }}
            />

            <YAxis
              stroke={FINCEPT.MUTED}
              fontSize={9}
              fontFamily={TERMINAL_FONT}
              tickFormatter={(value) =>
                viewMode === 'value'
                  ? value >= 1000 ? `$${(value / 1000).toFixed(1)}k` : `$${value}`
                  : `${value.toFixed(1)}%`
              }
              domain={activeDomain}
              tick={{ fill: FINCEPT.GRAY }}
              axisLine={{ stroke: FINCEPT.BORDER }}
              tickLine={{ stroke: FINCEPT.BORDER }}
              width={55}
            />

            {/* Reference line at initial capital / 0% return */}
            <ReferenceLine
              y={viewMode === 'value' ? initialCapital : 0}
              stroke={FINCEPT.MUTED}
              strokeDasharray="6 4"
              strokeWidth={1}
              label={{
                value: viewMode === 'value' ? 'Initial' : '0%',
                position: 'right',
                fill: FINCEPT.MUTED,
                fontSize: 9,
                fontFamily: TERMINAL_FONT,
              }}
            />

            <Tooltip
              contentStyle={{
                backgroundColor: FINCEPT.HEADER_BG,
                border: `1px solid ${FINCEPT.BORDER}`,
                borderRadius: 0,
                fontFamily: TERMINAL_FONT,
                fontSize: '10px',
                padding: '8px 10px',
              }}
              labelStyle={{ color: FINCEPT.ORANGE, fontFamily: TERMINAL_FONT, fontWeight: 700, marginBottom: '4px' }}
              itemStyle={{ padding: '1px 0' }}
              formatter={(value: number, name: string) => {
                if (viewMode === 'value') {
                  const pnl = value - initialCapital;
                  const pnlStr = pnl >= 0 ? `+${formatCurrency(pnl)}` : formatCurrency(pnl);
                  return [`${formatCurrency(value)}  (${pnlStr})`, name];
                }
                return [`${value >= 0 ? '+' : ''}${value.toFixed(2)}%`, name];
              }}
              labelFormatter={(label) => `Cycle ${label}`}
              cursor={{ stroke: FINCEPT.ORANGE, strokeWidth: 1, strokeDasharray: '4 4' }}
            />

            {showLegend && (
              <Legend
                wrapperStyle={{ paddingTop: '8px', fontFamily: TERMINAL_FONT, fontSize: '9px' }}
                formatter={(value) => (
                  <span style={{
                    color: hoveredModel === value ? FINCEPT.WHITE : FINCEPT.GRAY,
                    fontFamily: TERMINAL_FONT,
                    fontWeight: hoveredModel === value ? 700 : 400,
                    transition: 'all 0.15s',
                  }}>
                    {value}
                  </span>
                )}
                onMouseEnter={(e) => setHoveredModel(e.value as string)}
                onMouseLeave={() => setHoveredModel(null)}
              />
            )}

            {modelNames.map((modelName, idx) => {
              const color = getColor(modelName, idx);
              const isHovered = hoveredModel === modelName;
              const isOtherHovered = hoveredModel !== null && hoveredModel !== modelName;

              return (
                <Area
                  key={modelName}
                  type="monotone"
                  dataKey={modelName}
                  stroke={color}
                  strokeWidth={isHovered ? 3 : isOtherHovered ? 1 : 2}
                  fill={`url(#grad-${chartId}-${idx})`}
                  fillOpacity={isOtherHovered ? 0.3 : 1}
                  strokeOpacity={isOtherHovered ? 0.4 : 1}
                  dot={{
                    fill: color,
                    stroke: FINCEPT.DARK_BG,
                    strokeWidth: 1,
                    r: isHovered ? 4 : 3,
                  }}
                  activeDot={{
                    fill: color,
                    stroke: FINCEPT.WHITE,
                    strokeWidth: 2,
                    r: 5,
                  }}
                  connectNulls
                  animationDuration={500}
                  animationEasing="ease-out"
                />
              );
            })}
          </AreaChart>
        </ResponsiveContainer>
      </div>

      {/* Footer Stats */}
      <div style={{
        display: 'flex',
        justifyContent: 'space-between',
        alignItems: 'center',
        padding: '4px 12px',
        borderTop: `1px solid ${FINCEPT.BORDER}`,
        backgroundColor: FINCEPT.HEADER_BG,
        fontSize: '9px',
        color: FINCEPT.GRAY,
      }}>
        <span>
          Initial: <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>{formatCurrency(initialCapital)}</span>
        </span>
        <span style={{ color: FINCEPT.MUTED }}>|</span>
        <span>
          Models: <span style={{ color: FINCEPT.WHITE, fontWeight: 600 }}>{modelNames.length}</span>
        </span>
        <span style={{ color: FINCEPT.MUTED }}>|</span>
        {modelStats.length > 0 && (
          <span>
            Best: <span style={{ color: FINCEPT.GREEN, fontWeight: 600 }}>
              {modelStats[0].name} ({modelStats[0].retPct >= 0 ? '+' : ''}{modelStats[0].retPct.toFixed(2)}%)
            </span>
          </span>
        )}
      </div>
    </div>
  );
};

export default withErrorBoundary(PerformanceChart, { name: 'AlphaArena.PerformanceChart' });
