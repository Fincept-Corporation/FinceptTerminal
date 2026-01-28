/**
 * Backtesting Shared Types & Constants
 */

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
// Types
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
  stopLoss: number;
  takeProfit: number;
  trailingStop: number;
  allowShort: boolean;
  compareRandom: boolean;
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

export function getDefaultParameters(strategyType: StrategyType): Record<string, number> {
  const def = STRATEGY_DEFINITIONS.find(s => s.type === strategyType);
  if (!def) return {};
  const params: Record<string, number> = {};
  for (const p of def.parameters) {
    params[p.key] = p.default;
  }
  return params;
}

// ============================================================================
// Sub-Tab Types
// ============================================================================

export type BacktestingSubTab =
  | 'backtest' | 'optimize' | 'data' | 'indicators'
  | 'signals' | 'labels' | 'splitters' | 'catalog' | 'returns';

export type OptimizationObjective = 'sharpe' | 'sortino' | 'return' | 'calmar' | 'profit_factor' | 'sqn';

export type DataSourceType = 'yfinance' | 'binance' | 'ccxt' | 'alpaca' | 'gbm';

export type SignalGeneratorType =
  | 'RAND' | 'RANDX' | 'RANDNX' | 'RPROB' | 'RPROBX'
  | 'RPROBCX' | 'RPROBNX' | 'STX' | 'STCX' | 'OHLCSTX' | 'OHLCSTCX';

export type LabelGeneratorType = 'FIXLB' | 'MEANLB' | 'LEXLB' | 'TRENDLB' | 'BOLB';

export type SplitterType = 'RollingSplitter' | 'ExpandingSplitter' | 'PurgedKFoldSplitter' | 'RangeSplitter';

// ============================================================================
// Param Definition (generic, reusable across tabs)
// ============================================================================

export interface ParamDef {
  key: string;
  label: string;
  type: 'int' | 'float' | 'string';
  default: number | string;
  min?: number;
  max?: number;
  step?: number;
  options?: string[];
}

// ============================================================================
// Signal Generator Definitions
// ============================================================================

