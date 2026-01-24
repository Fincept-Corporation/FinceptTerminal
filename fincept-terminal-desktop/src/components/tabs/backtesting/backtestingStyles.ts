/**
 * Backtesting Shared Styles & Types
 *
 * FINCEPT UI Design System constants, reusable style factories,
 * and extended types for the institutional-grade backtesting system.
 */

import React from 'react';

// ============================================================================
// FINCEPT Design System Colors
// ============================================================================

export const F = {
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

// ============================================================================
// Reusable Style Factories
// ============================================================================

export const inputStyle: React.CSSProperties = {
  width: '100%',
  padding: '8px 10px',
  backgroundColor: F.DARK_BG,
  color: F.WHITE,
  border: `1px solid ${F.BORDER}`,
  borderRadius: '2px',
  fontSize: '10px',
  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
  outline: 'none',
};

export const labelStyle: React.CSSProperties = {
  fontSize: '9px',
  fontWeight: 700,
  color: F.GRAY,
  letterSpacing: '0.5px',
  textTransform: 'uppercase',
  marginBottom: '6px',
  display: 'block',
};

export const sectionHeaderStyle: React.CSSProperties = {
  padding: '10px 12px',
  backgroundColor: F.HEADER_BG,
  borderBottom: `1px solid ${F.BORDER}`,
  display: 'flex',
  alignItems: 'center',
  gap: '8px',
  cursor: 'pointer',
  userSelect: 'none',
};

export const panelStyle: React.CSSProperties = {
  backgroundColor: F.PANEL_BG,
  border: `1px solid ${F.BORDER}`,
  borderRadius: '2px',
};

export const selectStyle: React.CSSProperties = {
  ...inputStyle,
  appearance: 'none',
  cursor: 'pointer',
  paddingRight: '28px',
  backgroundImage: `url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='10' height='6'%3E%3Cpath d='M0 0l5 6 5-6z' fill='%23787878'/%3E%3C/svg%3E")`,
  backgroundRepeat: 'no-repeat',
  backgroundPosition: 'right 10px center',
};

export const buttonStyle: React.CSSProperties = {
  padding: '8px 14px',
  fontSize: '10px',
  fontWeight: 700,
  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
  textTransform: 'uppercase',
  letterSpacing: '0.5px',
  border: 'none',
  borderRadius: '2px',
  cursor: 'pointer',
  display: 'flex',
  alignItems: 'center',
  gap: '6px',
};

export const tabStyle = (active: boolean): React.CSSProperties => ({
  padding: '6px 12px',
  fontSize: '9px',
  fontWeight: 700,
  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
  textTransform: 'uppercase',
  letterSpacing: '0.5px',
  color: active ? F.ORANGE : F.GRAY,
  backgroundColor: active ? F.HEADER_BG : 'transparent',
  border: 'none',
  borderBottom: active ? `2px solid ${F.ORANGE}` : '2px solid transparent',
  cursor: 'pointer',
  transition: 'color 0.15s',
});

export const metricValueStyle = (value: number): React.CSSProperties => ({
  fontSize: '12px',
  fontWeight: 700,
  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
  color: value > 0 ? F.GREEN : value < 0 ? F.RED : F.WHITE,
});

export const cellStyle: React.CSSProperties = {
  padding: '6px 8px',
  fontSize: '10px',
  fontFamily: '"IBM Plex Mono", "Consolas", monospace',
  borderBottom: `1px solid ${F.BORDER}`,
};

export const headerCellStyle: React.CSSProperties = {
  ...cellStyle,
  color: F.GRAY,
  fontWeight: 700,
  textTransform: 'uppercase',
  fontSize: '9px',
  letterSpacing: '0.5px',
};

// ============================================================================
// Extended Types
// ============================================================================

export type StrategyType =
  | 'sma_crossover'
  | 'ema_crossover'
  | 'rsi'
  | 'macd'
  | 'bollinger_bands'
  | 'mean_reversion'
  | 'momentum'
  | 'breakout'
  | 'stochastic'
  | 'adx_trend'
  | 'keltner_breakout'
  | 'triple_ma'
  | 'dual_momentum'
  | 'volatility_breakout'
  | 'rsi_macd'
  | 'macd_adx'
  | 'bollinger_rsi'
  | 'williams_r'
  | 'cci'
  | 'obv_trend'
  | 'custom';

export type PositionSizing = 'fixed' | 'kelly' | 'fixed_fractional' | 'vol_target';

export interface BacktestConfigExtended {
  symbols: string[];
  weights: number[];
  startDate: string;
  endDate: string;
  initialCapital: number;
  strategyType: StrategyType;
  strategyCode?: string;
  parameters: Record<string, number>;
  commission: number;
  slippage: number;
  leverage: number;
  benchmark: string;
  positionSizing: PositionSizing;
  positionSizeValue: number;
  stopLoss: number;       // Stop-loss as decimal (0.05 = 5%), 0 = disabled
  takeProfit: number;     // Take-profit as decimal (0.10 = 10%), 0 = disabled
  trailingStop: number;   // Trailing stop as decimal (0.03 = 3%), 0 = disabled
  allowShort: boolean;    // Allow short selling
  compareRandom: boolean; // Compare vs random portfolio (stat significance)
}

export interface MonthlyReturnsData {
  year: number;
  months: (number | null)[];  // 12 months, null = no data
  yearTotal: number;
}

export interface RollingMetric {
  date: string;
  value: number;
}

export interface AdvancedMetrics {
  var95: number;
  var99: number;
  cvar95: number;
  cvar99: number;
  ulcerIndex: number;
  omegaRatio: number;
  tailRatio: number;
  kurtosis: number;
  skewness: number;
  avgDailyReturn: number;
  dailyReturnStd: number;
  dailyReturnP5: number;
  dailyReturnP25: number;
  dailyReturnP75: number;
  dailyReturnP95: number;
  returnsHistogram: { bin: number; count: number }[];
  monthlyReturns: MonthlyReturnsData[];
  rollingSharpe: RollingMetric[];
  rollingVolatility: RollingMetric[];
  rollingDrawdown: RollingMetric[];
  benchmarkAlpha: number;
  benchmarkBeta: number;
  informationRatio: number;
  trackingError: number;
  rSquared: number;
  benchmarkCorrelation: number;
}

export interface ExtendedBacktestResult {
  id: string;
  status: 'completed' | 'failed';
  performance: Record<string, number>;
  trades: ExtendedTrade[];
  equity: ExtendedEquityPoint[];
  statistics: Record<string, number | string>;
  advancedMetrics?: AdvancedMetrics;
  logs: string[];
  usingSyntheticData?: boolean;
}

export interface ExtendedTrade {
  id: string;
  symbol: string;
  entryDate: string;
  exitDate: string;
  side: 'long' | 'short';
  quantity: number;
  entryPrice: number;
  exitPrice: number;
  pnl: number;
  pnlPercent: number;
  holdingPeriod: number;
  commission: number;
  slippage: number;
  exitReason: string;
}

export interface ExtendedEquityPoint {
  date: string;
  equity: number;
  returns: number;
  drawdown: number;
  benchmark?: number;
}

// ============================================================================
// Strategy Definitions
// ============================================================================

export interface StrategyDefinition {
  type: StrategyType;
  label: string;
  description: string;
  parameters: StrategyParameterDef[];
}

export interface StrategyParameterDef {
  key: string;
  label: string;
  type: 'int' | 'float';
  default: number;
  min: number;
  max: number;
  step: number;
}

export const STRATEGY_DEFINITIONS: StrategyDefinition[] = [
  {
    type: 'sma_crossover',
    label: 'SMA Crossover',
    description: 'Simple moving average crossover',
    parameters: [
      { key: 'fastPeriod', label: 'Fast Period', type: 'int', default: 10, min: 2, max: 200, step: 1 },
      { key: 'slowPeriod', label: 'Slow Period', type: 'int', default: 20, min: 5, max: 500, step: 1 },
    ],
  },
  {
    type: 'ema_crossover',
    label: 'EMA Crossover',
    description: 'Exponential moving average crossover',
    parameters: [
      { key: 'fastPeriod', label: 'Fast Period', type: 'int', default: 12, min: 2, max: 200, step: 1 },
      { key: 'slowPeriod', label: 'Slow Period', type: 'int', default: 26, min: 5, max: 500, step: 1 },
    ],
  },
  {
    type: 'rsi',
    label: 'RSI',
    description: 'Relative Strength Index mean-reversion',
    parameters: [
      { key: 'period', label: 'Period', type: 'int', default: 14, min: 2, max: 100, step: 1 },
      { key: 'oversold', label: 'Oversold', type: 'int', default: 30, min: 5, max: 50, step: 1 },
      { key: 'overbought', label: 'Overbought', type: 'int', default: 70, min: 50, max: 95, step: 1 },
    ],
  },
  {
    type: 'macd',
    label: 'MACD',
    description: 'Moving Average Convergence/Divergence',
    parameters: [
      { key: 'fastPeriod', label: 'Fast Period', type: 'int', default: 12, min: 2, max: 100, step: 1 },
      { key: 'slowPeriod', label: 'Slow Period', type: 'int', default: 26, min: 5, max: 200, step: 1 },
      { key: 'signalPeriod', label: 'Signal Period', type: 'int', default: 9, min: 2, max: 50, step: 1 },
    ],
  },
  {
    type: 'bollinger_bands',
    label: 'Bollinger Bands',
    description: 'Mean-reversion on Bollinger Band extremes',
    parameters: [
      { key: 'period', label: 'Period', type: 'int', default: 20, min: 5, max: 100, step: 1 },
      { key: 'stdDev', label: 'Std Dev', type: 'float', default: 2.0, min: 0.5, max: 4.0, step: 0.1 },
    ],
  },
  {
    type: 'mean_reversion',
    label: 'Mean Reversion',
    description: 'Z-score based mean reversion strategy',
    parameters: [
      { key: 'period', label: 'Lookback', type: 'int', default: 20, min: 5, max: 200, step: 1 },
      { key: 'zThreshold', label: 'Z-Threshold', type: 'float', default: 2.0, min: 0.5, max: 4.0, step: 0.1 },
    ],
  },
  {
    type: 'momentum',
    label: 'Momentum',
    description: 'N-bar lookback return momentum',
    parameters: [
      { key: 'lookback', label: 'Lookback', type: 'int', default: 20, min: 5, max: 200, step: 1 },
      { key: 'threshold', label: 'Threshold %', type: 'float', default: 0.0, min: -5.0, max: 5.0, step: 0.1 },
    ],
  },
  {
    type: 'breakout',
    label: 'Breakout',
    description: 'Donchian channel breakout with ATR filter',
    parameters: [
      { key: 'period', label: 'Channel Period', type: 'int', default: 20, min: 5, max: 100, step: 1 },
      { key: 'atrMult', label: 'ATR Multiplier', type: 'float', default: 1.5, min: 0.5, max: 5.0, step: 0.1 },
    ],
  },
  {
    type: 'stochastic',
    label: 'Stochastic',
    description: '%K/%D crossover oscillator strategy',
    parameters: [
      { key: 'kPeriod', label: '%K Period', type: 'int', default: 14, min: 5, max: 50, step: 1 },
      { key: 'dPeriod', label: '%D Period', type: 'int', default: 3, min: 2, max: 20, step: 1 },
      { key: 'oversold', label: 'Oversold', type: 'int', default: 20, min: 5, max: 40, step: 1 },
      { key: 'overbought', label: 'Overbought', type: 'int', default: 80, min: 60, max: 95, step: 1 },
    ],
  },
  {
    type: 'adx_trend',
    label: 'ADX Trend',
    description: 'Trend-following when ADX signals strong trend',
    parameters: [
      { key: 'period', label: 'ADX Period', type: 'int', default: 14, min: 5, max: 50, step: 1 },
      { key: 'threshold', label: 'ADX Threshold', type: 'int', default: 25, min: 15, max: 50, step: 1 },
    ],
  },
  {
    type: 'keltner_breakout',
    label: 'Keltner Breakout',
    description: 'Breakout above/below Keltner Channel bands',
    parameters: [
      { key: 'emaPeriod', label: 'EMA Period', type: 'int', default: 20, min: 5, max: 100, step: 1 },
      { key: 'atrPeriod', label: 'ATR Period', type: 'int', default: 10, min: 5, max: 50, step: 1 },
      { key: 'atrMult', label: 'ATR Multiplier', type: 'float', default: 2.0, min: 0.5, max: 5.0, step: 0.1 },
    ],
  },
  {
    type: 'triple_ma',
    label: 'Triple MA',
    description: 'Three moving average alignment (fast > mid > slow = long)',
    parameters: [
      { key: 'fastPeriod', label: 'Fast Period', type: 'int', default: 5, min: 2, max: 50, step: 1 },
      { key: 'midPeriod', label: 'Mid Period', type: 'int', default: 20, min: 5, max: 100, step: 1 },
      { key: 'slowPeriod', label: 'Slow Period', type: 'int', default: 50, min: 10, max: 300, step: 1 },
    ],
  },
  {
    type: 'dual_momentum',
    label: 'Dual Momentum',
    description: 'Absolute + relative momentum filter',
    parameters: [
      { key: 'lookback', label: 'Lookback', type: 'int', default: 60, min: 10, max: 252, step: 1 },
      { key: 'absThreshold', label: 'Abs Threshold %', type: 'float', default: 0.0, min: -5.0, max: 10.0, step: 0.5 },
    ],
  },
  {
    type: 'volatility_breakout',
    label: 'Volatility Breakout',
    description: 'Enter on expansion of volatility (ATR spike)',
    parameters: [
      { key: 'atrPeriod', label: 'ATR Period', type: 'int', default: 14, min: 5, max: 50, step: 1 },
      { key: 'atrMult', label: 'ATR Multiplier', type: 'float', default: 2.0, min: 1.0, max: 5.0, step: 0.1 },
      { key: 'lookback', label: 'Lookback', type: 'int', default: 20, min: 5, max: 100, step: 1 },
    ],
  },
  {
    type: 'rsi_macd',
    label: 'RSI + MACD',
    description: 'Combined RSI oversold/overbought with MACD confirmation',
    parameters: [
      { key: 'rsiPeriod', label: 'RSI Period', type: 'int', default: 14, min: 5, max: 50, step: 1 },
      { key: 'oversold', label: 'Oversold', type: 'int', default: 30, min: 10, max: 45, step: 1 },
      { key: 'overbought', label: 'Overbought', type: 'int', default: 70, min: 55, max: 90, step: 1 },
    ],
  },
  {
    type: 'macd_adx',
    label: 'MACD + ADX',
    description: 'MACD signal with ADX trend strength filter',
    parameters: [
      { key: 'fastPeriod', label: 'Fast', type: 'int', default: 12, min: 5, max: 50, step: 1 },
      { key: 'slowPeriod', label: 'Slow', type: 'int', default: 26, min: 10, max: 100, step: 1 },
      { key: 'adxThreshold', label: 'ADX Threshold', type: 'int', default: 25, min: 15, max: 50, step: 1 },
    ],
  },
  {
    type: 'bollinger_rsi',
    label: 'Bollinger + RSI',
    description: 'Bollinger Band touch with RSI confirmation',
    parameters: [
      { key: 'bbPeriod', label: 'BB Period', type: 'int', default: 20, min: 5, max: 100, step: 1 },
      { key: 'stdDev', label: 'Std Dev', type: 'float', default: 2.0, min: 0.5, max: 4.0, step: 0.1 },
      { key: 'rsiPeriod', label: 'RSI Period', type: 'int', default: 14, min: 5, max: 50, step: 1 },
    ],
  },
  {
    type: 'williams_r',
    label: "Williams %R",
    description: 'Williams %R overbought/oversold oscillator',
    parameters: [
      { key: 'period', label: 'Period', type: 'int', default: 14, min: 5, max: 50, step: 1 },
      { key: 'oversold', label: 'Oversold', type: 'int', default: -80, min: -95, max: -60, step: 1 },
      { key: 'overbought', label: 'Overbought', type: 'int', default: -20, min: -40, max: -5, step: 1 },
    ],
  },
  {
    type: 'cci',
    label: 'CCI',
    description: 'Commodity Channel Index trend/reversal',
    parameters: [
      { key: 'period', label: 'Period', type: 'int', default: 20, min: 5, max: 100, step: 1 },
      { key: 'threshold', label: 'Threshold', type: 'int', default: 100, min: 50, max: 300, step: 10 },
    ],
  },
  {
    type: 'obv_trend',
    label: 'OBV Trend',
    description: 'On-Balance Volume trend confirmation',
    parameters: [
      { key: 'maPeriod', label: 'OBV MA Period', type: 'int', default: 20, min: 5, max: 100, step: 1 },
      { key: 'priceMaPeriod', label: 'Price MA Period', type: 'int', default: 50, min: 10, max: 200, step: 1 },
    ],
  },
];

// ============================================================================
// Utility Functions
// ============================================================================

export function formatPercent(value: number, decimals = 2): string {
  return `${(value * 100).toFixed(decimals)}%`;
}

export function formatNumber(value: number, decimals = 2): string {
  return value.toLocaleString(undefined, {
    minimumFractionDigits: decimals,
    maximumFractionDigits: decimals,
  });
}

export function formatCurrency(value: number): string {
  return value.toLocaleString(undefined, {
    style: 'currency',
    currency: 'USD',
    minimumFractionDigits: 0,
    maximumFractionDigits: 0,
  });
}

export function getDefaultParameters(strategyType: StrategyType): Record<string, number> {
  const def = STRATEGY_DEFINITIONS.find(s => s.type === strategyType);
  if (!def) return {};
  const params: Record<string, number> = {};
  for (const p of def.parameters) {
    params[p.key] = p.default;
  }
  return params;
}
