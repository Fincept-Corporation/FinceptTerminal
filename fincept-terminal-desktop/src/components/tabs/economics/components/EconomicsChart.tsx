// EconomicsChart Component - SVG Chart for Economic Data

import React from 'react';
import { FINCEPT, CHART_WIDTH, CHART_HEIGHT } from '../constants';
import type { DataPoint } from '../types';

interface EconomicsChartProps {
  data: DataPoint[];
  sourceColor: string;
  formatValue: (val: number) => string;
}

// Smart date label formatter - creates unique, readable labels
const formatSmartLabel = (date: string, index: number, allDates: string[], totalLabels: number): string => {
  const monthNames = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];

  // Parse date components safely
  const year = date.substring(0, 4) || '';
  const month = date.length >= 7 ? date.substring(5, 7) : '';
  const day = date.length >= 10 ? date.substring(8, 10) : '';

  const monthIdx = month ? parseInt(month, 10) - 1 : -1;
  const monthName = monthIdx >= 0 && monthIdx < 12 ? monthNames[monthIdx] : '';
  const shortYear = year.substring(2, 4);

  // Determine what level of detail we need based on data span
  const firstDate = allDates[0] || '';
  const lastDate = allDates[allDates.length - 1] || '';
  const firstYear = firstDate.substring(0, 4);
  const lastYear = lastDate.substring(0, 4);
  const sameYear = firstYear === lastYear;

  // Count unique values at each level
  const uniqueYears = new Set(allDates.map(d => d.substring(0, 4))).size;
  const uniqueMonths = new Set(allDates.map(d => d.substring(0, 7))).size;

  // If only one year of data, don't repeat the year
  if (sameYear && day && monthName) {
    // Daily data within same year: "15 Jan"
    return `${parseInt(day)} ${monthName}`;
  }

  if (uniqueYears <= 2 && monthName && day) {
    // Daily data spanning ~2 years: "15 Jan'25"
    return `${parseInt(day)} ${monthName}'${shortYear}`;
  }

  if (uniqueMonths > uniqueYears * 2 && monthName) {
    // Monthly or more frequent data: "Jan'25"
    return `${monthName}'${shortYear}`;
  }

  // Annual or sparse data: just show year
  return year;
};

// Filter data to only show points where value changed (for constant data like Fed rates)
const filterSignificantPoints = (data: DataPoint[]): DataPoint[] => {
  if (data.length <= 2) return data;

  const result: DataPoint[] = [];

  // Always include first point
  result.push(data[0]);

  for (let i = 1; i < data.length - 1; i++) {
    const prev = data[i - 1].value;
    const curr = data[i].value;
    const next = data[i + 1].value;

    // Include point if value changed from previous OR will change to next
    // This keeps the "step" pattern for rate changes
    if (curr !== prev || curr !== next) {
      result.push(data[i]);
    }
  }

  // Always include last point
  result.push(data[data.length - 1]);

  return result;
};

// Check if data has many repeated values (like Fed rates)
const hasSignificantRepetition = (data: DataPoint[]): boolean => {
  if (data.length < 10) return false;

  let sameCount = 0;
  for (let i = 1; i < data.length; i++) {
    if (data[i].value === data[i - 1].value) {
      sameCount++;
    }
  }

  // If more than 50% of consecutive points are the same, filter them
  return sameCount / data.length > 0.5;
};

// Generate evenly distributed label indices with no duplicates
const generateLabelIndices = (
  data: DataPoint[],
  maxLabels: number
): number[] => {
  if (data.length === 0) return [];
  if (data.length <= maxLabels) {
    return data.map((_, i) => i);
  }

  const indices: number[] = [];
  const step = (data.length - 1) / (maxLabels - 1);

  for (let i = 0; i < maxLabels; i++) {
    const idx = Math.round(i * step);
    if (indices.length === 0 || indices[indices.length - 1] !== idx) {
      indices.push(idx);
    }
  }

  // Ensure last point is included
  if (indices[indices.length - 1] !== data.length - 1) {
    indices.push(data.length - 1);
  }

  return indices;
};

