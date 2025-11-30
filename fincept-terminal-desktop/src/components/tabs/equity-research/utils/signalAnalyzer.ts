// Signal analysis utilities for technical indicators

import { MomentumIndicatorParams, IndicatorData } from '../types';

export type Signal = 'BUY' | 'SELL' | 'NEUTRAL';

export interface SignalResult {
  signal: Signal;
  strength: number; // 0-100
  reason: string;
}

/**
 * Analyzes RSI indicator for buy/sell signals
 */
export function analyzeRSI(
  values: IndicatorData[],
  params: MomentumIndicatorParams['rsi']
): SignalResult {
  if (!params || values.length === 0) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'Insufficient data' };
  }

  const currentValue = values[values.length - 1]?.value;
  if (currentValue === null || currentValue === undefined) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'No current value' };
  }

  if (currentValue <= params.oversold) {
    const strength = Math.min(100, ((params.oversold - currentValue) / params.oversold) * 100);
    return {
      signal: 'BUY',
      strength,
      reason: `RSI ${currentValue.toFixed(2)} is in oversold territory (<${params.oversold})`,
    };
  }

  if (currentValue >= params.overbought) {
    const strength = Math.min(100, ((currentValue - params.overbought) / (100 - params.overbought)) * 100);
    return {
      signal: 'SELL',
      strength,
      reason: `RSI ${currentValue.toFixed(2)} is in overbought territory (>${params.overbought})`,
    };
  }

  return {
    signal: 'NEUTRAL',
    strength: 50,
    reason: `RSI ${currentValue.toFixed(2)} is neutral (${params.oversold}-${params.overbought})`,
  };
}

/**
 * Analyzes MACD indicator for buy/sell signals
 */
export function analyzeMACD(
  macdLine: IndicatorData[],
  signalLine: IndicatorData[],
  histogram: IndicatorData[]
): SignalResult {
  if (macdLine.length < 2 || signalLine.length < 2 || histogram.length < 2) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'Insufficient data' };
  }

  const currentHistogram = histogram[histogram.length - 1]?.value;
  const previousHistogram = histogram[histogram.length - 2]?.value;
  const currentMACD = macdLine[macdLine.length - 1]?.value;
  const currentSignal = signalLine[signalLine.length - 1]?.value;

  if (
    currentHistogram === null ||
    previousHistogram === null ||
    currentMACD === null ||
    currentSignal === null
  ) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'No current values' };
  }

  // Bullish crossover
  if (previousHistogram <= 0 && currentHistogram > 0) {
    return {
      signal: 'BUY',
      strength: 80,
      reason: 'MACD bullish crossover - MACD line crossed above signal line',
    };
  }

  // Bearish crossover
  if (previousHistogram >= 0 && currentHistogram < 0) {
    return {
      signal: 'SELL',
      strength: 80,
      reason: 'MACD bearish crossover - MACD line crossed below signal line',
    };
  }

  // Strong bullish momentum
  if (currentHistogram > 0 && currentHistogram > previousHistogram) {
    const strength = Math.min(100, Math.abs(currentHistogram) * 50);
    return {
      signal: 'BUY',
      strength,
      reason: 'MACD shows increasing bullish momentum',
    };
  }

  // Strong bearish momentum
  if (currentHistogram < 0 && currentHistogram < previousHistogram) {
    const strength = Math.min(100, Math.abs(currentHistogram) * 50);
    return {
      signal: 'SELL',
      strength,
      reason: 'MACD shows increasing bearish momentum',
    };
  }

  return {
    signal: 'NEUTRAL',
    strength: 50,
    reason: 'MACD shows neutral momentum',
  };
}

/**
 * Analyzes Stochastic Oscillator for buy/sell signals
 */
