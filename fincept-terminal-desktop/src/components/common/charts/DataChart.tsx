// DataChart — Generic SVG time-series chart
// Lifted from economics/EconomicsChart and made tab-agnostic.

import React from 'react';
import { FINCEPT_COLORS, CHART_WIDTH, CHART_HEIGHT } from './types';
import type { DataPoint } from './types';

interface DataChartProps {
  data: DataPoint[];
  sourceColor: string;
  formatValue: (val: number) => string;
  width?: number;
  height?: number;
}

// Smart date label formatter
const formatSmartLabel = (date: string, _index: number, allDates: string[], _totalLabels: number): string => {
  const monthNames = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
  const year = date.substring(0, 4) || '';
  const month = date.length >= 7 ? date.substring(5, 7) : '';
  const day = date.length >= 10 ? date.substring(8, 10) : '';
  const monthIdx = month ? parseInt(month, 10) - 1 : -1;
  const monthName = monthIdx >= 0 && monthIdx < 12 ? monthNames[monthIdx] : '';
  const shortYear = year.substring(2, 4);
  const firstYear = (allDates[0] || '').substring(0, 4);
  const lastYear = (allDates[allDates.length - 1] || '').substring(0, 4);
  const sameYear = firstYear === lastYear;
  const uniqueYears = new Set(allDates.map(d => d.substring(0, 4))).size;
  const uniqueMonths = new Set(allDates.map(d => d.substring(0, 7))).size;

  if (sameYear && day && monthName) return `${parseInt(day)} ${monthName}`;
  if (uniqueYears <= 2 && monthName && day) return `${parseInt(day)} ${monthName}'${shortYear}`;
  if (uniqueMonths > uniqueYears * 2 && monthName) return `${monthName}'${shortYear}`;
  return year;
};

const filterSignificantPoints = (data: DataPoint[]): DataPoint[] => {
  if (data.length <= 2) return data;
  const result: DataPoint[] = [data[0]];
  for (let i = 1; i < data.length - 1; i++) {
    if (data[i].value !== data[i - 1].value || data[i].value !== data[i + 1].value) {
      result.push(data[i]);
    }
  }
  result.push(data[data.length - 1]);
  return result;
};

const hasSignificantRepetition = (data: DataPoint[]): boolean => {
  if (data.length < 10) return false;
  let same = 0;
  for (let i = 1; i < data.length; i++) if (data[i].value === data[i - 1].value) same++;
  return same / data.length > 0.5;
};

const generateLabelIndices = (data: DataPoint[], maxLabels: number): number[] => {
  if (data.length === 0) return [];
  if (data.length <= maxLabels) return data.map((_, i) => i);
  const indices: number[] = [];
  const step = (data.length - 1) / (maxLabels - 1);
  for (let i = 0; i < maxLabels; i++) {
    const idx = Math.round(i * step);
    if (indices.length === 0 || indices[indices.length - 1] !== idx) indices.push(idx);
  }
  if (indices[indices.length - 1] !== data.length - 1) indices.push(data.length - 1);
  return indices;
};

export function DataChart({ data: rawData, sourceColor, formatValue, width: w, height: h }: DataChartProps) {
  if (!rawData || rawData.length === 0) return null;

  const data = hasSignificantRepetition(rawData) ? filterSignificantPoints(rawData) : rawData;
  const width = w ?? CHART_WIDTH;
  const height = h ?? CHART_HEIGHT;
  const padding = { top: 30, right: 40, bottom: 50, left: 80 };
  const chartWidth = width - padding.left - padding.right;
  const chartHeight = height - padding.top - padding.bottom;

  const values = data.map(d => d.value);
  const minVal = Math.min(...values);
  const maxVal = Math.max(...values);
  const range = maxVal - minVal || 1;

  const points = data.map((d, i) => ({
    x: padding.left + (i / (data.length - 1 || 1)) * chartWidth,
    y: padding.top + chartHeight - ((d.value - minVal) / range) * chartHeight,
    data: d,
  }));

  const pathD = points.map((p, i) => `${i === 0 ? 'M' : 'L'} ${p.x} ${p.y}`).join(' ');
  const areaD = `${pathD} L ${points[points.length - 1].x} ${padding.top + chartHeight} L ${padding.left} ${padding.top + chartHeight} Z`;
  const allDates = data.map(d => d.date);
  const maxLabels = Math.max(2, Math.min(15, Math.floor(chartWidth / 70)));
  const labelIndices = generateLabelIndices(data, maxLabels);

  return (
    <svg width={width} height={height} style={{ display: 'block', margin: '0 auto', maxWidth: '100%' }}>
      <rect width={width} height={height} fill={FINCEPT_COLORS.PANEL_BG} />
      {[0, 1, 2, 3, 4, 5].map(i => {
        const y = padding.top + (chartHeight * i) / 5;
        const val = maxVal - (range * i) / 5;
        return (
          <g key={i}>
            <line x1={padding.left} y1={y} x2={width - padding.right} y2={y} stroke={FINCEPT_COLORS.BORDER} strokeDasharray="4,4" />
            <text x={padding.left - 12} y={y + 4} textAnchor="end" fill={FINCEPT_COLORS.WHITE} fontSize="11" fontFamily="monospace">{formatValue(val)}</text>
          </g>
        );
      })}
      {labelIndices.map(idx => {
        const x = padding.left + (idx / (data.length - 1 || 1)) * chartWidth;
        return <line key={`vg-${idx}`} x1={x} y1={padding.top} x2={x} y2={padding.top + chartHeight} stroke={FINCEPT_COLORS.BORDER} strokeDasharray="2,4" strokeOpacity="0.5" />;
      })}
      <path d={areaD} fill={`${sourceColor}25`} />
      <path d={pathD} fill="none" stroke={sourceColor} strokeWidth="2.5" />
      {labelIndices.map(idx => {
        const d = data[idx];
        const x = padding.left + (idx / (data.length - 1 || 1)) * chartWidth;
        return <text key={`xl-${idx}`} x={x} y={height - 15} textAnchor="middle" fill={FINCEPT_COLORS.WHITE} fontSize="10" fontFamily="monospace">{formatSmartLabel(d.date, idx, allDates, labelIndices.length)}</text>;
      })}
      {points.map((p, i) => <circle key={i} cx={p.x} cy={p.y} r={data.length > 30 ? 2 : 3} fill={sourceColor} />)}
      <line x1={padding.left} y1={padding.top + chartHeight} x2={width - padding.right} y2={padding.top + chartHeight} stroke={FINCEPT_COLORS.BORDER} strokeWidth="1" />
      <line x1={padding.left} y1={padding.top} x2={padding.left} y2={padding.top + chartHeight} stroke={FINCEPT_COLORS.BORDER} strokeWidth="1" />
    </svg>
  );
}

export default DataChart;
