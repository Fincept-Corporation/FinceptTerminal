/**
 * FFN Analytics Formatters
 * Utility functions for formatting values
 */

import { FINCEPT } from '../constants';

/**
 * Format a number as percentage
 */
export function formatPercent(value: number | null | undefined, decimals = 2): string {
  if (value === null || value === undefined || isNaN(value)) return 'N/A';
  return `${(value * 100).toFixed(decimals)}%`;
}

/**
 * Format a number as ratio
 */
export function formatRatio(value: number | null | undefined, decimals = 2): string {
  if (value === null || value === undefined || isNaN(value)) return 'N/A';
  return value.toFixed(decimals);
}

/**
 * Get color based on value (positive = green, negative = red)
 */
export function getValueColor(value: number | null | undefined, inverse = false): string {
  if (value === null || value === undefined) return FINCEPT.GRAY;
  if (inverse) return value < 0 ? FINCEPT.GREEN : value > 0 ? FINCEPT.RED : FINCEPT.WHITE;
  return value > 0 ? FINCEPT.GREEN : value < 0 ? FINCEPT.RED : FINCEPT.WHITE;
}

/**
 * Get heatmap color based on return value
 */
export function getHeatmapColor(value: number | null): string {
  if (value === null || value === undefined) return FINCEPT.PANEL_BG;
  if (value > 0.05) return '#00D66F';
  if (value > 0.02) return '#00A050';
  if (value > 0) return '#006030';
  if (value > -0.02) return '#603000';
  if (value > -0.05) return '#A05000';
  return '#FF3B3B';
}

/**
 * Parse date string to formatted date
 */
export function formatDate(dateStr: string): string {
  const date = new Date(dateStr);
  return date.toLocaleDateString('en-US', {
    year: 'numeric',
    month: 'short',
    day: 'numeric'
  });
}
