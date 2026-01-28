import React, { memo, useMemo } from 'react';
import type { DataPoint, ChartType } from '../types';

// Fincept Design System Colors
const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
};

const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

interface DBnomicsChartProps {
  seriesArray: DataPoint[];
  chartType: ChartType;
  compact?: boolean;
}

function DBnomicsChart({ seriesArray, chartType, compact = false }: DBnomicsChartProps) {
  // Memoize all chart calculations
  const chartData = useMemo(() => {
    if (seriesArray.length === 0) return null;

    const h = compact ? 240 : 400;
    const ml = 60;
    const mr = 20;
    const mt = 20;
    const mb = 50;
    const actualWidth = compact ? 380 : 800;
    const pw = actualWidth - ml - mr;
    const ph = h - mt - mb;

    // Aggregate all observations
    const allValues = seriesArray.flatMap(s => s.observations.map(o => o.value));
    const yMin = Math.min(...allValues) * 0.9;
    const yMax = Math.max(...allValues) * 1.1;
    const yRange = yMax - yMin || 1;

    // Pre-sort observations for each series
    const sortedSeries = seriesArray.map(series => ({
      ...series,
      sortedObs: [...series.observations].sort(
        (a, b) => new Date(a.period).getTime() - new Date(b.period).getTime()
      ),
    }));

    return {
      h,
      ml,
      mr,
      mt,
      mb,
      actualWidth,
      pw,
      ph,
      yMin,
      yMax,
      yRange,
      sortedSeries,
    };
  }, [seriesArray, compact]);

  // Memoize Y-axis grid data
  const yAxisData = useMemo(() => {
    if (!chartData) return [];
    const { yMin, yRange, h, mb, ph } = chartData;
    return [0, 1, 2, 3, 4].map(i => ({
      val: yMin + (yRange * i) / 4,
      y: h - mb - (ph * i) / 4,
    }));
  }, [chartData]);

  // Memoize X-axis labels
  const xAxisLabels = useMemo(() => {
    if (!chartData || chartData.sortedSeries.length === 0) return [];
    const { sortedSeries, ml, pw, h, mb } = chartData;
    const firstSortedObs = sortedSeries[0].sortedObs;

    return firstSortedObs
      .map((obs, idx, arr) => {
        if (idx % Math.max(1, Math.floor(arr.length / 6)) !== 0 && idx !== arr.length - 1) {
          return null;
        }
        const x = ml + (pw * idx) / (arr.length - 1 || 1);
        return { x, y: h - mb + 15, label: obs.period.substring(0, 7) };
      })
      .filter(Boolean);
  }, [chartData]);

  if (!chartData || seriesArray.length === 0) return null;

  const { h, ml, mr, mt, mb, actualWidth, pw, ph, yMin, yRange, sortedSeries } = chartData;

  return (
    <svg
      width={compact ? '100%' : 800}
      height={h}
      viewBox={`0 0 ${actualWidth} ${h}`}
      preserveAspectRatio="xMidYMid meet"
      style={{
        backgroundColor: FINCEPT.PANEL_BG,
        width: '100%',
        height: 'auto',
        maxHeight: compact ? '240px' : '400px',
      }}
    >
      {/* Axes */}
      <line x1={ml} y1={mt} x2={ml} y2={h - mb} stroke={FINCEPT.BORDER} strokeWidth="1" />
      <line x1={ml} y1={h - mb} x2={actualWidth - mr} y2={h - mb} stroke={FINCEPT.BORDER} strokeWidth="1" />

      {/* Y-axis grid and labels */}
      {yAxisData.map((item, i) => (
        <g key={i}>
          <line x1={ml} y1={item.y} x2={actualWidth - mr} y2={item.y} stroke={FINCEPT.HEADER_BG} strokeDasharray="2,2" />
          <text x={ml - 8} y={item.y + 3} textAnchor="end" fill={FINCEPT.GRAY} fontSize="9" fontFamily={FONT_FAMILY}>
            {item.val.toFixed(1)}
          </text>
        </g>
      ))}

      {/* Render each series */}
      {sortedSeries.map((series, sIdx) => {
        const sorted = series.sortedObs;

        return (
          <g key={sIdx}>
            {/* Area fill for area chart */}
            {chartType === 'area' && (
              <polygon
                points={(() => {
                  const points = sorted
                    .map((o, i) => {
                      const xi = ml + (pw * i) / (sorted.length - 1 || 1);
                      const oValue = typeof o.value === 'number' ? o.value : Number(o.value);
                      const yi = h - mb - (ph * ((oValue - yMin) / yRange));
                      return `${xi},${yi}`;
                    })
                    .join(' ');
                  return `${points} ${actualWidth - mr},${h - mb} ${ml},${h - mb}`;
                })()}
                fill={series.color}
                opacity="0.2"
              />
            )}

            {sorted.map((obs, idx) => {
              const x = ml + (pw * idx) / (sorted.length - 1 || 1);
              const y = h - mb - (ph * ((Number(obs.value) - yMin) / yRange));
              const barWidth = (pw / sorted.length) * 0.8;

              if (chartType === 'line' || chartType === 'area') {
                return (
                  <g key={idx}>
                    <circle cx={x} cy={y} r="2" fill={series.color} />
                    {idx < sorted.length - 1 && (
                      <line
                        x1={x}
                        y1={y}
                        x2={ml + (pw * (idx + 1)) / (sorted.length - 1 || 1)}
                        y2={h - mb - (ph * ((Number(sorted[idx + 1].value) - yMin) / yRange))}
                        stroke={series.color}
                        strokeWidth="1.5"
                      />
                    )}
                  </g>
                );
              } else if (chartType === 'bar') {
                return (
                  <rect
                    key={idx}
                    x={x - barWidth / 2 + (sIdx * barWidth) / seriesArray.length}
                    y={y}
                    width={barWidth / seriesArray.length}
                    height={h - mb - y}
                    fill={series.color}
                    opacity="0.8"
                  />
                );
              } else if (chartType === 'scatter') {
                return <circle key={idx} cx={x} cy={y} r="3" fill={series.color} opacity="0.7" />;
              } else if (chartType === 'candlestick') {
                const nextObs = sorted[idx + 1];
                const open = obs.value;
                const close = nextObs ? nextObs.value : obs.value;
                const high = Math.max(open, close);
                const low = Math.min(open, close);
                const yHigh = h - mb - (ph * ((high - yMin) / yRange));
                const yLow = h - mb - (ph * ((low - yMin) / yRange));
                const yOpen = h - mb - (ph * ((open - yMin) / yRange));
                const yClose = h - mb - (ph * ((close - yMin) / yRange));

                return (
                  <g key={idx}>
                    <line x1={x} y1={yHigh} x2={x} y2={yLow} stroke={series.color} strokeWidth="1" />
                    <rect
                      x={x - 3}
                      y={Math.min(yOpen, yClose)}
                      width={6}
                      height={Math.abs(yClose - yOpen) || 1}
                      fill={close >= open ? FINCEPT.GREEN : FINCEPT.RED}
                    />
                  </g>
                );
              }
              return null;
            })}
          </g>
        );
      })}

      {/* X-axis labels */}
      {xAxisLabels.map((item, idx) =>
        item ? (
          <text
            key={idx}
            x={item.x}
            y={item.y}
            textAnchor="middle"
            fill={FINCEPT.GRAY}
            fontSize="8"
            fontFamily={FONT_FAMILY}
          >
            {item.label}
          </text>
        ) : null
      )}

      {/* Legend */}
      {sortedSeries.map((series, idx) => (
        <g key={idx}>
          <rect x={ml + idx * 120} y={5} width={10} height={10} fill={series.color} rx="1" />
          <text x={ml + idx * 120 + 15} y={13} fill={FINCEPT.WHITE} fontSize="9" fontFamily={FONT_FAMILY}>
            {series.series_name.substring(0, 12)}
          </text>
        </g>
      ))}
    </svg>
  );
}

export default memo(DBnomicsChart);
