/**
 * Surface Analytics - Constants
 */

// Default symbols for volatility surface analysis
export const VOL_SURFACE_SYMBOLS = [
  { value: 'SPY', label: 'SPY - S&P 500 ETF' },
  { value: 'QQQ', label: 'QQQ - NASDAQ 100 ETF' },
  { value: 'IWM', label: 'IWM - Russell 2000 ETF' },
  { value: 'VIX', label: 'VIX - Volatility Index' },
  { value: 'AAPL', label: 'AAPL - Apple Inc.' },
  { value: 'TSLA', label: 'TSLA - Tesla Inc.' },
  { value: 'NVDA', label: 'NVDA - NVIDIA Corp.' },
  { value: 'AMZN', label: 'AMZN - Amazon.com' },
] as const;

// Default assets for correlation analysis
export const CORRELATION_ASSETS = [
  'SPY',   // S&P 500
  'QQQ',   // NASDAQ 100
  'IWM',   // Russell 2000
  'DIA',   // Dow Jones
  'GLD',   // Gold
  'SLV',   // Silver
  'TLT',   // 20+ Year Treasury
  'IEF',   // 7-10 Year Treasury
  'HYG',   // High Yield Bonds
  'LQD',   // Investment Grade Bonds
  'VNQ',   // Real Estate
  'EEM',   // Emerging Markets
  'EFA',   // Developed Markets
  'USO',   // Oil
  'UNG',   // Natural Gas
] as const;

// Treasury maturities for yield curve
export const TREASURY_MATURITIES = [
  { months: 1, label: '1M' },
  { months: 2, label: '2M' },
  { months: 3, label: '3M' },
  { months: 6, label: '6M' },
  { months: 12, label: '1Y' },
  { months: 24, label: '2Y' },
  { months: 36, label: '3Y' },
  { months: 60, label: '5Y' },
  { months: 84, label: '7Y' },
  { months: 120, label: '10Y' },
  { months: 240, label: '20Y' },
  { months: 360, label: '30Y' },
] as const;

// Databento datasets
export const DATABENTO_DATASETS = {
  OPRA_OPTIONS: 'OPRA.PILLAR',
  CME_FUTURES: 'GLBX.MDP3',
  XNAS_EQUITIES: 'XNAS.ITCH',
} as const;

// Databento schemas
export const DATABENTO_SCHEMAS = {
  OHLCV_1D: 'ohlcv-1d',
  OHLCV_1H: 'ohlcv-1h',
  OHLCV_1M: 'ohlcv-1m',
  MBP_1: 'mbp-1',
  CMBP_1: 'cmbp-1', // Consolidated MBP for OPRA (2025+)
  DEFINITION: 'definition',
  TRADES: 'trades',
} as const;

// Risk-free rate (approximate, should be fetched from Fed)
export const RISK_FREE_RATE = 0.043; // 4.3%

// Color scales for charts
export const COLOR_SCALES = {
  volatility: [
    [0, '#1a237e'], [0.2, '#1565c0'], [0.4, '#0288d1'],
    [0.6, '#26c6da'], [0.75, '#ffa726'], [0.9, '#ff7043'], [1, '#d32f2f']
  ],
  correlation: [
    [0, '#b71c1c'], [0.3, '#e53935'], [0.45, '#ff9800'],
    [0.5, '#ffeb3b'], [0.65, '#7cb342'], [0.8, '#43a047'], [1, '#2e7d32']
  ],
  yieldCurve: [
    [0, '#1a237e'], [0.25, '#283593'], [0.4, '#3949ab'],
    [0.55, '#5e35b1'], [0.7, '#7e57c2'], [0.85, '#ba68c8'], [1, '#f06292']
  ],
  pca: [
    [0, '#0d47a1'], [0.2, '#1976d2'], [0.4, '#42a5f5'],
    [0.5, '#e0e0e0'], [0.6, '#ef5350'], [0.8, '#e53935'], [1, '#b71c1c']
  ],
} as const;

// Chart configuration
export const PLOTLY_CONFIG = {
  displayModeBar: true,
  displaylogo: false,
  modeBarButtonsToRemove: ['select2d', 'lasso2d'],
  responsive: true,
} as const;

// Note: Colors and typography are now sourced from ThemeContext (useTerminalTheme hook)
// Old FINCEPT_COLORS and TYPOGRAPHY constants have been removed in favor of the theme system