export function analyzeStochastic(
  kValues: IndicatorData[],
  dValues: IndicatorData[],
  params: MomentumIndicatorParams['stochastic']
): SignalResult {
  if (!params || kValues.length < 2 || dValues.length < 2) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'Insufficient data' };
  }

  const currentK = kValues[kValues.length - 1]?.value;
  const currentD = dValues[dValues.length - 1]?.value;
  const previousK = kValues[kValues.length - 2]?.value;
  const previousD = dValues[dValues.length - 2]?.value;

  if (currentK === null || currentD === null || previousK === null || previousD === null) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'No current values' };
  }

  // Oversold with bullish crossover
  if (currentK < params.oversold && previousK < previousD && currentK > currentD) {
    return {
      signal: 'BUY',
      strength: 90,
      reason: `Stochastic oversold bullish crossover (K=${currentK.toFixed(2)})`,
    };
  }

  // Overbought with bearish crossover
  if (currentK > params.overbought && previousK > previousD && currentK < currentD) {
    return {
      signal: 'SELL',
      strength: 90,
      reason: `Stochastic overbought bearish crossover (K=${currentK.toFixed(2)})`,
    };
  }

  // Oversold condition
  if (currentK < params.oversold) {
    return {
      signal: 'BUY',
      strength: 70,
      reason: `Stochastic in oversold territory (K=${currentK.toFixed(2)})`,
    };
  }

  // Overbought condition
  if (currentK > params.overbought) {
    return {
      signal: 'SELL',
      strength: 70,
      reason: `Stochastic in overbought territory (K=${currentK.toFixed(2)})`,
    };
  }

  return {
    signal: 'NEUTRAL',
    strength: 50,
    reason: `Stochastic neutral (K=${currentK.toFixed(2)}, D=${currentD.toFixed(2)})`,
  };
}

/**
 * Analyzes CCI (Commodity Channel Index) for buy/sell signals
 */
export function analyzeCCI(
  values: IndicatorData[],
  params: MomentumIndicatorParams['cci']
): SignalResult {
  if (!params || values.length === 0) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'Insufficient data' };
  }

  const currentValue = values[values.length - 1]?.value;
  if (currentValue === null || currentValue === undefined) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'No current value' };
  }

  if (currentValue <= params.oversold) {
    const strength = Math.min(100, (Math.abs(currentValue - params.oversold) / 100) * 100);
    return {
      signal: 'BUY',
      strength,
      reason: `CCI ${currentValue.toFixed(2)} indicates oversold condition`,
    };
  }

  if (currentValue >= params.overbought) {
    const strength = Math.min(100, (Math.abs(currentValue - params.overbought) / 100) * 100);
    return {
      signal: 'SELL',
      strength,
      reason: `CCI ${currentValue.toFixed(2)} indicates overbought condition`,
    };
  }

  return {
    signal: 'NEUTRAL',
    strength: 50,
    reason: `CCI ${currentValue.toFixed(2)} is neutral`,
  };
}

/**
 * Analyzes Rate of Change (ROC) for buy/sell signals
 */
export function analyzeROC(values: IndicatorData[]): SignalResult {
  if (values.length < 2) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'Insufficient data' };
  }

  const currentValue = values[values.length - 1]?.value;
  const previousValue = values[values.length - 2]?.value;

  if (currentValue === null || previousValue === null) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'No current values' };
  }

  // Strong positive momentum
  if (currentValue > 5 && currentValue > previousValue) {
    const strength = Math.min(100, Math.abs(currentValue) * 5);
    return {
      signal: 'BUY',
      strength,
      reason: `Strong positive ROC ${currentValue.toFixed(2)}%`,
    };
  }

  // Strong negative momentum
  if (currentValue < -5 && currentValue < previousValue) {
    const strength = Math.min(100, Math.abs(currentValue) * 5);
    return {
      signal: 'SELL',
      strength,
      reason: `Strong negative ROC ${currentValue.toFixed(2)}%`,
    };
  }

  // Reversal from negative to positive
  if (previousValue < 0 && currentValue > 0) {
    return {
      signal: 'BUY',
      strength: 75,
      reason: 'ROC turning positive - momentum reversal',
    };
  }

  // Reversal from positive to negative
  if (previousValue > 0 && currentValue < 0) {
    return {
      signal: 'SELL',
      strength: 75,
      reason: 'ROC turning negative - momentum reversal',
    };
  }

  return {
    signal: 'NEUTRAL',
    strength: 50,
    reason: `ROC ${currentValue.toFixed(2)}% shows neutral momentum`,
  };
}

