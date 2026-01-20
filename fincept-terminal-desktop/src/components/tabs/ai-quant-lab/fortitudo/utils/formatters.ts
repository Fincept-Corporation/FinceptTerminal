/**
 * Fortitudo Panel Formatting Utilities
 */

import { FINCEPT } from '../constants';

/**
 * Format a number as a percentage string
 */
export function formatPercent(value: number | null | undefined): string {
  if (value === null || value === undefined || isNaN(value)) return 'N/A';
  return `${(value * 100).toFixed(2)}%`;
}

/**
 * Format a number as a ratio with 3 decimal places
 */
export function formatRatio(value: number | null | undefined): string {
  if (value === null || value === undefined || isNaN(value)) return 'N/A';
  return value.toFixed(3);
}

/**
 * Format a Greek value with 4 decimal places
 */
export function formatGreek(val: number | undefined): string {
  if (val === undefined || val === null || isNaN(val)) return 'N/A';
  return val.toFixed(4);
}

/**
 * Get color for Greek value based on sign
 */
export function getGreekValueColor(val: number | undefined): string {
  if (val === undefined || val === null || isNaN(val)) return FINCEPT.GRAY;
  if (val > 0) return FINCEPT.GREEN;
  if (val < 0) return FINCEPT.RED;
  return FINCEPT.WHITE;
}

/**
 * Normalize weights from object or array to array with asset names
 */
export function normalizeWeights(
  weights: number[] | Record<string, number> | undefined,
  assets: string[] | undefined
): { asset: string; weight: number }[] {
  if (!weights) return [];

  if (Array.isArray(weights)) {
    return weights.map((w, idx) => ({
      asset: assets?.[idx] || `Asset ${idx + 1}`,
      weight: w
    }));
  } else {
    // weights is an object {assetName: weight}
    return Object.entries(weights).map(([asset, weight]) => ({
      asset,
      weight: weight as number
    }));
  }
}

/**
 * Format currency value
 */
export function formatCurrency(value: number | null | undefined, decimals: number = 2): string {
  if (value === null || value === undefined || isNaN(value)) return 'N/A';
  return `$${value.toFixed(decimals)}`;
}