export const SIGNAL_GENERATOR_DEFINITIONS: Record<SignalGeneratorType, {
  label: string;
  description: string;
  params: ParamDef[];
}> = {
  RAND: {
    label: 'Random Entries',
    description: 'Generate n random entry signals',
    params: [
      { key: 'n', label: 'Count', type: 'int', default: 10, min: 1, max: 500, step: 1 },
      { key: 'seed', label: 'Seed', type: 'int', default: 42, min: 0, max: 99999, step: 1 },
    ],
  },
  RANDX: {
    label: 'Random Entry/Exit',
    description: 'Generate n random entry/exit pairs',
    params: [
      { key: 'n', label: 'Count', type: 'int', default: 10, min: 1, max: 500, step: 1 },
      { key: 'seed', label: 'Seed', type: 'int', default: 42, min: 0, max: 99999, step: 1 },
    ],
  },
  RANDNX: {
    label: 'Random Entry/Exit (Hold)',
    description: 'Random entry/exit with min/max hold period',
    params: [
      { key: 'n', label: 'Count', type: 'int', default: 10, min: 1, max: 500, step: 1 },
      { key: 'seed', label: 'Seed', type: 'int', default: 42, min: 0, max: 99999, step: 1 },
      { key: 'min_hold', label: 'Min Hold', type: 'int', default: 1, min: 1, max: 100, step: 1 },
      { key: 'max_hold', label: 'Max Hold', type: 'int', default: 20, min: 1, max: 500, step: 1 },
    ],
  },
  RPROB: {
    label: 'Prob. Entries',
    description: 'Entries with fixed probability per bar',
    params: [
      { key: 'entry_prob', label: 'Entry Prob', type: 'float', default: 0.1, min: 0.01, max: 1.0, step: 0.01 },
      { key: 'seed', label: 'Seed', type: 'int', default: 42, min: 0, max: 99999, step: 1 },
    ],
  },
  RPROBX: {
    label: 'Prob. Entry/Exit',
    description: 'Entry and exit with independent probabilities',
    params: [
      { key: 'entry_prob', label: 'Entry Prob', type: 'float', default: 0.1, min: 0.01, max: 1.0, step: 0.01 },
      { key: 'exit_prob', label: 'Exit Prob', type: 'float', default: 0.1, min: 0.01, max: 1.0, step: 0.01 },
      { key: 'seed', label: 'Seed', type: 'int', default: 42, min: 0, max: 99999, step: 1 },
    ],
  },
  RPROBCX: {
    label: 'Prob. Entry/Exit (Cooldown)',
    description: 'Probabilistic with cooldown between trades',
    params: [
      { key: 'entry_prob', label: 'Entry Prob', type: 'float', default: 0.1, min: 0.01, max: 1.0, step: 0.01 },
      { key: 'exit_prob', label: 'Exit Prob', type: 'float', default: 0.1, min: 0.01, max: 1.0, step: 0.01 },
      { key: 'cooldown', label: 'Cooldown', type: 'int', default: 5, min: 0, max: 100, step: 1 },
      { key: 'seed', label: 'Seed', type: 'int', default: 42, min: 0, max: 99999, step: 1 },
    ],
  },
  RPROBNX: {
    label: 'Prob. Entry/Exit (N)',
    description: 'N probabilistic entry/exit pairs',
    params: [
      { key: 'n', label: 'Count', type: 'int', default: 10, min: 1, max: 500, step: 1 },
      { key: 'entry_prob', label: 'Entry Prob', type: 'float', default: 0.1, min: 0.01, max: 1.0, step: 0.01 },
      { key: 'exit_prob', label: 'Exit Prob', type: 'float', default: 0.2, min: 0.01, max: 1.0, step: 0.01 },
      { key: 'seed', label: 'Seed', type: 'int', default: 42, min: 0, max: 99999, step: 1 },
    ],
  },
  STX: {
    label: 'Stop/TP Exit',
    description: 'Stop-loss and take-profit exit signals',
    params: [
      { key: 'stop_loss', label: 'Stop Loss', type: 'float', default: 0.05, min: 0.001, max: 0.5, step: 0.005 },
      { key: 'take_profit', label: 'Take Profit', type: 'float', default: 0.1, min: 0.001, max: 1.0, step: 0.005 },
    ],
  },
  STCX: {
    label: 'Stop/TP/Trail Exit',
    description: 'Stop-loss, take-profit, and trailing stop exits',
    params: [
      { key: 'stop_loss', label: 'Stop Loss', type: 'float', default: 0.05, min: 0.001, max: 0.5, step: 0.005 },
      { key: 'take_profit', label: 'Take Profit', type: 'float', default: 0.1, min: 0.001, max: 1.0, step: 0.005 },
      { key: 'trailing_stop', label: 'Trail Stop', type: 'float', default: 0.03, min: 0.001, max: 0.5, step: 0.005 },
    ],
  },
  OHLCSTX: {
    label: 'OHLC Stop/TP',
    description: 'OHLC-aware stop-loss and take-profit',
    params: [
      { key: 'stop_loss', label: 'Stop Loss', type: 'float', default: 0.05, min: 0.001, max: 0.5, step: 0.005 },
      { key: 'take_profit', label: 'Take Profit', type: 'float', default: 0.1, min: 0.001, max: 1.0, step: 0.005 },
    ],
  },
  OHLCSTCX: {
    label: 'OHLC Stop/TP/Trail',
    description: 'OHLC-aware stop, take-profit, trailing stop',
    params: [
      { key: 'stop_loss', label: 'Stop Loss', type: 'float', default: 0.05, min: 0.001, max: 0.5, step: 0.005 },
      { key: 'take_profit', label: 'Take Profit', type: 'float', default: 0.1, min: 0.001, max: 1.0, step: 0.005 },
      { key: 'trailing_stop', label: 'Trail Stop', type: 'float', default: 0.03, min: 0.001, max: 0.5, step: 0.005 },
    ],
  },
};

// ============================================================================
// Label Generator Definitions
// ============================================================================

