/**
 * FFN Analytics Constants
 * Color palette and sample data
 */

// Fincept Professional Color Palette
export const FINCEPT = {
  ORANGE: '#FF8800',
  WHITE: '#FFFFFF',
  RED: '#FF3B3B',
  GREEN: '#00D66F',
  GRAY: '#787878',
  DARK_BG: '#000000',
  PANEL_BG: '#0F0F0F',
  HEADER_BG: '#1A1A1A',
  CYAN: '#00E5FF',
  YELLOW: '#FFD700',
  BLUE: '#0088FF',
  PURPLE: '#9D4EDD',
  BORDER: '#2A2A2A',
  HOVER: '#1F1F1F',
  MUTED: '#4A4A4A'
};

// Sample price data for demonstration
export const SAMPLE_PRICES: Record<string, number> = {
  '2023-01-03': 100.0,
  '2023-01-04': 101.2,
  '2023-01-05': 99.8,
  '2023-01-06': 102.5,
  '2023-01-09': 103.1,
  '2023-01-10': 104.0,
  '2023-01-11': 102.3,
  '2023-01-12': 105.2,
  '2023-01-13': 106.8,
  '2023-01-17': 105.5,
  '2023-01-18': 107.2,
  '2023-01-19': 106.0,
  '2023-01-20': 108.5,
  '2023-01-23': 109.3,
  '2023-01-24': 110.0,
  '2023-01-25': 108.7,
  '2023-01-26': 111.2,
  '2023-01-27': 112.5,
  '2023-01-30': 111.0,
  '2023-01-31': 113.8
};

// Default configuration values
export const DEFAULT_CONFIG = {
  risk_free_rate: 0.02,
  annualization_factor: 252,
  drawdown_threshold: 0.05
};

// Default historical days
export const DEFAULT_HISTORICAL_DAYS = 365;

// Default benchmark symbol
export const DEFAULT_BENCHMARK_SYMBOL = 'SPY';

// Common benchmark symbols for suggestions
export const COMMON_BENCHMARKS = ['SPY', 'QQQ', 'IWM', 'DIA'];

// Month names for heatmap
export const MONTH_NAMES = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
