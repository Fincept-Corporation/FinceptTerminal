// Watchlist Utilities - Shared constants, formatting, and helper functions

// Bloomberg Color Scheme
export const BLOOMBERG_COLORS = {
  ORANGE: '#FFA500',
  WHITE: '#FFFFFF',
  RED: '#FF0000',
  GREEN: '#00C800',
  YELLOW: '#FFFF00',
  GRAY: '#787878',
  BLUE: '#6496FA',
  PURPLE: '#C864FF',
  CYAN: '#00FFFF',
  DARK_BG: '#000000',
  PANEL_BG: '#0a0a0a'
};

// Format currency values
export const formatCurrency = (value: number | undefined | null): string => {
  if (value === undefined || value === null) return 'N/A';
  return `$${value.toFixed(2)}`;
};

// Format percentage values
export const formatPercent = (value: number | undefined | null): string => {
  if (value === undefined || value === null) return 'N/A';
  const sign = value >= 0 ? '+' : '';
  return `${sign}${value.toFixed(2)}%`;
};

// Format large numbers (market cap, volume)
export const formatLargeNumber = (value: number | undefined | null): string => {
  if (value === undefined || value === null) return 'N/A';

  if (value >= 1_000_000_000_000) {
    return `${(value / 1_000_000_000_000).toFixed(2)}T`;
  } else if (value >= 1_000_000_000) {
    return `${(value / 1_000_000_000).toFixed(2)}B`;
  } else if (value >= 1_000_000) {
    return `${(value / 1_000_000).toFixed(1)}M`;
  } else if (value >= 1_000) {
    return `${(value / 1_000).toFixed(1)}K`;
  }

  return value.toFixed(0);
};

// Format volume
export const formatVolume = (value: number | undefined | null): string => {
  return formatLargeNumber(value);
};

// Get color based on value (positive/negative)
export const getChangeColor = (value: number | undefined | null): string => {
  if (value === undefined || value === null) return BLOOMBERG_COLORS.GRAY;
  return value >= 0 ? BLOOMBERG_COLORS.GREEN : BLOOMBERG_COLORS.RED;
};

// Sort stocks by different criteria
export type SortCriteria = 'TICKER' | 'CHANGE' | 'VOLUME' | 'PRICE';

export const sortStocks = <T extends { symbol: string; quote: any }>(
  stocks: T[],
  criteria: SortCriteria,
  ascending: boolean = false
): T[] => {
  const sorted = [...stocks];

  sorted.sort((a, b) => {
    let compareValue = 0;

    switch (criteria) {
      case 'TICKER':
        compareValue = a.symbol.localeCompare(b.symbol);
        break;
      case 'CHANGE':
        const changeA = a.quote?.change_percent || 0;
        const changeB = b.quote?.change_percent || 0;
        compareValue = changeB - changeA;
        break;
      case 'VOLUME':
        const volA = a.quote?.volume || 0;
        const volB = b.quote?.volume || 0;
        compareValue = volB - volA;
        break;
      case 'PRICE':
        const priceA = a.quote?.price || 0;
        const priceB = b.quote?.price || 0;
        compareValue = priceB - priceA;
        break;
    }

    return ascending ? -compareValue : compareValue;
  });

  return sorted;
};

// Default watchlist colors
export const WATCHLIST_COLORS = [
  '#FFA500', // Orange
  '#6496FA', // Blue
  '#00C800', // Green
  '#00FFFF', // Cyan
  '#C864FF', // Purple
  '#FFFF00', // Yellow
  '#FF0000', // Red
  '#FFFFFF', // White
];

// Get next available color for new watchlist
export const getNextWatchlistColor = (existingColors: string[]): string => {
  for (const color of WATCHLIST_COLORS) {
    if (!existingColors.includes(color)) {
      return color;
    }
  }
  return WATCHLIST_COLORS[0]; // Fallback to orange
};
