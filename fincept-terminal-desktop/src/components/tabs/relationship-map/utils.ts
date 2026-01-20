// Utility functions for RelationshipMapTab

/**
 * Format monetary values with appropriate suffixes
 */
export const formatMoney = (val: number): string => {
  if (!val || val === 0) return '$0';
  if (val >= 1e12) return `$${(val / 1e12).toFixed(2)}T`;
  if (val >= 1e9) return `$${(val / 1e9).toFixed(2)}B`;
  if (val >= 1e6) return `$${(val / 1e6).toFixed(2)}M`;
  if (val >= 1e3) return `$${(val / 1e3).toFixed(2)}K`;
  return `$${val.toFixed(2)}`;
};

/**
 * Format market cap with appropriate suffixes (same as formatMoney but for display)
 */
export const formatMarketCap = (val: number): string => {
  if (!val) return 'N/A';
  if (val >= 1e12) return `$${(val / 1e12).toFixed(2)}T`;
  if (val >= 1e9) return `$${(val / 1e9).toFixed(2)}B`;
  if (val >= 1e6) return `$${(val / 1e6).toFixed(2)}M`;
  return `$${val}`;
};

/**
 * Format percentage values
 */
export const formatPercent = (val: number): string => {
  if (!val && val !== 0) return 'N/A';
  return `${(val * 100).toFixed(2)}%`;
};

/**
 * Format large numbers with appropriate suffixes
 */
export const formatNumber = (val: number): string => {
  if (!val && val !== 0) return 'N/A';
  if (val >= 1e9) return `${(val / 1e9).toFixed(2)}B`;
  if (val >= 1e6) return `${(val / 1e6).toFixed(2)}M`;
  return val.toFixed(2);
};

/**
 * Format date for display
 */
export const formatDate = (dateStr: string): string => {
  return new Date(dateStr).toLocaleDateString('en-US', {
    month: 'short',
    day: 'numeric',
    year: '2-digit',
  });
};
