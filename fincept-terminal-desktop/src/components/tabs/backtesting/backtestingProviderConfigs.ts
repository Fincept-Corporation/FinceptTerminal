/**
 * Backtesting Provider Configurations
 *
 * Per-provider UI configs defining supported commands, command mappings,
 * available strategies, and category metadata.
 */

import {
  Activity, Play, Target, ChevronRight, Database, Zap, TrendingUp, Tag, Split,
  DollarSign, Search, Filter, Settings, BarChart3, Code, PieChart,
} from 'lucide-react';

// ============================================================================
// Types
// ============================================================================

export type BacktestingProviderSlug = 'vectorbt' | 'backtestingpy' | 'fasttrade' | 'zipline' | 'bt' | 'fincept';

export type CommandType =
  | 'backtest' | 'optimize' | 'walk_forward' | 'data' | 'indicator'
  | 'indicator_signals' | 'signals' | 'labels' | 'splits' | 'returns'
  | 'browse_strategies' | 'browse_indicators' | 'labels_to_signals';

interface StrategyParam {
  name: string;
  label: string;
  default: number;
  min?: number;
  max?: number;
  step?: number;
  type?: 'number' | 'range';
}

interface Strategy {
  id: string;
  name: string;
  description: string;
  params: StrategyParam[];
}

interface Command {
  id: CommandType;
  label: string;
  icon: React.ElementType;
  description: string;
  color: string;
}

interface CategoryInfoEntry {
  label: string;
  icon: React.ElementType;
  color: string;
}

export interface BacktestingProviderConfig {
  slug: BacktestingProviderSlug;
  displayName: string;
  commands: CommandType[];
  commandMap: Record<string, string>;
  strategies: Record<string, Strategy[]>;
  categoryInfo: Record<string, CategoryInfoEntry>;
}

// ============================================================================
// Colors (matching BacktestingTab FINCEPT palette)
// ============================================================================

const C = {
  ORANGE: '#FF8800', WHITE: '#FFFFFF', RED: '#FF3B3B', GREEN: '#00D66F',
  GRAY: '#787878', CYAN: '#00E5FF', YELLOW: '#FFD700', BLUE: '#0088FF', PURPLE: '#9D4EDD',
};

// ============================================================================
// Shared command definitions (full superset, filtered per-provider)
// ============================================================================

export const ALL_COMMANDS: Command[] = [
  { id: 'backtest', label: 'Backtest', icon: Play, description: 'Run strategy backtest', color: C.ORANGE },
  { id: 'optimize', label: 'Optimize', icon: Target, description: 'Parameter optimization', color: C.GREEN },
  { id: 'walk_forward', label: 'Walk Forward', icon: ChevronRight, description: 'Walk-forward validation', color: C.BLUE },
  { id: 'data', label: 'Data', icon: Database, description: 'Download market data', color: C.CYAN },
  { id: 'indicator', label: 'Indicators', icon: Activity, description: 'Calculate indicators', color: C.PURPLE },
  { id: 'indicator_signals', label: 'Ind. Signals', icon: Zap, description: 'Indicator signals', color: C.YELLOW },
  { id: 'signals', label: 'Signals', icon: TrendingUp, description: 'Generate signals', color: C.GREEN },
  { id: 'labels', label: 'Labels', icon: Tag, description: 'ML label generation', color: C.BLUE },
  { id: 'splits', label: 'Splits', icon: Split, description: 'Cross-validation', color: C.PURPLE },
  { id: 'returns', label: 'Returns', icon: DollarSign, description: 'Returns analysis', color: C.CYAN },
  { id: 'browse_strategies', label: 'Browse Strategies', icon: Search, description: 'View strategy catalog', color: C.GREEN },
  { id: 'browse_indicators', label: 'Browse Indicators', icon: Filter, description: 'View indicator catalog', color: C.PURPLE },
  { id: 'labels_to_signals', label: 'Labels\u2192Signals', icon: ChevronRight, description: 'Convert labels to signals', color: C.YELLOW },
];

// ============================================================================
// Shared strategies (available in all 3 providers)
// ============================================================================

const SHARED_TREND: Strategy[] = [
  {
    id: 'sma_crossover', name: 'SMA Crossover', description: 'Fast SMA crosses Slow SMA',
    params: [
      { name: 'fastPeriod', label: 'Fast Period', default: 10, min: 2, max: 100 },
      { name: 'slowPeriod', label: 'Slow Period', default: 20, min: 5, max: 200 },
    ],
  },
  {
    id: 'ema_crossover', name: 'EMA Crossover', description: 'Fast EMA crosses Slow EMA',
    params: [
      { name: 'fastPeriod', label: 'Fast Period', default: 10, min: 2, max: 100 },
      { name: 'slowPeriod', label: 'Slow Period', default: 20, min: 5, max: 200 },
    ],
  },
  {
    id: 'macd', name: 'MACD Crossover', description: 'MACD line crosses Signal line',
    params: [
      { name: 'fastPeriod', label: 'Fast Period', default: 12, min: 2, max: 50 },
      { name: 'slowPeriod', label: 'Slow Period', default: 26, min: 10, max: 100 },
      { name: 'signalPeriod', label: 'Signal Period', default: 9, min: 2, max: 50 },
    ],
  },
  {
    id: 'adx_trend', name: 'ADX Trend Filter', description: 'ADX-based trend following',
    params: [
      { name: 'adxPeriod', label: 'ADX Period', default: 14, min: 5, max: 50 },
      { name: 'adxThreshold', label: 'ADX Threshold', default: 25, min: 10, max: 50 },
    ],
  },
];