export const LABEL_GENERATOR_DEFINITIONS: Record<LabelGeneratorType, {
  label: string;
  description: string;
  params: ParamDef[];
}> = {
  FIXLB: {
    label: 'Fixed Horizon',
    description: 'Label based on return over a fixed horizon',
    params: [
      { key: 'horizon', label: 'Horizon', type: 'int', default: 5, min: 1, max: 100, step: 1 },
      { key: 'threshold', label: 'Threshold', type: 'float', default: 0.0, min: -0.1, max: 0.1, step: 0.001 },
    ],
  },
  MEANLB: {
    label: 'Mean Reversion',
    description: 'Label based on z-score from rolling mean',
    params: [
      { key: 'window', label: 'Window', type: 'int', default: 20, min: 5, max: 200, step: 1 },
      { key: 'threshold', label: 'Threshold', type: 'float', default: 1.0, min: 0.1, max: 4.0, step: 0.1 },
    ],
  },
  LEXLB: {
    label: 'Local Extrema',
    description: 'Label local minima (+1) and maxima (-1)',
    params: [
      { key: 'window', label: 'Window', type: 'int', default: 5, min: 1, max: 50, step: 1 },
    ],
  },
  TRENDLB: {
    label: 'Trend',
    description: 'Label uptrend (+1), downtrend (-1), sideways (0)',
    params: [
      { key: 'window', label: 'Window', type: 'int', default: 20, min: 5, max: 200, step: 1 },
      { key: 'threshold', label: 'Threshold', type: 'float', default: 0.0, min: -0.1, max: 0.1, step: 0.001 },
    ],
  },
  BOLB: {
    label: 'Bollinger Labels',
    description: 'Label by position within Bollinger Bands (-2 to +2)',
    params: [
      { key: 'window', label: 'Window', type: 'int', default: 20, min: 5, max: 200, step: 1 },
      { key: 'alpha', label: 'Alpha (Std)', type: 'float', default: 2.0, min: 0.5, max: 4.0, step: 0.1 },
    ],
  },
};

// ============================================================================
// Splitter Definitions
// ============================================================================

export const SPLITTER_DEFINITIONS: Record<SplitterType, {
  label: string;
  description: string;
  params: ParamDef[];
}> = {
  RollingSplitter: {
    label: 'Rolling Window',
    description: 'Fixed-size rolling train/test windows',
    params: [
      { key: 'window_len', label: 'Window Length', type: 'int', default: 252, min: 10, max: 2000, step: 1 },
      { key: 'test_len', label: 'Test Length', type: 'int', default: 63, min: 5, max: 500, step: 1 },
      { key: 'step', label: 'Step', type: 'int', default: 21, min: 1, max: 252, step: 1 },
    ],
  },
  ExpandingSplitter: {
    label: 'Expanding Window',
    description: 'Growing train window with fixed test window',
    params: [
      { key: 'min_len', label: 'Min Train Length', type: 'int', default: 252, min: 10, max: 2000, step: 1 },
      { key: 'test_len', label: 'Test Length', type: 'int', default: 63, min: 5, max: 500, step: 1 },
      { key: 'step', label: 'Step', type: 'int', default: 21, min: 1, max: 252, step: 1 },
    ],
  },
  PurgedKFoldSplitter: {
    label: 'Purged K-Fold',
    description: 'K-fold with purge & embargo for time series',
    params: [
      { key: 'n_splits', label: 'N Splits', type: 'int', default: 5, min: 2, max: 20, step: 1 },
      { key: 'purge_len', label: 'Purge Length', type: 'int', default: 5, min: 0, max: 50, step: 1 },
      { key: 'embargo_len', label: 'Embargo Length', type: 'int', default: 5, min: 0, max: 50, step: 1 },
    ],
  },
  RangeSplitter: {
    label: 'Custom Ranges',
    description: 'Manually defined train/test date ranges',
    params: [],
  },
};

// ============================================================================
// Data Source Definitions
// ============================================================================