/**
 * Analyzes Williams %R for buy/sell signals
 */
export function analyzeWilliamsR(
  values: IndicatorData[],
  params: MomentumIndicatorParams['williams_r']
): SignalResult {
  if (!params || values.length === 0) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'Insufficient data' };
  }

  const currentValue = values[values.length - 1]?.value;
  if (currentValue === null || currentValue === undefined) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'No current value' };
  }

  if (currentValue <= params.oversold) {
    const strength = Math.min(100, ((params.oversold - currentValue) / 20) * 100);
    return {
      signal: 'BUY',
      strength,
      reason: `Williams %R ${currentValue.toFixed(2)} is oversold`,
    };
  }

  if (currentValue >= params.overbought) {
    const strength = Math.min(100, ((currentValue - params.overbought) / 20) * 100);
    return {
      signal: 'SELL',
      strength,
      reason: `Williams %R ${currentValue.toFixed(2)} is overbought`,
    };
  }

  return {
    signal: 'NEUTRAL',
    strength: 50,
    reason: `Williams %R ${currentValue.toFixed(2)} is neutral`,
  };
}

/**
 * Analyzes Awesome Oscillator for buy/sell signals
 */
export function analyzeAwesomeOscillator(values: IndicatorData[]): SignalResult {
  if (values.length < 3) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'Insufficient data' };
  }

  const current = values[values.length - 1]?.value;
  const previous = values[values.length - 2]?.value;
  const twoBarsAgo = values[values.length - 3]?.value;

  if (current === null || previous === null || twoBarsAgo === null) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'No current values' };
  }

  // Twin Peaks (bullish)
  if (previous < 0 && current > previous && previous < twoBarsAgo) {
    return {
      signal: 'BUY',
      strength: 85,
      reason: 'AO bullish twin peaks pattern',
    };
  }

  // Twin Peaks (bearish)
  if (previous > 0 && current < previous && previous > twoBarsAgo) {
    return {
      signal: 'SELL',
      strength: 85,
      reason: 'AO bearish twin peaks pattern',
    };
  }

  // Zero line cross (bullish)
  if (previous <= 0 && current > 0) {
    return {
      signal: 'BUY',
      strength: 80,
      reason: 'AO crossed above zero line',
    };
  }

  // Zero line cross (bearish)
  if (previous >= 0 && current < 0) {
    return {
      signal: 'SELL',
      strength: 80,
      reason: 'AO crossed below zero line',
    };
  }

  // Momentum strength
  if (current > 0 && current > previous) {
    const strength = Math.min(100, Math.abs(current) * 50);
    return {
      signal: 'BUY',
      strength,
      reason: `AO shows bullish momentum (${current.toFixed(4)})`,
    };
  }

  if (current < 0 && current < previous) {
    const strength = Math.min(100, Math.abs(current) * 50);
    return {
      signal: 'SELL',
      strength,
      reason: `AO shows bearish momentum (${current.toFixed(4)})`,
    };
  }

  return {
    signal: 'NEUTRAL',
    strength: 50,
    reason: `AO neutral at ${current.toFixed(4)}`,
  };
}

/**
 * Analyzes TSI (True Strength Index) for buy/sell signals
 */