const SHARED_MEAN_REVERSION: Strategy[] = [
  {
    id: 'mean_reversion', name: 'Z-Score Reversion', description: 'Z-score mean reversion',
    params: [
      { name: 'window', label: 'Window', default: 20, min: 5, max: 100 },
      { name: 'threshold', label: 'Z-Score Threshold', default: 2.0, min: 0.5, max: 5.0, step: 0.1 },
    ],
  },
  {
    id: 'bollinger_bands', name: 'Bollinger Bands', description: 'Bollinger band mean reversion',
    params: [
      { name: 'period', label: 'Period', default: 20, min: 5, max: 100 },
      { name: 'stdDev', label: 'Std Dev', default: 2.0, min: 1.0, max: 4.0, step: 0.1 },
    ],
  },
  {
    id: 'rsi', name: 'RSI Mean Reversion', description: 'RSI oversold/overbought',
    params: [
      { name: 'period', label: 'Period', default: 14, min: 2, max: 50 },
      { name: 'oversold', label: 'Oversold', default: 30, min: 10, max: 40 },
      { name: 'overbought', label: 'Overbought', default: 70, min: 60, max: 90 },
    ],
  },
  {
    id: 'stochastic', name: 'Stochastic', description: 'Stochastic oscillator',
    params: [
      { name: 'kPeriod', label: 'K Period', default: 14, min: 5, max: 50 },
      { name: 'dPeriod', label: 'D Period', default: 3, min: 2, max: 20 },
      { name: 'oversold', label: 'Oversold', default: 20, min: 10, max: 30 },
      { name: 'overbought', label: 'Overbought', default: 80, min: 70, max: 90 },
    ],
  },
];

const SHARED_MOMENTUM: Strategy[] = [
  {
    id: 'momentum', name: 'Momentum (ROC)', description: 'Rate of change momentum',
    params: [
      { name: 'period', label: 'Period', default: 12, min: 5, max: 50 },
      { name: 'threshold', label: 'Threshold', default: 0.02, min: 0.01, max: 0.1, step: 0.01 },
    ],
  },
];

const SHARED_BREAKOUT: Strategy[] = [
  {
    id: 'breakout', name: 'Donchian Breakout', description: 'Donchian channel breakout',
    params: [
      { name: 'period', label: 'Period', default: 20, min: 5, max: 100 },
    ],
  },
];

// ============================================================================
// VectorBT-only strategies
// ============================================================================

const VBT_EXTRA_TREND: Strategy[] = [
  {
    id: 'keltner_breakout', name: 'Keltner Channel', description: 'Keltner channel breakout',
    params: [
      { name: 'period', label: 'Period', default: 20, min: 5, max: 100 },
      { name: 'atrMultiplier', label: 'ATR Multiplier', default: 2.0, min: 0.5, max: 5.0, step: 0.1 },
    ],
  },
  {
    id: 'triple_ma', name: 'Triple MA', description: 'Three moving average system',
    params: [
      { name: 'fastPeriod', label: 'Fast Period', default: 10, min: 2, max: 50 },
      { name: 'mediumPeriod', label: 'Medium Period', default: 20, min: 5, max: 100 },
      { name: 'slowPeriod', label: 'Slow Period', default: 50, min: 10, max: 200 },
    ],
  },
];

const VBT_EXTRA_MOMENTUM: Strategy[] = [
  {
    id: 'dual_momentum', name: 'Dual Momentum', description: 'Absolute + relative momentum',
    params: [
      { name: 'absolutePeriod', label: 'Absolute Period', default: 12, min: 3, max: 24 },
      { name: 'relativePeriod', label: 'Relative Period', default: 12, min: 3, max: 24 },
    ],
  },
];

const VBT_EXTRA_BREAKOUT: Strategy[] = [
  {
    id: 'volatility_breakout', name: 'Volatility Breakout', description: 'ATR-based volatility breakout',
    params: [
      { name: 'atrPeriod', label: 'ATR Period', default: 14, min: 5, max: 50 },
      { name: 'atrMultiplier', label: 'ATR Multiplier', default: 2.0, min: 0.5, max: 5.0, step: 0.1 },
    ],
  },
];

const VBT_MULTI_INDICATOR: Strategy[] = [
  {
    id: 'rsi_macd', name: 'RSI + MACD', description: 'RSI and MACD confluence',
    params: [
      { name: 'rsiPeriod', label: 'RSI Period', default: 14, min: 5, max: 30 },
      { name: 'macdFast', label: 'MACD Fast', default: 12, min: 5, max: 30 },
      { name: 'macdSlow', label: 'MACD Slow', default: 26, min: 10, max: 50 },
    ],
  },
  {
    id: 'macd_adx', name: 'MACD + ADX Filter', description: 'MACD with ADX trend filter',
    params: [
      { name: 'macdFast', label: 'MACD Fast', default: 12, min: 5, max: 30 },
      { name: 'macdSlow', label: 'MACD Slow', default: 26, min: 10, max: 50 },
      { name: 'adxPeriod', label: 'ADX Period', default: 14, min: 5, max: 30 },
    ],
  },
  {
    id: 'bollinger_rsi', name: 'Bollinger + RSI', description: 'Bollinger bands with RSI',
    params: [
      { name: 'bbPeriod', label: 'BB Period', default: 20, min: 10, max: 50 },
      { name: 'rsiPeriod', label: 'RSI Period', default: 14, min: 5, max: 30 },
    ],
  },
];

