/**
 * Backtesting Tab - Constants
 * Design system re-exported from portfolio-tab/finceptStyles for theme support.
 * Domain-specific backtesting constants defined here.
 */

import type { BacktestingProviderSlug } from './backtestingProviderConfigs';
import {
  FINCEPT as _FINCEPT,
  TYPOGRAPHY,
} from '../portfolio-tab/finceptStyles';

// ============================================================================
// Re-export themed design system (uses CSS variables for dynamic theming)
// ============================================================================

export const FINCEPT = _FINCEPT;

// ============================================================================
// Provider Accent Colors
// ============================================================================

export const PROVIDER_COLORS: Record<BacktestingProviderSlug, string> = {
  fincept: FINCEPT.ORANGE,
  vectorbt: FINCEPT.CYAN,
  backtestingpy: FINCEPT.GREEN,
  fasttrade: FINCEPT.YELLOW,
  zipline: '#E91E63',
  bt: '#FF6B35',
};

// ============================================================================
// Typography (re-export MONO as FONT_FAMILY for backward compat)
// ============================================================================

export const FONT_FAMILY = TYPOGRAPHY.MONO;

// ============================================================================
// Indicator Options
// ============================================================================

export const INDICATORS = [
  { id: 'ma', label: 'Moving Average (SMA)', params: ['period'] },
  { id: 'ema', label: 'Moving Average (EMA)', params: ['period'] },
  { id: 'mstd', label: 'Moving Std Dev', params: ['period'] },
  { id: 'rsi', label: 'RSI', params: ['period'] },
  { id: 'stoch', label: 'Stochastic', params: ['k_period', 'd_period'] },
  { id: 'cci', label: 'CCI', params: ['period'] },
  { id: 'williams_r', label: 'Williams %R', params: ['period'] },
  { id: 'macd', label: 'MACD', params: ['fast', 'slow', 'signal'] },
  { id: 'adx', label: 'ADX', params: ['period'] },
  { id: 'momentum', label: 'Momentum', params: ['lookback'] },
  { id: 'atr', label: 'ATR', params: ['period'] },
  { id: 'bbands', label: 'Bollinger Bands', params: ['period'] },
  { id: 'keltner', label: 'Keltner Channel', params: ['period'] },
  { id: 'donchian', label: 'Donchian Channel', params: ['period'] },
  { id: 'zscore', label: 'Z-Score', params: ['period'] },
  { id: 'obv', label: 'OBV', params: [] },
  { id: 'vwap', label: 'VWAP', params: [] },
] as const;

// ============================================================================
// Position Sizing Options
// ============================================================================

export const POSITION_SIZING = [
  { id: 'fixed', label: 'Fixed Size', params: ['size'] },
  { id: 'percent', label: 'Percent of Capital', params: ['percent'] },
  { id: 'kelly', label: 'Kelly Criterion', params: [] },
  { id: 'volatility', label: 'Volatility Target', params: ['targetVol'] },
  { id: 'risk', label: 'Risk-Based', params: ['riskPercent'] },
] as const;

// ============================================================================
// Signal Modes
// ============================================================================

export const SIGNAL_MODES = [
  { id: 'crossover_signals', label: 'Crossover', description: 'MA/EMA crossover signals' },
  { id: 'threshold_signals', label: 'Threshold', description: 'RSI/CCI threshold signals' },
  { id: 'breakout_signals', label: 'Breakout', description: 'Channel breakout signals' },
  { id: 'mean_reversion_signals', label: 'Mean Reversion', description: 'Z-score mean reversion' },
  { id: 'signal_filter', label: 'Signal Filter', description: 'Filter signals with indicators' },
] as const;

// ============================================================================
// ML Label Types (VectorBT)
// ============================================================================

export const LABEL_TYPES = [
  { id: 'FIXLB', label: 'Fixed Horizon', description: 'Fixed-time horizon labeling', params: ['horizon', 'threshold'] },
  { id: 'MEANLB', label: 'Mean Reversion', description: 'Mean-reversion labeling', params: ['window', 'threshold'] },
  { id: 'LEXLB', label: 'Local Extrema', description: 'Local extrema labeling', params: ['window'] },
  { id: 'TRENDLB', label: 'Trend Scan', description: 'Trend-scanning labeling', params: ['window', 'threshold', 'method'] },
  { id: 'BOLB', label: 'Bollinger Labels', description: 'Bollinger band labeling', params: ['window', 'alpha'] },
] as const;

// ============================================================================
// Splitter / Cross-Validation Types (VectorBT)
// ============================================================================

export const SPLITTER_TYPES = [
  { id: 'RollingSplitter', label: 'Rolling Window', description: 'Fixed-size rolling train/test splits', params: ['window_len', 'test_len', 'step'] },
  { id: 'ExpandingSplitter', label: 'Expanding Window', description: 'Expanding train window', params: ['min_len', 'test_len', 'step'] },
  { id: 'PurgedKFold', label: 'Purged K-Fold', description: 'K-fold with purge & embargo', params: ['n_splits', 'purge_len', 'embargo_len'] },
] as const;

// ============================================================================
// Returns Analysis Types (VectorBT)
// ============================================================================

export const RETURNS_ANALYSIS_TYPES = [
  { id: 'full', label: 'Full Analysis', description: 'Complete returns summary' },
  { id: 'rolling', label: 'Rolling Metrics', description: 'Rolling Sharpe, vol, returns' },
  { id: 'distribution', label: 'Distribution', description: 'Returns distribution analysis' },
  { id: 'benchmark_comparison', label: 'Benchmark', description: 'Compare vs benchmark' },
] as const;

// ============================================================================
// Random Signal Generator Types (VectorBT)
// ============================================================================

export const SIGNAL_GENERATORS = [
  { id: 'RAND', label: 'Random', description: 'Uniform random entry/exit', params: ['n', 'seed'] },
  { id: 'RANDX', label: 'Random (Exclusive)', description: 'Non-overlapping random signals', params: ['n', 'seed'] },
  { id: 'RANDNX', label: 'Random N (Excl.)', description: 'N random signals with hold range', params: ['n', 'min_hold', 'max_hold'] },
  { id: 'RPROB', label: 'Probability', description: 'Probability-based entry/exit', params: ['entry_prob', 'exit_prob', 'cooldown'] },
  { id: 'RPROBX', label: 'Probability (Excl.)', description: 'Non-overlapping probability signals', params: ['entry_prob', 'exit_prob', 'cooldown'] },
] as const;
