/**
 * ForecastChart Component
 * Interactive SVG-based line chart with tooltips for time series visualization
 */

import React, { useState, useMemo, useRef } from 'react';
import { FINCEPT } from '../constants';
import type { ForecastChartProps, ChartDataPoint, TooltipState } from '../types';

export const ForecastChart: React.FC<ForecastChartProps> = ({
  historicalData,
  forecastData,
  height = 300
}) => {
  const [tooltip, setTooltip] = useState<TooltipState>({ visible: false, x: 0, y: 0, data: null });
  const [zoomLevel, setZoomLevel] = useState(1);
  const [panOffset, setPanOffset] = useState(0);
  const svgRef = useRef<SVGSVGElement>(null);

  const chartData = useMemo(() => {
    const historical: ChartDataPoint[] = historicalData.map(d => ({
      time: d.time,
      value: d.value,
      type: 'historical' as const
    }));

    const forecast: ChartDataPoint[] = forecastData.map(d => ({
      time: d.time,
      value: d.value,
      type: 'forecast' as const,
      entity_id: d.entity_id
    }));

    return [...historical, ...forecast];
  }, [historicalData, forecastData]);

  if (chartData.length === 0) {
    return (
      <div
        className="flex items-center justify-center"
        style={{ height, backgroundColor: FINCEPT.DARK_BG }}
      >
        <span className="text-sm font-mono" style={{ color: FINCEPT.GRAY }}>
          No data to display
        </span>
      </div>
    );
  }

  const baseWidth = 800;
  const width = baseWidth * zoomLevel;
  const padding = { top: 20, right: 60, bottom: 40, left: 60 };
  const chartWidth = width - padding.left - padding.right;
  const chartHeight = height - padding.top - padding.bottom;

  // Calculate scales
  const values = chartData.map(d => d.value);
  const minValue = Math.min(...values);
  const maxValue = Math.max(...values);
  const valueRange = maxValue - minValue || 1;
  const yMin = minValue - valueRange * 0.1;
  const yMax = maxValue + valueRange * 0.1;

  const xScale = (index: number) => padding.left + (index / (chartData.length - 1 || 1)) * chartWidth;
  const yScale = (value: number) => padding.top + chartHeight - ((value - yMin) / (yMax - yMin)) * chartHeight;

  // Find split point between historical and forecast
  const historicalEndIndex = historicalData.length - 1;

  // Generate path for historical data
  const historicalPath = historicalData
    .map((d, i) => {
      const x = xScale(i);
      const y = yScale(d.value);
      return `${i === 0 ? 'M' : 'L'} ${x} ${y}`;
    })
    .join(' ');

  // Generate path for forecast data (starts from last historical point)
  const forecastPath = forecastData.length > 0
    ? [
        historicalData.length > 0
          ? `M ${xScale(historicalEndIndex)} ${yScale(historicalData[historicalEndIndex]?.value ?? forecastData[0].value)}`
          : `M ${xScale(0)} ${yScale(forecastData[0].value)}`,
        ...forecastData.map((d, i) => {
          const x = xScale(historicalData.length + i);
          const y = yScale(d.value);
          return `L ${x} ${y}`;
        })
      ].join(' ')
    : '';

  // Y-axis ticks
  const yTicks = Array.from({ length: 5 }, (_, i) => {
    const value = yMin + (yMax - yMin) * (i / 4);
    return { value, y: yScale(value) };
  });

  // X-axis labels (show first, middle, last, and split point)
  const xLabels = [
    { index: 0, label: chartData[0]?.time.split('T')[0] || '' },
    { index: historicalEndIndex, label: chartData[historicalEndIndex]?.time.split('T')[0] || '' },
    { index: chartData.length - 1, label: chartData[chartData.length - 1]?.time.split('T')[0] || '' }
  ].filter((item, index, arr) =>
    arr.findIndex(t => t.index === item.index) === index && item.label
  );

  // Handle mouse events for interactive tooltips
  const handleMouseEnter = (data: ChartDataPoint, x: number, y: number) => {
    setTooltip({ visible: true, x, y, data });
  };

  const handleMouseLeave = () => {
    setTooltip({ visible: false, x: 0, y: 0, data: null });
  };

  // Zoom controls
  const handleZoomIn = () => setZoomLevel(prev => Math.min(prev + 0.5, 3));
  const handleZoomOut = () => setZoomLevel(prev => Math.max(prev - 0.5, 1));
  const handleResetZoom = () => { setZoomLevel(1); setPanOffset(0); };

  return (
    <div className="w-full">
      {/* Zoom Controls */}
      <div className="flex justify-end gap-2 mb-2">
        <button
          onClick={handleZoomOut}
          className="px-2 py-1 rounded text-xs font-mono"
          style={{ backgroundColor: FINCEPT.HEADER_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}` }}
          disabled={zoomLevel <= 1}
        >
          -
        </button>
        <span className="px-2 py-1 text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
          {Math.round(zoomLevel * 100)}%
        </span>
        <button
          onClick={handleZoomIn}
          className="px-2 py-1 rounded text-xs font-mono"
          style={{ backgroundColor: FINCEPT.HEADER_BG, color: FINCEPT.WHITE, border: `1px solid ${FINCEPT.BORDER}` }}
          disabled={zoomLevel >= 3}
        >
          +
        </button>
        {zoomLevel !== 1 && (
          <button
            onClick={handleResetZoom}
            className="px-2 py-1 rounded text-xs font-mono"
            style={{ backgroundColor: FINCEPT.HEADER_BG, color: FINCEPT.ORANGE, border: `1px solid ${FINCEPT.BORDER}` }}
          >
            Reset
          </button>
        )}
      </div>

      {/* Chart Container with horizontal scroll */}
      <div className="overflow-x-auto" style={{ position: 'relative' }}>
        <svg
          ref={svgRef}
          width={width}
          height={height}
          viewBox={`0 0 ${width} ${height}`}
          style={{ backgroundColor: FINCEPT.DARK_BG, minWidth: baseWidth }}
        >
          {/* Grid lines */}
          {yTicks.map((tick, i) => (
            <line
              key={i}
              x1={padding.left}
              y1={tick.y}
              x2={width - padding.right}
              y2={tick.y}
              stroke={FINCEPT.BORDER}
              strokeWidth={1}
              strokeDasharray="4,4"
            />
          ))}

          {/* Forecast region background */}
          {forecastData.length > 0 && (
            <rect
              x={xScale(historicalEndIndex)}
              y={padding.top}
              width={chartWidth - xScale(historicalEndIndex) + padding.left}
              height={chartHeight}
              fill={FINCEPT.ORANGE}
              opacity={0.05}
            />
          )}

          {/* Vertical line at forecast start */}
          {forecastData.length > 0 && historicalData.length > 0 && (
            <line
              x1={xScale(historicalEndIndex)}
              y1={padding.top}
              x2={xScale(historicalEndIndex)}
              y2={height - padding.bottom}
              stroke={FINCEPT.ORANGE}
              strokeWidth={1}
              strokeDasharray="6,4"
              opacity={0.6}
            />
          )}

          {/* Historical line */}
          {historicalPath && (
            <path
              d={historicalPath}
              fill="none"
              stroke={FINCEPT.CYAN}
              strokeWidth={2}
            />
          )}

          {/* Forecast line */}
          {forecastPath && (
            <path
              d={forecastPath}
              fill="none"
              stroke={FINCEPT.ORANGE}
              strokeWidth={2}
              strokeDasharray="6,3"
            />
          )}

          {/* Historical data points - Interactive */}
          {historicalData.map((d, i) => {
            const cx = xScale(i);
            const cy = yScale(d.value);
            const dataPoint: ChartDataPoint = { time: d.time, value: d.value, type: 'historical' };
            return (
              <g key={`hist-${i}`}>
                {/* Larger invisible hit area */}
                <circle
                  cx={cx}
                  cy={cy}
                  r={12}
                  fill="transparent"
                  style={{ cursor: 'pointer' }}
                  onMouseEnter={() => handleMouseEnter(dataPoint, cx, cy)}
                  onMouseLeave={handleMouseLeave}
                />
                {/* Visible point */}
                <circle
                  cx={cx}
                  cy={cy}
                  r={tooltip.data?.time === d.time && tooltip.data?.type === 'historical' ? 6 : 3}
                  fill={FINCEPT.CYAN}
                  style={{ transition: 'r 0.15s ease', pointerEvents: 'none' }}
                />
              </g>
            );
          })}

          {/* Forecast data points - Interactive */}
          {forecastData.map((d, i) => {
            const cx = xScale(historicalData.length + i);
            const cy = yScale(d.value);
            const dataPoint: ChartDataPoint = { time: d.time, value: d.value, type: 'forecast', entity_id: d.entity_id };
            return (
              <g key={`forecast-${i}`}>
                {/* Larger invisible hit area */}
                <circle
                  cx={cx}
                  cy={cy}
                  r={12}
                  fill="transparent"
                  style={{ cursor: 'pointer' }}
                  onMouseEnter={() => handleMouseEnter(dataPoint, cx, cy)}
                  onMouseLeave={handleMouseLeave}
                />
                {/* Visible point */}
                <circle
                  cx={cx}
                  cy={cy}
                  r={tooltip.data?.time === d.time && tooltip.data?.type === 'forecast' ? 7 : 4}
                  fill={FINCEPT.ORANGE}
                  stroke={FINCEPT.DARK_BG}
                  strokeWidth={1}
                  style={{ transition: 'r 0.15s ease', pointerEvents: 'none' }}
                />
              </g>
            );
          })}

          {/* Y-axis labels */}
          {yTicks.map((tick, i) => (
            <text
              key={i}
              x={padding.left - 10}
              y={tick.y + 4}
              textAnchor="end"
              fontSize={10}
              fontFamily="monospace"
              fill={FINCEPT.GRAY}
            >
              {tick.value.toFixed(2)}
            </text>
          ))}

          {/* X-axis labels */}
          {xLabels.map((item, i) => (
            <text
              key={i}
              x={xScale(item.index)}
              y={height - padding.bottom + 20}
              textAnchor="middle"
              fontSize={10}
              fontFamily="monospace"
              fill={FINCEPT.GRAY}
            >
              {item.label}
            </text>
          ))}

          {/* Legend */}
          <g transform={`translate(${width - padding.right - 120}, ${padding.top})`}>
            <rect x={0} y={0} width={120} height={50} fill={FINCEPT.PANEL_BG} rx={4} />
            <line x1={10} y1={15} x2={35} y2={15} stroke={FINCEPT.CYAN} strokeWidth={2} />
            <text x={42} y={18} fontSize={10} fontFamily="monospace" fill={FINCEPT.WHITE}>Historical</text>
            <line x1={10} y1={35} x2={35} y2={35} stroke={FINCEPT.ORANGE} strokeWidth={2} strokeDasharray="6,3" />
            <text x={42} y={38} fontSize={10} fontFamily="monospace" fill={FINCEPT.WHITE}>Forecast</text>
          </g>

          {/* Forecast label */}
          {forecastData.length > 0 && historicalData.length > 0 && (
            <text
              x={xScale(historicalEndIndex) + 5}
              y={padding.top + 15}
              fontSize={10}
              fontFamily="monospace"
              fill={FINCEPT.ORANGE}
            >
              Forecast
            </text>
          )}
        </svg>

        {/* Tooltip */}
        {tooltip.visible && tooltip.data && (
          <div
            className="absolute pointer-events-none px-3 py-2 rounded shadow-lg z-50"
            style={{
              left: Math.min(tooltip.x + 10, width - 180),
              top: Math.max(tooltip.y - 60, 10),
              backgroundColor: FINCEPT.PANEL_BG,
              border: `1px solid ${tooltip.data.type === 'forecast' ? FINCEPT.ORANGE : FINCEPT.CYAN}`,
              minWidth: 160
            }}
          >
            <div className="text-xs font-mono font-bold mb-1" style={{ color: tooltip.data.type === 'forecast' ? FINCEPT.ORANGE : FINCEPT.CYAN }}>
              {tooltip.data.type === 'forecast' ? 'FORECAST' : 'HISTORICAL'}
            </div>
            <div className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
              Date: <span style={{ color: FINCEPT.WHITE }}>{tooltip.data.time.split('T')[0]}</span>
            </div>
            <div className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
              Value: <span style={{ color: FINCEPT.GREEN, fontWeight: 'bold' }}>{tooltip.data.value.toFixed(4)}</span>
            </div>
            {tooltip.data.entity_id && (
              <div className="text-xs font-mono" style={{ color: FINCEPT.GRAY }}>
                Entity: <span style={{ color: FINCEPT.CYAN }}>{tooltip.data.entity_id}</span>
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
};