const VBT_OTHER: Strategy[] = [
  {
    id: 'williams_r', name: 'Williams %R', description: 'Williams %R oscillator',
    params: [
      { name: 'period', label: 'Period', default: 14, min: 5, max: 50 },
      { name: 'oversold', label: 'Oversold', default: -80, min: -90, max: -70 },
      { name: 'overbought', label: 'Overbought', default: -20, min: -30, max: -10 },
    ],
  },
  {
    id: 'cci', name: 'CCI', description: 'Commodity Channel Index',
    params: [
      { name: 'period', label: 'Period', default: 20, min: 5, max: 50 },
      { name: 'threshold', label: 'Threshold', default: 100, min: 50, max: 200 },
    ],
  },
  {
    id: 'obv_trend', name: 'OBV Trend', description: 'On-Balance Volume trend',
    params: [
      { name: 'maPeriod', label: 'MA Period', default: 20, min: 5, max: 100 },
    ],
  },
];

const VBT_CUSTOM: Strategy[] = [
  { id: 'code', name: 'Custom Code', description: 'Python custom strategy', params: [] },
];

// ============================================================================
// Fast-Trade extra strategies (not in VectorBT UI)
// ============================================================================

const FASTTRADE_EXTRA_TREND: Strategy[] = [
  {
    id: 'ichimoku', name: 'Ichimoku Cloud', description: 'Ichimoku Kinko Hyo trend system',
    params: [
      { name: 'tenkanPeriod', label: 'Tenkan Period', default: 9, min: 5, max: 30 },
      { name: 'kijunPeriod', label: 'Kijun Period', default: 26, min: 10, max: 60 },
      { name: 'senkouBPeriod', label: 'Senkou B Period', default: 52, min: 20, max: 120 },
    ],
  },
  {
    id: 'psar', name: 'Parabolic SAR', description: 'Parabolic stop and reverse',
    params: [
      { name: 'afStep', label: 'AF Step', default: 0.02, min: 0.01, max: 0.1, step: 0.01 },
      { name: 'afMax', label: 'AF Max', default: 0.2, min: 0.1, max: 0.5, step: 0.01 },
    ],
  },
  {
    id: 'ma_ribbon', name: 'MA Ribbon', description: 'Multiple moving average ribbon',
    params: [
      { name: 'shortestPeriod', label: 'Shortest MA', default: 5, min: 2, max: 20 },
      { name: 'longestPeriod', label: 'Longest MA', default: 50, min: 20, max: 200 },
      { name: 'count', label: 'MA Count', default: 6, min: 3, max: 12 },
    ],
  },
  {
    id: 'keltner_channel', name: 'Keltner Channel', description: 'Keltner channel trend system',
    params: [
      { name: 'period', label: 'EMA Period', default: 20, min: 5, max: 100 },
      { name: 'atrMultiplier', label: 'ATR Multiplier', default: 2.0, min: 0.5, max: 5.0, step: 0.1 },
    ],
  },
];

const FASTTRADE_EXTRA_MEAN_REVERSION: Strategy[] = [
  {
    id: 'mfi', name: 'Money Flow Index', description: 'Volume-weighted RSI reversal',
    params: [
      { name: 'period', label: 'Period', default: 14, min: 5, max: 50 },
      { name: 'oversold', label: 'Oversold', default: 20, min: 10, max: 40 },
      { name: 'overbought', label: 'Overbought', default: 80, min: 60, max: 90 },
    ],
  },
];

const FASTTRADE_EXTRA_MOMENTUM: Strategy[] = [
  {
    id: 'macd_zero_cross', name: 'MACD Zero Cross', description: 'MACD histogram zero-line crossover',
    params: [
      { name: 'fastPeriod', label: 'Fast Period', default: 12, min: 2, max: 50 },
      { name: 'slowPeriod', label: 'Slow Period', default: 26, min: 10, max: 100 },
      { name: 'signalPeriod', label: 'Signal Period', default: 9, min: 2, max: 50 },
    ],
  },
];

const FASTTRADE_EXTRA_BREAKOUT: Strategy[] = [
  {
    id: 'atr_trailing_stop', name: 'ATR Trailing Stop', description: 'ATR-based trailing stop breakout',
    params: [
      { name: 'atrPeriod', label: 'ATR Period', default: 14, min: 5, max: 50 },
      { name: 'atrMultiplier', label: 'ATR Multiplier', default: 3.0, min: 1.0, max: 6.0, step: 0.1 },
    ],
  },
];

const FASTTRADE_EXTRA_MULTI_INDICATOR: Strategy[] = [
  {
    id: 'trend_momentum', name: 'Trend + Momentum', description: 'MA trend filter with RSI momentum',
    params: [
      { name: 'maPeriod', label: 'MA Period', default: 50, min: 10, max: 200 },
      { name: 'rsiPeriod', label: 'RSI Period', default: 14, min: 5, max: 30 },
      { name: 'rsiThreshold', label: 'RSI Threshold', default: 50, min: 30, max: 70 },
    ],
  },
];

