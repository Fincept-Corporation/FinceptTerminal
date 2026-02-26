// Watchlist Utilities - Shared constants, formatting, and helper functions

// Fincept Design System Color Palette
export const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
};

// Legacy export for backwards compat
export const FINCEPT_COLORS = FINCEPT;
export const getFinceptColors = () => FINCEPT;

// Font family constant
export const FONT_FAMILY = '"IBM Plex Mono", "Consolas", monospace';

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
  if (value === undefined || value === null) return FINCEPT.GRAY;
  return value >= 0 ? FINCEPT.GREEN : FINCEPT.RED;
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
      case 'CHANGE': {
        const changeA = a.quote?.change_percent || 0;
        const changeB = b.quote?.change_percent || 0;
        compareValue = changeB - changeA;
        break;
      }
      case 'VOLUME': {
        const volA = a.quote?.volume || 0;
        const volB = b.quote?.volume || 0;
        compareValue = volB - volA;
        break;
      }
      case 'PRICE': {
        const priceA = a.quote?.price || 0;
        const priceB = b.quote?.price || 0;
        compareValue = priceB - priceA;
        break;
      }
    }

    return ascending ? -compareValue : compareValue;
  });

  return sorted;
};

// Default watchlist colors
export const WATCHLIST_COLORS = [
  '#FF8800', // Orange
  '#0088FF', // Blue
  '#00D66F', // Green
  '#00E5FF', // Cyan
  '#9D4EDD', // Purple
  '#FFD700', // Yellow
  '#FF3B3B', // Red
  '#FFFFFF', // White
];

// Get next available color for new watchlist
export const getNextWatchlistColor = (existingColors: string[]): string => {
  for (const color of WATCHLIST_COLORS) {
    if (!existingColors.includes(color)) {
      return color;
    }
  }
  return WATCHLIST_COLORS[0];
};
