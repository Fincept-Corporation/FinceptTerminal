// Type definitions for Equity Research module

export interface CandleData {
  time: number;
  open: number;
  high: number;
  low: number;
  close: number;
  volume: number;
}

export interface IndicatorData {
  time: number;
  value: number | null;
}

export interface MomentumIndicatorParams {
  rsi?: {
    period: number;
    overbought: number;
    oversold: number;
  };
  macd?: {
    fast_period: number;
    slow_period: number;
    signal_period: number;
  };
  stochastic?: {
    k_period: number;
    d_period: number;
    overbought: number;
    oversold: number;
  };
  cci?: {
    period: number;
    overbought: number;
    oversold: number;
  };
  roc?: {
    period: number;
  };
  williams_r?: {
    period: number;
    overbought: number;
    oversold: number;
  };
  awesome_oscillator?: {
    fast_period: number;
    slow_period: number;
  };
  tsi?: {
    long_period: number;
    short_period: number;
    signal_period: number;
  };
  ultimate_oscillator?: {
    period1: number;
    period2: number;
    period3: number;
    weight1: number;
    weight2: number;
    weight3: number;
  };
}

export interface MomentumIndicatorResult {
  rsi?: {
    values: IndicatorData[];
    signal: 'BUY' | 'SELL' | 'NEUTRAL';
    current_value: number;
    params: MomentumIndicatorParams['rsi'];
  };
  macd?: {
    macd_line: IndicatorData[];
    signal_line: IndicatorData[];
    histogram: IndicatorData[];
    signal: 'BUY' | 'SELL' | 'NEUTRAL';
    current_macd: number;
    current_signal: number;
    current_histogram: number;
    params: MomentumIndicatorParams['macd'];
  };
  stochastic?: {
    k_values: IndicatorData[];
    d_values: IndicatorData[];
    signal: 'BUY' | 'SELL' | 'NEUTRAL';
    current_k: number;
    current_d: number;
    params: MomentumIndicatorParams['stochastic'];
  };
  cci?: {
    values: IndicatorData[];
    signal: 'BUY' | 'SELL' | 'NEUTRAL';
    current_value: number;
    params: MomentumIndicatorParams['cci'];
  };
  roc?: {
    values: IndicatorData[];
    signal: 'BUY' | 'SELL' | 'NEUTRAL';
    current_value: number;
    params: MomentumIndicatorParams['roc'];
  };
  williams_r?: {
    values: IndicatorData[];
    signal: 'BUY' | 'SELL' | 'NEUTRAL';
    current_value: number;
    params: MomentumIndicatorParams['williams_r'];
  };
  awesome_oscillator?: {
    values: IndicatorData[];
    signal: 'BUY' | 'SELL' | 'NEUTRAL';
    current_value: number;
    params: MomentumIndicatorParams['awesome_oscillator'];
  };
  tsi?: {
    values: IndicatorData[];
    signal_line: IndicatorData[];
    signal: 'BUY' | 'SELL' | 'NEUTRAL';
    current_value: number;
    current_signal: number;
    params: MomentumIndicatorParams['tsi'];
  };
  ultimate_oscillator?: {
    values: IndicatorData[];
    signal: 'BUY' | 'SELL' | 'NEUTRAL';
    current_value: number;
    params: MomentumIndicatorParams['ultimate_oscillator'];
  };
}

export type TimeFrame = '1d' | '5d' | '1mo' | '3mo' | '6mo' | '1y' | '2y' | '5y' | 'max';

export interface TimeFrameConfig {
  value: TimeFrame;
  label: string;
  interval: '1m' | '5m' | '15m' | '30m' | '1h' | '1d' | '1wk' | '1mo';
}

export const TIME_FRAMES: TimeFrameConfig[] = [
  { value: '1d', label: '1 Day', interval: '5m' },
  { value: '5d', label: '5 Days', interval: '15m' },
  { value: '1mo', label: '1 Month', interval: '1h' },
  { value: '3mo', label: '3 Months', interval: '1d' },
  { value: '6mo', label: '6 Months', interval: '1d' },
  { value: '1y', label: '1 Year', interval: '1d' },
  { value: '2y', label: '2 Years', interval: '1wk' },
  { value: '5y', label: '5 Years', interval: '1wk' },
  { value: 'max', label: 'Max', interval: '1mo' },
];

export const DEFAULT_MOMENTUM_PARAMS: MomentumIndicatorParams = {
  rsi: {
    period: 14,
    overbought: 70,
    oversold: 30,
  },
  macd: {
    fast_period: 12,
    slow_period: 26,
    signal_period: 9,
  },
  stochastic: {
    k_period: 14,
    d_period: 3,
    overbought: 80,
    oversold: 20,
  },
  cci: {
    period: 20,
    overbought: 100,
    oversold: -100,
  },
  roc: {
    period: 12,
  },
  williams_r: {
    period: 14,
    overbought: -20,
    oversold: -80,
  },
  awesome_oscillator: {
    fast_period: 5,
    slow_period: 34,
  },
  tsi: {
    long_period: 25,
    short_period: 13,
    signal_period: 13,
  },
  ultimate_oscillator: {
    period1: 7,
    period2: 14,
    period3: 28,
    weight1: 4,
    weight2: 2,
    weight3: 1,
  },
};

export type IndicatorType = keyof MomentumIndicatorParams;

export interface IndicatorDisplayConfig {
  type: IndicatorType;
  name: string;
  description: string;
  enabled: boolean;
  color: string;
}