// ============================================================================
// Provider Configurations
// ============================================================================

const VECTORBT_CONFIG: BacktestingProviderConfig = {
  slug: 'vectorbt',
  displayName: 'VECTORBT',
  commands: [
    'backtest', 'optimize', 'walk_forward', 'data', 'indicator',
    'indicator_signals', 'signals', 'labels', 'splits', 'returns',
    'browse_strategies', 'browse_indicators', 'labels_to_signals',
  ],
  commandMap: {
    backtest: 'run_backtest',
    optimize: 'optimize',
    walk_forward: 'walk_forward',
    data: 'get_historical_data',
    indicator: 'indicator_param_sweep',
    indicator_signals: 'indicator_signals',
    signals: 'generate_signals',
    labels: 'generate_labels',
    splits: 'generate_splits',
    returns: 'analyze_returns',
    browse_strategies: 'get_strategies',
    browse_indicators: 'get_indicators',
    labels_to_signals: 'labels_to_signals',
  },
  strategies: {
    trend: [...SHARED_TREND, ...VBT_EXTRA_TREND],
    meanReversion: [...SHARED_MEAN_REVERSION],
    momentum: [...SHARED_MOMENTUM, ...VBT_EXTRA_MOMENTUM],
    breakout: [...SHARED_BREAKOUT, ...VBT_EXTRA_BREAKOUT],
    multiIndicator: [...VBT_MULTI_INDICATOR],
    other: [...VBT_OTHER],
    custom: [...VBT_CUSTOM],
  },
  categoryInfo: {
    trend: { label: 'Trend Following', icon: TrendingUp, color: C.BLUE },
    meanReversion: { label: 'Mean Reversion', icon: Target, color: C.PURPLE },
    momentum: { label: 'Momentum', icon: Zap, color: C.YELLOW },
    breakout: { label: 'Breakout', icon: BarChart3, color: C.GREEN },
    multiIndicator: { label: 'Multi-Indicator', icon: Activity, color: C.CYAN },
    other: { label: 'Other', icon: Settings, color: C.GRAY },
    custom: { label: 'Custom', icon: Code, color: C.ORANGE },
  },
};

// ============================================================================
// Backtesting.py extra strategies (from btp_strategies.py - 26 total)
// ============================================================================

const BTP_EXTRA_TREND: Strategy[] = [
  {
    id: 'keltner_breakout', name: 'Keltner Channel', description: 'Keltner channel breakout',
    params: [
      { name: 'period', label: 'Period', default: 20, min: 5, max: 100 },
      { name: 'atrMultiplier', label: 'ATR Multiplier', default: 2.0, min: 0.5, max: 5.0, step: 0.1 },
    ],
  },
  {
    id: 'triple_ma', name: 'Triple MA', description: 'Three moving average system',
    params: [
      { name: 'fastPeriod', label: 'Fast Period', default: 10, min: 2, max: 50 },
      { name: 'mediumPeriod', label: 'Medium Period', default: 20, min: 5, max: 100 },
      { name: 'slowPeriod', label: 'Slow Period', default: 50, min: 10, max: 200 },
    ],
  },
  {
    id: 'ichimoku', name: 'Ichimoku Cloud', description: 'Ichimoku Kinko Hyo trend system',
    params: [
      { name: 'tenkanPeriod', label: 'Tenkan Period', default: 9, min: 5, max: 30 },
      { name: 'kijunPeriod', label: 'Kijun Period', default: 26, min: 10, max: 60 },
      { name: 'senkouBPeriod', label: 'Senkou B Period', default: 52, min: 20, max: 120 },
    ],
  },
  {
    id: 'psar', name: 'Parabolic SAR', description: 'Parabolic stop and reverse',
    params: [
      { name: 'afStep', label: 'AF Step', default: 0.02, min: 0.01, max: 0.1, step: 0.01 },
      { name: 'afMax', label: 'AF Max', default: 0.2, min: 0.1, max: 0.5, step: 0.01 },
    ],
  },
];

const BTP_EXTRA_MEAN_REVERSION: Strategy[] = [
  {
    id: 'williams_r', name: 'Williams %R', description: 'Williams %R oscillator',
    params: [
      { name: 'period', label: 'Period', default: 14, min: 5, max: 50 },
      { name: 'oversold', label: 'Oversold', default: -80, min: -90, max: -70 },
      { name: 'overbought', label: 'Overbought', default: -20, min: -30, max: -10 },
    ],
  },
  {
    id: 'cci', name: 'CCI', description: 'Commodity Channel Index',
    params: [
      { name: 'period', label: 'Period', default: 20, min: 5, max: 50 },
      { name: 'threshold', label: 'Threshold', default: 100, min: 50, max: 200 },
    ],
  },
  {
    id: 'mfi', name: 'Money Flow Index', description: 'Volume-weighted RSI reversal',
    params: [
      { name: 'period', label: 'Period', default: 14, min: 5, max: 50 },
      { name: 'oversold', label: 'Oversold', default: 20, min: 10, max: 40 },
      { name: 'overbought', label: 'Overbought', default: 80, min: 60, max: 90 },
    ],
  },
];