export function analyzeTSI(
  values: IndicatorData[],
  signalLine: IndicatorData[]
): SignalResult {
  if (values.length < 2 || signalLine.length < 2) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'Insufficient data' };
  }

  const currentTSI = values[values.length - 1]?.value;
  const previousTSI = values[values.length - 2]?.value;
  const currentSignal = signalLine[signalLine.length - 1]?.value;
  const previousSignal = signalLine[signalLine.length - 2]?.value;

  if (
    currentTSI === null ||
    previousTSI === null ||
    currentSignal === null ||
    previousSignal === null
  ) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'No current values' };
  }

  // Bullish crossover
  if (previousTSI < previousSignal && currentTSI > currentSignal) {
    return {
      signal: 'BUY',
      strength: 85,
      reason: 'TSI bullish crossover',
    };
  }

  // Bearish crossover
  if (previousTSI > previousSignal && currentTSI < currentSignal) {
    return {
      signal: 'SELL',
      strength: 85,
      reason: 'TSI bearish crossover',
    };
  }

  // Overbought (>25)
  if (currentTSI > 25) {
    return {
      signal: 'SELL',
      strength: 70,
      reason: `TSI overbought at ${currentTSI.toFixed(2)}`,
    };
  }

  // Oversold (<-25)
  if (currentTSI < -25) {
    return {
      signal: 'BUY',
      strength: 70,
      reason: `TSI oversold at ${currentTSI.toFixed(2)}`,
    };
  }

  return {
    signal: 'NEUTRAL',
    strength: 50,
    reason: `TSI neutral at ${currentTSI.toFixed(2)}`,
  };
}

/**
 * Analyzes Ultimate Oscillator for buy/sell signals
 */
export function analyzeUltimateOscillator(values: IndicatorData[]): SignalResult {
  if (values.length < 2) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'Insufficient data' };
  }

  const currentValue = values[values.length - 1]?.value;
  const previousValue = values[values.length - 2]?.value;

  if (currentValue === null || previousValue === null) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'No current values' };
  }

  // Oversold with bullish divergence
  if (currentValue < 30 && currentValue > previousValue) {
    return {
      signal: 'BUY',
      strength: 85,
      reason: `Ultimate Oscillator oversold reversal (${currentValue.toFixed(2)})`,
    };
  }

  // Overbought with bearish divergence
  if (currentValue > 70 && currentValue < previousValue) {
    return {
      signal: 'SELL',
      strength: 85,
      reason: `Ultimate Oscillator overbought reversal (${currentValue.toFixed(2)})`,
    };
  }

  // Oversold
  if (currentValue < 30) {
    return {
      signal: 'BUY',
      strength: 70,
      reason: `Ultimate Oscillator oversold at ${currentValue.toFixed(2)}`,
    };
  }

  // Overbought
  if (currentValue > 70) {
    return {
      signal: 'SELL',
      strength: 70,
      reason: `Ultimate Oscillator overbought at ${currentValue.toFixed(2)}`,
    };
  }

  return {
    signal: 'NEUTRAL',
    strength: 50,
    reason: `Ultimate Oscillator neutral at ${currentValue.toFixed(2)}`,
  };
}

/**
 * Aggregates signals from multiple indicators
 */
export function aggregateSignals(signals: SignalResult[]): SignalResult {
  if (signals.length === 0) {
    return { signal: 'NEUTRAL', strength: 0, reason: 'No indicators' };
  }

  const buySignals = signals.filter((s) => s.signal === 'BUY');
  const sellSignals = signals.filter((s) => s.signal === 'SELL');

  const buyStrength = buySignals.reduce((sum, s) => sum + s.strength, 0) / signals.length;
  const sellStrength = sellSignals.reduce((sum, s) => sum + s.strength, 0) / signals.length;

  if (buySignals.length > sellSignals.length && buyStrength > 60) {
    return {
      signal: 'BUY',
      strength: buyStrength,
      reason: `${buySignals.length}/${signals.length} indicators suggest BUY`,
    };
  }

  if (sellSignals.length > buySignals.length && sellStrength > 60) {
    return {
      signal: 'SELL',
      strength: sellStrength,
      reason: `${sellSignals.length}/${signals.length} indicators suggest SELL`,
    };
  }

  return {
    signal: 'NEUTRAL',
    strength: 50,
    reason: 'Mixed signals from indicators',
  };
}
