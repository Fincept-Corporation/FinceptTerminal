/**
 * CFA Quant Formatting Utilities
 */

export const formatNumber = (val: number, decimals = 4): string => {
  if (val === null || val === undefined || isNaN(val)) return '—';
  return val.toFixed(decimals);
};

export const formatPrice = (val: number): string => {
  if (val === null || val === undefined || isNaN(val)) return '—';
  return val.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 });
};

export const formatPercentage = (val: number, decimals = 2): string => {
  if (val === null || val === undefined || isNaN(val)) return '—';
  return `${val >= 0 ? '+' : ''}${val.toFixed(decimals)}%`;
};

export const formatDate = (timestamp: number | string): string => {
  const date = new Date(timestamp);
  return date.toLocaleDateString('en-US', { month: 'short', day: 'numeric', year: '2-digit' });
};