const BTP_EXTRA_MOMENTUM: Strategy[] = [
  {
    id: 'dual_momentum', name: 'Dual Momentum', description: 'Absolute + relative momentum',
    params: [
      { name: 'absolutePeriod', label: 'Absolute Period', default: 12, min: 3, max: 24 },
      { name: 'relativePeriod', label: 'Relative Period', default: 12, min: 3, max: 24 },
    ],
  },
  {
    id: 'macd_zero_cross', name: 'MACD Zero Cross', description: 'MACD histogram zero-line crossover',
    params: [
      { name: 'fastPeriod', label: 'Fast Period', default: 12, min: 2, max: 50 },
      { name: 'slowPeriod', label: 'Slow Period', default: 26, min: 10, max: 100 },
      { name: 'signalPeriod', label: 'Signal Period', default: 9, min: 2, max: 50 },
    ],
  },
];

const BTP_EXTRA_BREAKOUT: Strategy[] = [
  {
    id: 'volatility_breakout', name: 'Volatility Breakout', description: 'ATR-based volatility breakout',
    params: [
      { name: 'atrPeriod', label: 'ATR Period', default: 14, min: 5, max: 50 },
      { name: 'atrMultiplier', label: 'ATR Multiplier', default: 2.0, min: 0.5, max: 5.0, step: 0.1 },
    ],
  },
  {
    id: 'atr_trailing_stop', name: 'ATR Trailing Stop', description: 'ATR-based trailing stop breakout',
    params: [
      { name: 'atrPeriod', label: 'ATR Period', default: 14, min: 5, max: 50 },
      { name: 'atrMultiplier', label: 'ATR Multiplier', default: 3.0, min: 1.0, max: 6.0, step: 0.1 },
    ],
  },
];

const BTP_MULTI_INDICATOR: Strategy[] = [
  {
    id: 'rsi_macd', name: 'RSI + MACD', description: 'RSI and MACD confluence',
    params: [
      { name: 'rsiPeriod', label: 'RSI Period', default: 14, min: 5, max: 30 },
      { name: 'macdFast', label: 'MACD Fast', default: 12, min: 5, max: 30 },
      { name: 'macdSlow', label: 'MACD Slow', default: 26, min: 10, max: 50 },
    ],
  },
  {
    id: 'macd_adx', name: 'MACD + ADX Filter', description: 'MACD with ADX trend filter',
    params: [
      { name: 'macdFast', label: 'MACD Fast', default: 12, min: 5, max: 30 },
      { name: 'macdSlow', label: 'MACD Slow', default: 26, min: 10, max: 50 },
      { name: 'adxPeriod', label: 'ADX Period', default: 14, min: 5, max: 30 },
    ],
  },
  {
    id: 'bollinger_rsi', name: 'Bollinger + RSI', description: 'Bollinger bands with RSI',
    params: [
      { name: 'bbPeriod', label: 'BB Period', default: 20, min: 10, max: 50 },
      { name: 'rsiPeriod', label: 'RSI Period', default: 14, min: 5, max: 30 },
    ],
  },
  {
    id: 'trend_momentum', name: 'Trend + Momentum', description: 'MA trend filter with RSI momentum',
    params: [
      { name: 'maPeriod', label: 'MA Period', default: 50, min: 10, max: 200 },
      { name: 'rsiPeriod', label: 'RSI Period', default: 14, min: 5, max: 30 },
      { name: 'rsiThreshold', label: 'RSI Threshold', default: 50, min: 30, max: 70 },
    ],
  },
];

const BTP_OTHER: Strategy[] = [
  {
    id: 'obv_trend', name: 'OBV Trend', description: 'On-Balance Volume trend',
    params: [
      { name: 'maPeriod', label: 'MA Period', default: 20, min: 5, max: 100 },
    ],
  },
];

const BACKTESTINGPY_CONFIG: BacktestingProviderConfig = {
  slug: 'backtestingpy',
  displayName: 'BACKTESTING.PY',
  commands: [
    'backtest', 'optimize', 'walk_forward', 'data', 'indicator',
    'signals', 'browse_strategies', 'browse_indicators',
  ],
  commandMap: {
    backtest: 'run_backtest',
    optimize: 'optimize',
    walk_forward: 'walk_forward',
    data: 'get_historical_data',
    indicator: 'calculate_indicator',
    signals: 'generate_signals',
    browse_strategies: 'get_strategies',
    browse_indicators: 'get_indicators',
  },
  strategies: {
    trend: [...SHARED_TREND, ...BTP_EXTRA_TREND],
    meanReversion: [...SHARED_MEAN_REVERSION, ...BTP_EXTRA_MEAN_REVERSION],
    momentum: [...SHARED_MOMENTUM, ...BTP_EXTRA_MOMENTUM],
    breakout: [...SHARED_BREAKOUT, ...BTP_EXTRA_BREAKOUT],
    multiIndicator: [...BTP_MULTI_INDICATOR],
    other: [...BTP_OTHER],
  },
  categoryInfo: {
    trend: { label: 'Trend Following', icon: TrendingUp, color: C.BLUE },
    meanReversion: { label: 'Mean Reversion', icon: Target, color: C.PURPLE },
    momentum: { label: 'Momentum', icon: Zap, color: C.YELLOW },
    breakout: { label: 'Breakout', icon: BarChart3, color: C.GREEN },
    multiIndicator: { label: 'Multi-Indicator', icon: Activity, color: C.CYAN },
    other: { label: 'Other', icon: Settings, color: C.GRAY },
  },
};