export const DATA_SOURCE_DEFINITIONS: Record<DataSourceType, {
  label: string;
  description: string;
  extraParams: ParamDef[];
}> = {
  yfinance: {
    label: 'Yahoo Finance',
    description: 'Free stock/ETF/crypto data via yfinance',
    extraParams: [],
  },
  binance: {
    label: 'Binance',
    description: 'Crypto OHLCV data from Binance',
    extraParams: [
      { key: 'limit', label: 'Limit', type: 'int', default: 1000, min: 100, max: 10000, step: 100 },
    ],
  },
  ccxt: {
    label: 'CCXT (Multi-Exchange)',
    description: 'Crypto data via CCXT from any exchange',
    extraParams: [
      { key: 'exchange_id', label: 'Exchange', type: 'string', default: 'binance' },
      { key: 'limit', label: 'Limit', type: 'int', default: 1000, min: 100, max: 10000, step: 100 },
    ],
  },
  alpaca: {
    label: 'Alpaca',
    description: 'US equity data via Alpaca Markets API',
    extraParams: [
      { key: 'api_key', label: 'API Key', type: 'string', default: '' },
      { key: 'api_secret', label: 'API Secret', type: 'string', default: '' },
    ],
  },
  gbm: {
    label: 'Synthetic (GBM)',
    description: 'Geometric Brownian Motion synthetic data',
    extraParams: [
      { key: 's0', label: 'Start Price', type: 'float', default: 100, min: 1, max: 10000, step: 1 },
      { key: 'mu', label: 'Drift (mu)', type: 'float', default: 0.1, min: -1.0, max: 2.0, step: 0.01 },
      { key: 'sigma', label: 'Vol (sigma)', type: 'float', default: 0.2, min: 0.01, max: 2.0, step: 0.01 },
      { key: 'seed', label: 'Seed', type: 'int', default: 42, min: 0, max: 99999, step: 1 },
    ],
  },
};

// ============================================================================
// Indicator Definitions
// ============================================================================

export interface IndicatorDef {
  id: string;
  label: string;
  category: string;
  params: ParamDef[];
}

export const INDICATOR_DEFINITIONS: IndicatorDef[] = [
  // Trend
  { id: 'ma', label: 'Moving Average (SMA)', category: 'Trend', params: [
    { key: 'period', label: 'Period', type: 'int', default: 20, min: 2, max: 500, step: 1 },
  ]},
  { id: 'ema', label: 'EMA', category: 'Trend', params: [
    { key: 'period', label: 'Period', type: 'int', default: 20, min: 2, max: 500, step: 1 },
  ]},
  { id: 'bbands', label: 'Bollinger Bands', category: 'Trend', params: [
    { key: 'period', label: 'Period', type: 'int', default: 20, min: 5, max: 100, step: 1 },
    { key: 'alpha', label: 'Std Dev', type: 'float', default: 2.0, min: 0.5, max: 4.0, step: 0.1 },
  ]},
  { id: 'keltner', label: 'Keltner Channel', category: 'Trend', params: [
    { key: 'ema_period', label: 'EMA Period', type: 'int', default: 20, min: 5, max: 100, step: 1 },
    { key: 'atr_period', label: 'ATR Period', type: 'int', default: 10, min: 5, max: 50, step: 1 },
    { key: 'multiplier', label: 'Multiplier', type: 'float', default: 2.0, min: 0.5, max: 5.0, step: 0.1 },
  ]},
  { id: 'donchian', label: 'Donchian Channel', category: 'Trend', params: [
    { key: 'period', label: 'Period', type: 'int', default: 20, min: 5, max: 200, step: 1 },
  ]},
  // Momentum
  { id: 'rsi', label: 'RSI', category: 'Momentum', params: [
    { key: 'period', label: 'Period', type: 'int', default: 14, min: 2, max: 100, step: 1 },
  ]},
  { id: 'macd', label: 'MACD', category: 'Momentum', params: [
    { key: 'fast_period', label: 'Fast', type: 'int', default: 12, min: 2, max: 100, step: 1 },
    { key: 'slow_period', label: 'Slow', type: 'int', default: 26, min: 5, max: 200, step: 1 },
    { key: 'signal_period', label: 'Signal', type: 'int', default: 9, min: 2, max: 50, step: 1 },
  ]},
  { id: 'stoch', label: 'Stochastic', category: 'Momentum', params: [
    { key: 'k_period', label: '%K', type: 'int', default: 14, min: 5, max: 50, step: 1 },
    { key: 'd_period', label: '%D', type: 'int', default: 3, min: 2, max: 20, step: 1 },
  ]},
  { id: 'momentum', label: 'Momentum', category: 'Momentum', params: [
    { key: 'lookback', label: 'Lookback', type: 'int', default: 20, min: 1, max: 200, step: 1 },
  ]},
  { id: 'williams_r', label: 'Williams %R', category: 'Momentum', params: [
    { key: 'period', label: 'Period', type: 'int', default: 14, min: 5, max: 50, step: 1 },
  ]},
  { id: 'cci', label: 'CCI', category: 'Momentum', params: [
    { key: 'period', label: 'Period', type: 'int', default: 20, min: 5, max: 100, step: 1 },
  ]},
  // Volatility
  { id: 'atr', label: 'ATR', category: 'Volatility', params: [
    { key: 'period', label: 'Period', type: 'int', default: 14, min: 2, max: 100, step: 1 },
  ]},
  { id: 'mstd', label: 'Moving Std Dev', category: 'Volatility', params: [
    { key: 'period', label: 'Period', type: 'int', default: 20, min: 2, max: 200, step: 1 },
  ]},
  { id: 'zscore', label: 'Z-Score', category: 'Volatility', params: [
    { key: 'period', label: 'Period', type: 'int', default: 20, min: 5, max: 200, step: 1 },
  ]},
  // Volume
  { id: 'obv', label: 'On-Balance Volume', category: 'Volume', params: [] },
  { id: 'vwap', label: 'VWAP', category: 'Volume', params: [] },
  // Trend Strength
  { id: 'adx', label: 'ADX', category: 'Trend Strength', params: [
    { key: 'period', label: 'Period', type: 'int', default: 14, min: 5, max: 50, step: 1 },
  ]},
];

