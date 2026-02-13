// Shared chart/data utility functions — used across all tabs

import type { DataPoint, ChartStats, DateRangePreset } from './types';

/**
 * Compute start/end dates from a preset like '1M', '5Y', 'ALL', etc.
 */
export const getDateFromPreset = (preset: DateRangePreset): { start: string; end: string } => {
  const now = new Date();
  const end = now.toISOString().split('T')[0];
  let start: Date;

  switch (preset) {
    case '1M':
      start = new Date(now.getFullYear(), now.getMonth() - 1, now.getDate());
      break;
    case '3M':
      start = new Date(now.getFullYear(), now.getMonth() - 3, now.getDate());
      break;
    case '6M':
      start = new Date(now.getFullYear(), now.getMonth() - 6, now.getDate());
      break;
    case '1Y':
      start = new Date(now.getFullYear() - 1, now.getMonth(), now.getDate());
      break;
    case '2Y':
      start = new Date(now.getFullYear() - 2, now.getMonth(), now.getDate());
      break;
    case '5Y':
      start = new Date(now.getFullYear() - 5, now.getMonth(), now.getDate());
      break;
    case '10Y':
      start = new Date(now.getFullYear() - 10, now.getMonth(), now.getDate());
      break;
    case 'ALL':
    default:
      start = new Date(1960, 0, 1);
      break;
  }

  return { start: start.toISOString().split('T')[0], end };
};

/**
 * Client-side date range filter for DataPoint arrays.
 * Handles year-only ("2020"), month-only ("2020-01"), and full dates ("2020-01-15").
 */
export const filterByDateRange = (
  data: DataPoint[],
  startDate: string,
  endDate: string,
): DataPoint[] => {
  if (!startDate && !endDate) return data;
  return data.filter((d) => {
    const dateStr = d.date;
    const normalized =
      dateStr.length === 4
        ? `${dateStr}-01-01`
        : dateStr.length === 7
          ? `${dateStr}-01`
          : dateStr;
    return (!startDate || normalized >= startDate) && (!endDate || normalized <= endDate);
  });
};

/**
 * Compute summary statistics (latest, change, min, max, avg, count) for a DataPoint[].
 */
export const computeStats = (data: DataPoint[]): ChartStats | null => {
  if (!data || data.length === 0) return null;
  const values = data.map((d) => d.value);
  const latest = values[values.length - 1];
  const previous = values.length > 1 ? values[values.length - 2] : latest;
  const change = previous !== 0 ? ((latest - previous) / Math.abs(previous)) * 100 : 0;
  return {
    latest,
    change,
    min: Math.min(...values),
    max: Math.max(...values),
    avg: values.reduce((a, b) => a + b, 0) / values.length,
    count: values.length,
  };
};