const FASTTRADE_CONFIG: BacktestingProviderConfig = {
  slug: 'fasttrade',
  displayName: 'FAST-TRADE',
  commands: ['backtest'],
  commandMap: {
    backtest: 'run_backtest',
  },
  strategies: {
    trend: [...SHARED_TREND, ...FASTTRADE_EXTRA_TREND],
    meanReversion: [...SHARED_MEAN_REVERSION, ...FASTTRADE_EXTRA_MEAN_REVERSION],
    momentum: [...SHARED_MOMENTUM, ...FASTTRADE_EXTRA_MOMENTUM],
    breakout: [...SHARED_BREAKOUT, ...FASTTRADE_EXTRA_BREAKOUT],
    multiIndicator: [...FASTTRADE_EXTRA_MULTI_INDICATOR],
  },
  categoryInfo: {
    trend: { label: 'Trend Following', icon: TrendingUp, color: C.BLUE },
    meanReversion: { label: 'Mean Reversion', icon: Target, color: C.PURPLE },
    momentum: { label: 'Momentum', icon: Zap, color: C.YELLOW },
    breakout: { label: 'Breakout', icon: BarChart3, color: C.GREEN },
    multiIndicator: { label: 'Multi-Indicator', icon: Activity, color: C.CYAN },
  },
};

// ============================================================================
// Zipline-Reloaded extra strategies
// ============================================================================

const ZIPLINE_EXTRA_TREND: Strategy[] = [
  {
    id: 'triple_ma', name: 'Triple MA', description: 'Three moving average system',
    params: [
      { name: 'fastPeriod', label: 'Fast Period', default: 10, min: 2, max: 50 },
      { name: 'mediumPeriod', label: 'Medium Period', default: 20, min: 5, max: 100 },
      { name: 'slowPeriod', label: 'Slow Period', default: 50, min: 10, max: 200 },
    ],
  },
  {
    id: 'keltner_channel', name: 'Keltner Channel', description: 'Keltner channel trend system',
    params: [
      { name: 'period', label: 'EMA Period', default: 20, min: 5, max: 100 },
      { name: 'atrMultiplier', label: 'ATR Multiplier', default: 2.0, min: 0.5, max: 5.0, step: 0.1 },
    ],
  },
];

const ZIPLINE_EXTRA_MEAN_REVERSION: Strategy[] = [
  {
    id: 'williams_r', name: 'Williams %R', description: 'Williams %R oscillator',
    params: [
      { name: 'period', label: 'Period', default: 14, min: 5, max: 50 },
      { name: 'oversold', label: 'Oversold', default: -80, min: -90, max: -70 },
      { name: 'overbought', label: 'Overbought', default: -20, min: -30, max: -10 },
    ],
  },
  {
    id: 'cci', name: 'CCI', description: 'Commodity Channel Index',
    params: [
      { name: 'period', label: 'Period', default: 20, min: 5, max: 50 },
      { name: 'threshold', label: 'Threshold', default: 100, min: 50, max: 200 },
    ],
  },
];

const ZIPLINE_EXTRA_MOMENTUM: Strategy[] = [
  {
    id: 'dual_momentum', name: 'Dual Momentum', description: 'Absolute + relative momentum',
    params: [
      { name: 'absolutePeriod', label: 'Absolute Period', default: 12, min: 3, max: 24 },
      { name: 'relativePeriod', label: 'Relative Period', default: 12, min: 3, max: 24 },
    ],
  },
  {
    id: 'macd_zero_cross', name: 'MACD Zero Cross', description: 'MACD histogram zero-line crossover',
    params: [
      { name: 'fastPeriod', label: 'Fast Period', default: 12, min: 2, max: 50 },
      { name: 'slowPeriod', label: 'Slow Period', default: 26, min: 10, max: 100 },
      { name: 'signalPeriod', label: 'Signal Period', default: 9, min: 2, max: 50 },
    ],
  },
];

const ZIPLINE_EXTRA_BREAKOUT: Strategy[] = [
  {
    id: 'volatility_breakout', name: 'Volatility Breakout', description: 'ATR-based volatility breakout',
    params: [
      { name: 'atrPeriod', label: 'ATR Period', default: 14, min: 5, max: 50 },
      { name: 'atrMultiplier', label: 'ATR Multiplier', default: 2.0, min: 0.5, max: 5.0, step: 0.1 },
    ],
  },
];

const ZIPLINE_MULTI_INDICATOR: Strategy[] = [
  {
    id: 'rsi_macd', name: 'RSI + MACD', description: 'RSI and MACD confluence',
    params: [
      { name: 'rsiPeriod', label: 'RSI Period', default: 14, min: 5, max: 30 },
      { name: 'macdFast', label: 'MACD Fast', default: 12, min: 5, max: 30 },
      { name: 'macdSlow', label: 'MACD Slow', default: 26, min: 10, max: 50 },
    ],
  },
  {
    id: 'macd_adx', name: 'MACD + ADX Filter', description: 'MACD with ADX trend filter',
    params: [
      { name: 'macdFast', label: 'MACD Fast', default: 12, min: 5, max: 30 },
      { name: 'macdSlow', label: 'MACD Slow', default: 26, min: 10, max: 50 },
      { name: 'adxPeriod', label: 'ADX Period', default: 14, min: 5, max: 30 },
    ],
  },
  {
    id: 'bollinger_rsi', name: 'Bollinger + RSI', description: 'Bollinger bands with RSI',
    params: [
      { name: 'bbPeriod', label: 'BB Period', default: 20, min: 10, max: 50 },
      { name: 'rsiPeriod', label: 'RSI Period', default: 14, min: 5, max: 30 },
    ],
  },
];