// ============================================================================
// Returns Analysis Types (vbt_returns + vbt_generic)
// ============================================================================

export type ReturnsAnalysisType = 'returns_stats' | 'drawdowns' | 'ranges' | 'rolling';

export type RollingMetric =
  | 'total' | 'annualized' | 'volatility' | 'sharpe' | 'sortino'
  | 'calmar' | 'omega' | 'info_ratio' | 'downside_risk';

export const RETURNS_ANALYSIS_DEFINITIONS: Record<ReturnsAnalysisType, {
  label: string;
  description: string;
  params: ParamDef[];
}> = {
  returns_stats: {
    label: 'Returns Stats',
    description: 'Full returns statistics: ratios, capture, volatility, skew/kurtosis',
    params: [
      { key: 'risk_free', label: 'Risk-Free Rate', type: 'float', default: 0.0, min: 0, max: 0.2, step: 0.001 },
      { key: 'n_trials', label: 'N Trials (Deflated Sharpe)', type: 'int', default: 1, min: 1, max: 1000, step: 1 },
      { key: 'omega_threshold', label: 'Omega Threshold', type: 'float', default: 0.0, min: -0.1, max: 0.1, step: 0.001 },
    ],
  },
  drawdowns: {
    label: 'Drawdowns',
    description: 'Drawdown analysis: max/avg depth, durations, recovery, active drawdown',
    params: [],
  },
  ranges: {
    label: 'Ranges',
    description: 'Range analysis: contiguous positive/negative return periods',
    params: [
      { key: 'threshold', label: 'Threshold', type: 'float', default: 0.0, min: -1.0, max: 1.0, step: 0.01 },
    ],
  },
  rolling: {
    label: 'Rolling Metrics',
    description: 'Rolling window risk/return metrics over time',
    params: [
      { key: 'window', label: 'Window', type: 'int', default: 252, min: 5, max: 2000, step: 1 },
      { key: 'risk_free', label: 'Risk-Free Rate', type: 'float', default: 0.0, min: 0, max: 0.2, step: 0.001 },
    ],
  },
};

export const ROLLING_METRIC_OPTIONS: { value: RollingMetric; label: string }[] = [
  { value: 'total', label: 'Rolling Total Return' },
  { value: 'annualized', label: 'Rolling Annualized Return' },
  { value: 'volatility', label: 'Rolling Volatility' },
  { value: 'sharpe', label: 'Rolling Sharpe Ratio' },
  { value: 'sortino', label: 'Rolling Sortino Ratio' },
  { value: 'calmar', label: 'Rolling Calmar Ratio' },
  { value: 'omega', label: 'Rolling Omega Ratio' },
  { value: 'info_ratio', label: 'Rolling Information Ratio' },
  { value: 'downside_risk', label: 'Rolling Downside Risk' },
];

