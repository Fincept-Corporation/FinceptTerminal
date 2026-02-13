// Common chart/data visualization types — shared across all tabs

export interface DataPoint {
  date: string;
  value: number;
}

export interface ChartStats {
  latest: number;
  change: number;
  min: number;
  max: number;
  avg: number;
  count: number;
}

export interface SaveNotification {
  show: boolean;
  path: string;
  type: 'image' | 'csv';
}

export type DateRangePreset = '1M' | '3M' | '6M' | '1Y' | '2Y' | '5Y' | '10Y' | 'ALL';

// Fincept shared color tokens (single source of truth)
export const FINCEPT_COLORS = {
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
} as const;

export const CHART_WIDTH = 1200;
export const CHART_HEIGHT = 400;