const ZIPLINE_OTHER: Strategy[] = [
  {
    id: 'obv_trend', name: 'OBV Trend', description: 'On-Balance Volume trend',
    params: [
      { name: 'maPeriod', label: 'MA Period', default: 20, min: 5, max: 100 },
    ],
  },
];

const ZIPLINE_PIPELINE: Strategy[] = [
  {
    id: 'pipeline', name: 'Custom Pipeline', description: 'Factor-based screening and ranking (Zipline Pipeline API)',
    params: [
      { name: 'topN', label: 'Top N', default: 5, min: 1, max: 50 },
      { name: 'rebalanceDays', label: 'Rebalance Days', default: 21, min: 5, max: 63 },
    ],
  },
  {
    id: 'scheduled_rebalance', name: 'Scheduled Rebalance', description: 'Monthly rebalance using schedule_function',
    params: [
      { name: 'momentumWindow', label: 'Momentum Window', default: 63, min: 21, max: 252 },
    ],
  },
];

const ZIPLINE_CONFIG: BacktestingProviderConfig = {
  slug: 'zipline',
  displayName: 'ZIPLINE',
  commands: [
    'backtest', 'optimize', 'walk_forward', 'data', 'indicator',
    'indicator_signals', 'signals', 'labels', 'splits', 'returns',
    'browse_strategies', 'browse_indicators', 'labels_to_signals',
  ],
  commandMap: {
    backtest: 'run_backtest',
    optimize: 'optimize',
    walk_forward: 'walk_forward',
    data: 'get_historical_data',
    indicator: 'calculate_indicator',
    indicator_signals: 'indicator_signals',
    signals: 'generate_signals',
    labels: 'generate_labels',
    splits: 'generate_splits',
    returns: 'analyze_returns',
    browse_strategies: 'get_strategies',
    browse_indicators: 'get_indicators',
    labels_to_signals: 'labels_to_signals',
  },
  strategies: {
    trend: [...SHARED_TREND, ...ZIPLINE_EXTRA_TREND],
    meanReversion: [...SHARED_MEAN_REVERSION, ...ZIPLINE_EXTRA_MEAN_REVERSION],
    momentum: [...SHARED_MOMENTUM, ...ZIPLINE_EXTRA_MOMENTUM],
    breakout: [...SHARED_BREAKOUT, ...ZIPLINE_EXTRA_BREAKOUT],
    multiIndicator: [...ZIPLINE_MULTI_INDICATOR],
    other: [...ZIPLINE_OTHER],
    pipeline: [...ZIPLINE_PIPELINE],
  },
  categoryInfo: {
    trend: { label: 'Trend Following', icon: TrendingUp, color: C.BLUE },
    meanReversion: { label: 'Mean Reversion', icon: Target, color: C.PURPLE },
    momentum: { label: 'Momentum', icon: Zap, color: C.YELLOW },
    breakout: { label: 'Breakout', icon: BarChart3, color: C.GREEN },
    multiIndicator: { label: 'Multi-Indicator', icon: Activity, color: C.CYAN },
    other: { label: 'Other', icon: Settings, color: C.GRAY },
    pipeline: { label: 'Pipeline', icon: Filter, color: C.ORANGE },
  },
};

// ============================================================================
// BT (Flexible Portfolio Backtesting) strategies
// ============================================================================

const BT_PORTFOLIO: Strategy[] = [
  {
    id: 'equal_weight', name: 'Equal Weight', description: 'Equal-weight allocation across all assets',
    params: [
      { name: 'rebalancePeriod', label: 'Rebalance Period', default: 21, min: 5, max: 63 },
    ],
  },
  {
    id: 'inv_vol', name: 'Inverse Volatility', description: 'Weight assets inversely by volatility',
    params: [
      { name: 'lookback', label: 'Lookback (days)', default: 20, min: 10, max: 120 },
    ],
  },
  {
    id: 'mean_var', name: 'Mean-Variance', description: 'Markowitz mean-variance optimized portfolio',
    params: [
      { name: 'lookback', label: 'Lookback (days)', default: 60, min: 20, max: 252 },
    ],
  },
  {
    id: 'risk_parity', name: 'Risk Parity', description: 'Equal risk contribution (ERC) portfolio',
    params: [
      { name: 'lookback', label: 'Lookback (days)', default: 60, min: 20, max: 252 },
    ],
  },
  {
    id: 'target_vol', name: 'Target Volatility', description: 'Target a specific portfolio volatility',
    params: [
      { name: 'targetVol', label: 'Target Vol (%)', default: 10, min: 1, max: 50 },
      { name: 'lookback', label: 'Lookback (days)', default: 20, min: 10, max: 120 },
    ],
  },
  {
    id: 'min_var', name: 'Minimum Variance', description: 'Minimum variance portfolio (lowest risk)',
    params: [
      { name: 'lookback', label: 'Lookback (days)', default: 60, min: 20, max: 252 },
    ],
  },
];