// ============================================================================
// Indicator Mode (extended: calculate + signal generation + param sweep)
// ============================================================================

export type IndicatorMode =
  | 'calculate' | 'crossover_signals' | 'threshold_signals'
  | 'breakout_signals' | 'mean_reversion_signals' | 'signal_filter' | 'param_sweep';

export const INDICATOR_MODE_DEFINITIONS: Record<IndicatorMode, {
  label: string;
  description: string;
  params: ParamDef[];
}> = {
  calculate: {
    label: 'Calculate Indicator',
    description: 'Calculate a single indicator and return values',
    params: [],
  },
  crossover_signals: {
    label: 'Crossover Signals',
    description: 'Entry when fast crosses above slow, exit when fast crosses below',
    params: [
      { key: 'fast_indicator', label: 'Fast Indicator', type: 'string', default: 'ma', options: ['ma', 'ema'] },
      { key: 'fast_period', label: 'Fast Period', type: 'int', default: 10, min: 2, max: 500, step: 1 },
      { key: 'slow_indicator', label: 'Slow Indicator', type: 'string', default: 'ma', options: ['ma', 'ema'] },
      { key: 'slow_period', label: 'Slow Period', type: 'int', default: 20, min: 2, max: 500, step: 1 },
    ],
  },
  threshold_signals: {
    label: 'Threshold Signals',
    description: 'Entry when indicator crosses below lower, exit above upper (oscillator)',
    params: [
      { key: 'lower', label: 'Lower Threshold', type: 'float', default: 30, min: 0, max: 100, step: 1 },
      { key: 'upper', label: 'Upper Threshold', type: 'float', default: 70, min: 0, max: 100, step: 1 },
    ],
  },
  breakout_signals: {
    label: 'Breakout Signals',
    description: 'Entry on upper channel break, exit on lower channel break',
    params: [
      { key: 'channel', label: 'Channel', type: 'string', default: 'donchian', options: ['donchian', 'keltner', 'bbands'] },
      { key: 'period', label: 'Period', type: 'int', default: 20, min: 5, max: 200, step: 1 },
    ],
  },
  mean_reversion_signals: {
    label: 'Mean Reversion Signals',
    description: 'Entry when Z-score < -entry, exit when Z-score > exit',
    params: [
      { key: 'period', label: 'Z-Score Period', type: 'int', default: 20, min: 5, max: 200, step: 1 },
      { key: 'z_entry', label: 'Z Entry', type: 'float', default: 2.0, min: 0.5, max: 4.0, step: 0.1 },
      { key: 'z_exit', label: 'Z Exit', type: 'float', default: 0.0, min: -2.0, max: 2.0, step: 0.1 },
    ],
  },
  signal_filter: {
    label: 'Signal Filter',
    description: 'Apply indicator filter to entry signals (e.g., only trade when ADX > 25)',
    params: [
      { key: 'base_indicator', label: 'Base Indicator', type: 'string', default: 'rsi', options: ['rsi', 'macd', 'stoch', 'cci', 'williams_r', 'momentum'] },
      { key: 'base_period', label: 'Base Period', type: 'int', default: 14, min: 2, max: 200, step: 1 },
      { key: 'filter_indicator', label: 'Filter Indicator', type: 'string', default: 'adx', options: ['adx', 'atr', 'mstd', 'zscore', 'rsi'] },
      { key: 'filter_period', label: 'Filter Period', type: 'int', default: 14, min: 2, max: 200, step: 1 },
      { key: 'filter_threshold', label: 'Filter Threshold', type: 'float', default: 25, min: 0, max: 200, step: 1 },
      { key: 'filter_type', label: 'Filter Type', type: 'string', default: 'above', options: ['above', 'below'] },
    ],
  },
  param_sweep: {
    label: 'Parameter Sweep',
    description: 'Run indicator across all parameter combinations (IndicatorFactory.run_combs)',
    params: [],
  },
};
