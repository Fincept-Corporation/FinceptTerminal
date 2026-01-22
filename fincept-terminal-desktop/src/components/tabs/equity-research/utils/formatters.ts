// Formatting utilities for Equity Research tab

/**
 * Format a number with specified decimal places
 */
export const formatNumber = (num: number | null | undefined, decimals = 2): string => {
  if (num === null || num === undefined) return 'N/A';
  return num.toLocaleString(undefined, { minimumFractionDigits: decimals, maximumFractionDigits: decimals });
};

/**
 * Format a large number with currency prefix and abbreviations (T, B, M, K)
 * Handles negative numbers properly
 */
export const formatLargeNumber = (num: number | null | undefined): string => {
  if (num === null || num === undefined || isNaN(num)) return 'N/A';

  const absNum = Math.abs(num);
  const sign = num < 0 ? '-' : '';

  if (absNum >= 1e12) return `${sign}$${(absNum / 1e12).toFixed(2)}T`;
  if (absNum >= 1e9) return `${sign}$${(absNum / 1e9).toFixed(2)}B`;
  if (absNum >= 1e6) return `${sign}$${(absNum / 1e6).toFixed(2)}M`;
  if (absNum >= 1e3) return `${sign}$${(absNum / 1e3).toFixed(2)}K`;
  if (absNum === 0) return '$0';
  return `${sign}$${absNum.toFixed(2)}`;
};

/**
 * Format a decimal as a percentage
 */
export const formatPercent = (num: number | null | undefined): string => {
  if (num === null || num === undefined) return 'N/A';
  return `${(num * 100).toFixed(2)}%`;
};

/**
 * Format indicator value based on magnitude
 */
export const formatIndicatorValue = (val: number): string => {
  if (Math.abs(val) < 0.01) return val.toFixed(6);
  if (Math.abs(val) < 1) return val.toFixed(4);
  if (Math.abs(val) < 100) return val.toFixed(2);
  return val.toFixed(0);
};

/**
 * Get recommendation color based on recommendation key
 */
export const getRecommendationColor = (key: string | null, colors: { GREEN: string; RED: string; YELLOW: string; GRAY: string }): string => {
  if (!key) return colors.GRAY;
  if (key === 'buy' || key === 'strong_buy') return colors.GREEN;
  if (key === 'sell' || key === 'strong_sell') return colors.RED;
  return colors.YELLOW;
};

/**
 * Get recommendation text from key
 */
export const getRecommendationText = (key: string | null): string => {
  if (!key) return 'N/A';
  return key.toUpperCase().replace('_', ' ');
};

/**
 * Get year from period string (e.g., "2023-12-31" -> 2023)
 */
export const getYearFromPeriod = (period: string): number => {
  return new Date(period).getFullYear();
};