const BT_EXTRA_MOMENTUM: Strategy[] = [
  {
    id: 'momentum_topn', name: 'Momentum Top-N', description: 'Select top N assets by momentum',
    params: [
      { name: 'topN', label: 'Top N', default: 5, min: 1, max: 50 },
      { name: 'lookback', label: 'Momentum Lookback', default: 60, min: 10, max: 252 },
    ],
  },
  {
    id: 'momentum_inv_vol', name: 'Momentum + Inv Vol', description: 'Top-N momentum with inverse vol weighting',
    params: [
      { name: 'topN', label: 'Top N', default: 5, min: 1, max: 50 },
      { name: 'lookback', label: 'Lookback', default: 60, min: 10, max: 252 },
    ],
  },
];

const BT_CONFIG: BacktestingProviderConfig = {
  slug: 'bt',
  displayName: 'BT',
  commands: [
    'backtest', 'optimize', 'walk_forward', 'data', 'indicator',
    'indicator_signals', 'signals', 'labels', 'splits', 'returns',
    'browse_strategies', 'browse_indicators', 'labels_to_signals',
  ],
  commandMap: {
    backtest: 'run_backtest',
    optimize: 'optimize',
    walk_forward: 'walk_forward',
    data: 'get_historical_data',
    indicator: 'calculate_indicator',
    indicator_signals: 'indicator_signals',
    signals: 'generate_signals',
    labels: 'generate_labels',
    splits: 'generate_splits',
    returns: 'analyze_returns',
    browse_strategies: 'get_strategies',
    browse_indicators: 'get_indicators',
    labels_to_signals: 'labels_to_signals',
  },
  strategies: {
    portfolio: [...BT_PORTFOLIO],
    trend: [...SHARED_TREND],
    meanReversion: [...SHARED_MEAN_REVERSION],
    momentum: [...SHARED_MOMENTUM, ...BT_EXTRA_MOMENTUM],
    breakout: [...SHARED_BREAKOUT],
  },
  categoryInfo: {
    portfolio: { label: 'Portfolio Allocation', icon: PieChart, color: C.ORANGE },
    trend: { label: 'Trend Following', icon: TrendingUp, color: C.BLUE },
    meanReversion: { label: 'Mean Reversion', icon: Target, color: C.PURPLE },
    momentum: { label: 'Momentum', icon: Zap, color: C.YELLOW },
    breakout: { label: 'Breakout', icon: BarChart3, color: C.GREEN },
  },
};

// ============================================================================
// Fincept Strategies (400+ from _registry.py)
// Note: Strategies loaded dynamically via list_strategies Tauri command
// ============================================================================

const FINCEPT_CONFIG: BacktestingProviderConfig = {
  slug: 'fincept',
  displayName: 'FINCEPT ENGINE',
  commands: ['backtest', 'browse_strategies'],
  commandMap: {
    backtest: 'execute_fincept_strategy',
    browse_strategies: 'list_strategies',
  },
  strategies: {
    // Dynamically loaded from _registry.py
    // Will be populated by list_strategies() command
  },
  categoryInfo: {
    template: { label: 'Templates', icon: Code, color: C.ORANGE },
    crypto: { label: 'Crypto', icon: DollarSign, color: C.YELLOW },
    futures: { label: 'Futures', icon: TrendingUp, color: C.BLUE },
    options: { label: 'Options', icon: Target, color: C.PURPLE },
    forex: { label: 'Forex', icon: Zap, color: C.GREEN },
    index: { label: 'Index', icon: BarChart3, color: C.CYAN },
    'portfolio management': { label: 'Portfolio', icon: PieChart, color: C.CYAN },
    'alpha model': { label: 'Alpha Models', icon: Activity, color: C.RED },
    'risk management': { label: 'Risk', icon: Settings, color: C.GRAY },
    'universe selection': { label: 'Universe', icon: Search, color: C.BLUE },
    'regression test': { label: 'Regression', icon: Filter, color: C.GRAY },
    'general strategy': { label: 'General', icon: BarChart3, color: C.WHITE },
    brokerage: { label: 'Brokerage', icon: Settings, color: C.GRAY },
    'data consolidation': { label: 'Data', icon: Database, color: C.PURPLE },
  },
};

// ============================================================================
// Exports
// ============================================================================

export const PROVIDER_CONFIGS: Record<BacktestingProviderSlug, BacktestingProviderConfig> = {
  vectorbt: VECTORBT_CONFIG,
  backtestingpy: BACKTESTINGPY_CONFIG,
  fasttrade: FASTTRADE_CONFIG,
  zipline: ZIPLINE_CONFIG,
  bt: BT_CONFIG,
  fincept: FINCEPT_CONFIG,
};

export const PROVIDER_OPTIONS: { slug: BacktestingProviderSlug; displayName: string }[] = [
  { slug: 'fincept', displayName: 'FINCEPT ENGINE' },
  { slug: 'vectorbt', displayName: 'VECTORBT' },
  { slug: 'backtestingpy', displayName: 'BACKTESTING.PY' },
  { slug: 'fasttrade', displayName: 'FAST-TRADE' },
  { slug: 'zipline', displayName: 'ZIPLINE' },
  { slug: 'bt', displayName: 'BT' },
];
