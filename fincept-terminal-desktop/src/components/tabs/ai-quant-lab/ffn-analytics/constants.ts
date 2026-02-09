/**
 * FFN Analytics Constants
 * Color palette and sample data
 * GREEN THEME
 */

// Fincept Terminal Color Palette - GREEN THEME for FFN Analytics
// Components should prefer useTerminalTheme() hook for runtime values
export const FINCEPT = {
  GREEN: 'var(--ft-color-success, #00D66F)',    // Primary theme color for FFN Analytics
  WHITE: 'var(--ft-color-text, #FFFFFF)',
  RED: 'var(--ft-color-alert, #FF3B3B)',
  ORANGE: 'var(--ft-color-primary, #FF8800)',
  DARK_BG: 'var(--ft-color-background, #0F0F0F)',
  PANEL_BG: 'var(--ft-color-panel, #0F0F0F)',
  CYAN: 'var(--ft-color-accent, #00E5FF)',
  BLUE: 'var(--ft-color-info, #0088FF)',
  BORDER: 'var(--ft-border-color, #2A2A2A)',
  HOVER: '#1F1F1F',
  GRAY: 'var(--ft-color-text-muted, #888888)',
  MUTED: 'var(--ft-color-text-muted, #666666)',
  PURPLE: 'var(--ft-color-purple, #BB86FC)',
  YELLOW: 'var(--ft-color-warning, #FFD700)',
  HEADER_BG: 'var(--ft-color-panel, #1A1A1A)'
};

// Sample price data for demonstration (kept for backward compatibility)
// Note: Default behavior now uses live yfinance data
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