export function EconomicsChart({ data: rawData, sourceColor, formatValue }: EconomicsChartProps) {
  if (!rawData || rawData.length === 0) return null;

  // Filter out redundant points if data has many repeated values (like Fed rates)
  const data = hasSignificantRepetition(rawData) ? filterSignificantPoints(rawData) : rawData;

  const width = CHART_WIDTH;
  const height = CHART_HEIGHT;
  const padding = { top: 30, right: 40, bottom: 50, left: 80 };
  const chartWidth = width - padding.left - padding.right;
  const chartHeight = height - padding.top - padding.bottom;

  const values = data.map((d) => d.value);
  const minVal = Math.min(...values);
  const maxVal = Math.max(...values);
  const range = maxVal - minVal || 1;

  const points = data.map((d, i) => {
    const x = padding.left + (i / (data.length - 1 || 1)) * chartWidth;
    const y = padding.top + chartHeight - ((d.value - minVal) / range) * chartHeight;
    return { x, y, data: d };
  });

  const pathD = points.map((p, i) => `${i === 0 ? 'M' : 'L'} ${p.x} ${p.y}`).join(' ');
  const areaD = `${pathD} L ${points[points.length - 1].x} ${padding.top + chartHeight} L ${padding.left} ${padding.top + chartHeight} Z`;

  // Get all dates for label formatting
  const allDates = data.map(d => d.date);

  // Calculate how many labels can fit (estimate ~70px per label for horizontal)
  const maxLabels = Math.max(2, Math.min(15, Math.floor(chartWidth / 70)));

  // Generate evenly distributed label indices
  const labelIndices = generateLabelIndices(data, maxLabels);

  return (
    <svg width={width} height={height} style={{ display: 'block', margin: '0 auto', maxWidth: '100%' }}>
      <rect width={width} height={height} fill={FINCEPT.PANEL_BG} />

      {/* Horizontal Grid lines */}
      {[0, 1, 2, 3, 4, 5].map((i) => {
        const y = padding.top + (chartHeight * i) / 5;
        const val = maxVal - (range * i) / 5;
        return (
          <g key={i}>
            <line x1={padding.left} y1={y} x2={width - padding.right} y2={y} stroke={FINCEPT.BORDER} strokeDasharray="4,4" />
            <text x={padding.left - 12} y={y + 4} textAnchor="end" fill={FINCEPT.WHITE} fontSize="11" fontFamily="monospace">
              {formatValue(val)}
            </text>
          </g>
        );
      })}

      {/* Vertical grid lines */}
      {labelIndices.map((idx) => {
        const x = padding.left + (idx / (data.length - 1 || 1)) * chartWidth;
        return (
          <line key={`vgrid-${idx}`} x1={x} y1={padding.top} x2={x} y2={padding.top + chartHeight} stroke={FINCEPT.BORDER} strokeDasharray="2,4" strokeOpacity="0.5" />
        );
      })}

      {/* Area fill */}
      <path d={areaD} fill={`${sourceColor}25`} />

      {/* Line */}
      <path d={pathD} fill="none" stroke={sourceColor} strokeWidth="2.5" />


      {/* X-axis labels - horizontal only */}
      {labelIndices.map((idx) => {
        const d = data[idx];
        const x = padding.left + (idx / (data.length - 1 || 1)) * chartWidth;
        const label = formatSmartLabel(d.date, idx, allDates, labelIndices.length);
        return (
          <text key={`xlabel-${idx}`} x={x} y={height - 15} textAnchor="middle" fill={FINCEPT.WHITE} fontSize="10" fontFamily="monospace">
            {label}
          </text>
        );
      })}

      {/* Data points */}
      {points.map((p, i) => (
        <circle key={i} cx={p.x} cy={p.y} r={data.length > 30 ? 2 : 3} fill={sourceColor} />
      ))}

      {/* Axis lines */}
      <line x1={padding.left} y1={padding.top + chartHeight} x2={width - padding.right} y2={padding.top + chartHeight} stroke={FINCEPT.BORDER} strokeWidth="1" />
      <line x1={padding.left} y1={padding.top} x2={padding.left} y2={padding.top + chartHeight} stroke={FINCEPT.BORDER} strokeWidth="1" />
    </svg>
  );
}

export default EconomicsChart;
