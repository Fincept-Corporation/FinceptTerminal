/**
 * Functime Panel Formatting Utilities
 */

/**
 * Format a metric value with specified decimal places
 */
export function formatMetric(value: number | null | undefined, decimals = 4): string {
  if (value === null || value === undefined || isNaN(value)) return 'N/A';
  return value.toFixed(decimals);
}

/**
 * Format a percentage value
 */
export function formatPercent(value: number | null | undefined, decimals = 2): string {
  if (value === null || value === undefined || isNaN(value)) return 'N/A';
  return `${(value * 100).toFixed(decimals)}%`;
}

/**
 * Format a date string to display format
 */
export function formatDate(dateString: string): string {
  return dateString.split('T')[0];
}

/**
 * Parse historical data from JSON input
 */
export function parseHistoricalData(dataInput: string): { time: string; value: number }[] {
  try {
    const data = JSON.parse(dataInput);
    if (Array.isArray(data)) {
      return data.map(d => ({
        time: d.time,
        value: d.value
      }));
    }
    return [];
  } catch {
    return [];
  }
}
