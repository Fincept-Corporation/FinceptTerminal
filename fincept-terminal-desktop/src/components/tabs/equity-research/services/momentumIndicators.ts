// Service for calculating and parsing momentum indicators

import { invoke } from '@tauri-apps/api/core';
import {
  MomentumIndicatorParams,
  MomentumIndicatorResult,
  IndicatorData,
  CandleData,
} from '../types';
import * as SignalAnalyzer from '../utils/signalAnalyzer';

/**
 * Fetches historical price data for a symbol
 */
export async function fetchHistoricalData(
  symbol: string,
  period: string = '1y',
  interval: string = '1d'
): Promise<CandleData[]> {
  try {
    const result = await invoke<string>('run_python_script', {
      scriptName: 'Analytics/equityInvestment/get_historical_data.py',
      args: [symbol, period, interval],
    });

    const data = JSON.parse(result);

    if (!data || !Array.isArray(data)) {
      throw new Error('Invalid historical data format');
    }

    return data.map((item: any) => ({
      time: new Date(item.Date || item.timestamp).getTime() / 1000, // Convert to Unix timestamp in seconds
      open: parseFloat(item.Open),
      high: parseFloat(item.High),
      low: parseFloat(item.Low),
      close: parseFloat(item.Close),
      volume: parseInt(item.Volume),
    }));
  } catch (error) {
    console.error('Error fetching historical data:', error);
    throw error;
  }
}

/**
 * Calculates RSI (Relative Strength Index)
 */
export async function calculateRSI(
  symbol: string,
  params: MomentumIndicatorParams['rsi'],
  period: string = '1y',
  interval: string = '1d'
): Promise<MomentumIndicatorResult['rsi']> {
  try {
    const result = await invoke<string>('run_python_script', {
      scriptName: 'Analytics/technical_analysis/momentum_indicators.py',
      args: [
        symbol,
        'rsi',
        JSON.stringify({
          period: params?.period || 14,
          timeframe: period,
          interval: interval,
        }),
      ],
    });

    const data = JSON.parse(result);

    const values: IndicatorData[] = data.values.map((item: any) => ({
      time: new Date(item.time).getTime() / 1000,
      value: item.value !== null ? parseFloat(item.value) : null,
    }));

    const currentValue = values[values.length - 1]?.value || 0;
    const signal = SignalAnalyzer.analyzeRSI(values, params!);

    return {
      values,
      signal: signal.signal,
      current_value: currentValue,
      params: params!,
    };
  } catch (error) {
    console.error('Error calculating RSI:', error);
    throw error;
  }
}

/**
 * Calculates MACD (Moving Average Convergence Divergence)
 */
export async function calculateMACD(
  symbol: string,
  params: MomentumIndicatorParams['macd'],
  period: string = '1y',
  interval: string = '1d'
): Promise<MomentumIndicatorResult['macd']> {
  try {
    const result = await invoke<string>('run_python_script', {
      scriptName: 'Analytics/technical_analysis/momentum_indicators.py',
      args: [
        symbol,
        'macd',
        JSON.stringify({
          fast_period: params?.fast_period || 12,
          slow_period: params?.slow_period || 26,
          signal_period: params?.signal_period || 9,
          timeframe: period,
          interval: interval,
        }),
      ],
    });

    const data = JSON.parse(result);

    const macdLine: IndicatorData[] = data.macd_line.map((item: any) => ({
      time: new Date(item.time).getTime() / 1000,
      value: item.value !== null ? parseFloat(item.value) : null,
    }));

    const signalLine: IndicatorData[] = data.signal_line.map((item: any) => ({
      time: new Date(item.time).getTime() / 1000,
      value: item.value !== null ? parseFloat(item.value) : null,
    }));

    const histogram: IndicatorData[] = data.histogram.map((item: any) => ({
      time: new Date(item.time).getTime() / 1000,
      value: item.value !== null ? parseFloat(item.value) : null,
    }));

    const currentMACD = macdLine[macdLine.length - 1]?.value || 0;
    const currentSignal = signalLine[signalLine.length - 1]?.value || 0;
    const currentHistogram = histogram[histogram.length - 1]?.value || 0;

    const signal = SignalAnalyzer.analyzeMACD(macdLine, signalLine, histogram);

    return {
      macd_line: macdLine,
      signal_line: signalLine,
      histogram,
      signal: signal.signal,
      current_macd: currentMACD,
      current_signal: currentSignal,
      current_histogram: currentHistogram,
      params: params!,
    };
  } catch (error) {
    console.error('Error calculating MACD:', error);
    throw error;
  }
}

/**
 * Calculates Stochastic Oscillator
 */
export async function calculateStochastic(
  symbol: string,
  params: MomentumIndicatorParams['stochastic'],
  period: string = '1y',
  interval: string = '1d'
): Promise<MomentumIndicatorResult['stochastic']> {
  try {
    const result = await invoke<string>('run_python_script', {
      scriptName: 'Analytics/technical_analysis/momentum_indicators.py',
      args: [
        symbol,
        'stochastic',
        JSON.stringify({
          k_period: params?.k_period || 14,
          d_period: params?.d_period || 3,
          timeframe: period,
          interval: interval,
        }),
      ],
    });

    const data = JSON.parse(result);

    const kValues: IndicatorData[] = data.k_values.map((item: any) => ({
      time: new Date(item.time).getTime() / 1000,
      value: item.value !== null ? parseFloat(item.value) : null,
    }));

    const dValues: IndicatorData[] = data.d_values.map((item: any) => ({
      time: new Date(item.time).getTime() / 1000,
      value: item.value !== null ? parseFloat(item.value) : null,
    }));

    const currentK = kValues[kValues.length - 1]?.value || 0;
    const currentD = dValues[dValues.length - 1]?.value || 0;

    const signal = SignalAnalyzer.analyzeStochastic(kValues, dValues, params!);

    return {
      k_values: kValues,
      d_values: dValues,
      signal: signal.signal,
      current_k: currentK,
      current_d: currentD,
      params: params!,
    };
  } catch (error) {
    console.error('Error calculating Stochastic:', error);
    throw error;
  }
}

/**
 * Calculates CCI (Commodity Channel Index)
 */
export async function calculateCCI(
  symbol: string,
  params: MomentumIndicatorParams['cci'],
  period: string = '1y',
  interval: string = '1d'
): Promise<MomentumIndicatorResult['cci']> {
  try {
    const result = await invoke<string>('run_python_script', {
      scriptName: 'Analytics/technical_analysis/momentum_indicators.py',
      args: [
        symbol,
        'cci',
        JSON.stringify({
          period: params?.period || 20,
          timeframe: period,
          interval: interval,
        }),
      ],
    });

    const data = JSON.parse(result);

    const values: IndicatorData[] = data.values.map((item: any) => ({
      time: new Date(item.time).getTime() / 1000,
      value: item.value !== null ? parseFloat(item.value) : null,
    }));

    const currentValue = values[values.length - 1]?.value || 0;
    const signal = SignalAnalyzer.analyzeCCI(values, params!);

    return {
      values,
      signal: signal.signal,
      current_value: currentValue,
      params: params!,
    };
  } catch (error) {
    console.error('Error calculating CCI:', error);
    throw error;
  }
}

/**
 * Calculates ROC (Rate of Change)
 */
export async function calculateROC(
  symbol: string,
  params: MomentumIndicatorParams['roc'],
  period: string = '1y',
  interval: string = '1d'
): Promise<MomentumIndicatorResult['roc']> {
  try {
    const result = await invoke<string>('run_python_script', {
      scriptName: 'Analytics/technical_analysis/momentum_indicators.py',
      args: [
        symbol,
        'roc',
        JSON.stringify({
          period: params?.period || 12,
          timeframe: period,
          interval: interval,
        }),
      ],
    });

    const data = JSON.parse(result);

    const values: IndicatorData[] = data.values.map((item: any) => ({
      time: new Date(item.time).getTime() / 1000,
      value: item.value !== null ? parseFloat(item.value) : null,
    }));

    const currentValue = values[values.length - 1]?.value || 0;
    const signal = SignalAnalyzer.analyzeROC(values);

    return {
      values,
      signal: signal.signal,
      current_value: currentValue,
      params: params!,
    };
  } catch (error) {
    console.error('Error calculating ROC:', error);
    throw error;
  }
}

/**
 * Calculates Williams %R
 */
export async function calculateWilliamsR(
  symbol: string,
  params: MomentumIndicatorParams['williams_r'],
  period: string = '1y',
  interval: string = '1d'
): Promise<MomentumIndicatorResult['williams_r']> {
  try {
    const result = await invoke<string>('run_python_script', {
      scriptName: 'Analytics/technical_analysis/momentum_indicators.py',
      args: [
        symbol,
        'williams_r',
        JSON.stringify({
          period: params?.period || 14,
          timeframe: period,
          interval: interval,
        }),
      ],
    });

    const data = JSON.parse(result);

    const values: IndicatorData[] = data.values.map((item: any) => ({
      time: new Date(item.time).getTime() / 1000,
      value: item.value !== null ? parseFloat(item.value) : null,
    }));

    const currentValue = values[values.length - 1]?.value || 0;
    const signal = SignalAnalyzer.analyzeWilliamsR(values, params!);

    return {
      values,
      signal: signal.signal,
      current_value: currentValue,
      params: params!,
    };
  } catch (error) {
    console.error('Error calculating Williams %R:', error);
    throw error;
  }
}

/**
 * Calculates Awesome Oscillator
 */
export async function calculateAwesomeOscillator(
  symbol: string,
  params: MomentumIndicatorParams['awesome_oscillator'],
  period: string = '1y',
  interval: string = '1d'
): Promise<MomentumIndicatorResult['awesome_oscillator']> {
  try {
    const result = await invoke<string>('run_python_script', {
      scriptName: 'Analytics/technical_analysis/momentum_indicators.py',
      args: [
        symbol,
        'awesome_oscillator',
        JSON.stringify({
          fast_period: params?.fast_period || 5,
          slow_period: params?.slow_period || 34,
          timeframe: period,
          interval: interval,
        }),
      ],
    });

    const data = JSON.parse(result);

    const values: IndicatorData[] = data.values.map((item: any) => ({
      time: new Date(item.time).getTime() / 1000,
      value: item.value !== null ? parseFloat(item.value) : null,
    }));

    const currentValue = values[values.length - 1]?.value || 0;
    const signal = SignalAnalyzer.analyzeAwesomeOscillator(values);

    return {
      values,
      signal: signal.signal,
      current_value: currentValue,
      params: params!,
    };
  } catch (error) {
    console.error('Error calculating Awesome Oscillator:', error);
    throw error;
  }
}

/**
 * Calculates TSI (True Strength Index)
 */
export async function calculateTSI(
  symbol: string,
  params: MomentumIndicatorParams['tsi'],
  period: string = '1y',
  interval: string = '1d'
): Promise<MomentumIndicatorResult['tsi']> {
  try {
    const result = await invoke<string>('run_python_script', {
      scriptName: 'Analytics/technical_analysis/momentum_indicators.py',
      args: [
        symbol,
        'tsi',
        JSON.stringify({
          long_period: params?.long_period || 25,
          short_period: params?.short_period || 13,
          signal_period: params?.signal_period || 13,
          timeframe: period,
          interval: interval,
        }),
      ],
    });

    const data = JSON.parse(result);

    const values: IndicatorData[] = data.values.map((item: any) => ({
      time: new Date(item.time).getTime() / 1000,
      value: item.value !== null ? parseFloat(item.value) : null,
    }));

    const signalLine: IndicatorData[] = data.signal_line.map((item: any) => ({
      time: new Date(item.time).getTime() / 1000,
      value: item.value !== null ? parseFloat(item.value) : null,
    }));

    const currentValue = values[values.length - 1]?.value || 0;
    const currentSignal = signalLine[signalLine.length - 1]?.value || 0;

    const signal = SignalAnalyzer.analyzeTSI(values, signalLine);

    return {
      values,
      signal_line: signalLine,
      signal: signal.signal,
      current_value: currentValue,
      current_signal: currentSignal,
      params: params!,
    };
  } catch (error) {
    console.error('Error calculating TSI:', error);
    throw error;
  }
}

/**
 * Calculates Ultimate Oscillator
 */
export async function calculateUltimateOscillator(
  symbol: string,
  params: MomentumIndicatorParams['ultimate_oscillator'],
  period: string = '1y',
  interval: string = '1d'
): Promise<MomentumIndicatorResult['ultimate_oscillator']> {
  try {
    const result = await invoke<string>('run_python_script', {
      scriptName: 'Analytics/technical_analysis/momentum_indicators.py',
      args: [
        symbol,
        'ultimate_oscillator',
        JSON.stringify({
          period1: params?.period1 || 7,
          period2: params?.period2 || 14,
          period3: params?.period3 || 28,
          weight1: params?.weight1 || 4,
          weight2: params?.weight2 || 2,
          weight3: params?.weight3 || 1,
          timeframe: period,
          interval: interval,
        }),
      ],
    });

    const data = JSON.parse(result);

    const values: IndicatorData[] = data.values.map((item: any) => ({
      time: new Date(item.time).getTime() / 1000,
      value: item.value !== null ? parseFloat(item.value) : null,
    }));

    const currentValue = values[values.length - 1]?.value || 0;
    const signal = SignalAnalyzer.analyzeUltimateOscillator(values);

    return {
      values,
      signal: signal.signal,
      current_value: currentValue,
      params: params!,
    };
  } catch (error) {
    console.error('Error calculating Ultimate Oscillator:', error);
    throw error;
  }
}

/**
 * Calculates all momentum indicators
 */
export async function calculateAllMomentumIndicators(
  symbol: string,
  params: MomentumIndicatorParams,
  period: string = '1y',
  interval: string = '1d'
): Promise<MomentumIndicatorResult> {
  const results: MomentumIndicatorResult = {};

  try {
    if (params.rsi) {
      results.rsi = await calculateRSI(symbol, params.rsi, period, interval);
    }
    if (params.macd) {
      results.macd = await calculateMACD(symbol, params.macd, period, interval);
    }
    if (params.stochastic) {
      results.stochastic = await calculateStochastic(symbol, params.stochastic, period, interval);
    }
    if (params.cci) {
      results.cci = await calculateCCI(symbol, params.cci, period, interval);
    }
    if (params.roc) {
      results.roc = await calculateROC(symbol, params.roc, period, interval);
    }
    if (params.williams_r) {
      results.williams_r = await calculateWilliamsR(symbol, params.williams_r, period, interval);
    }
    if (params.awesome_oscillator) {
      results.awesome_oscillator = await calculateAwesomeOscillator(
        symbol,
        params.awesome_oscillator,
        period,
        interval
      );
    }
    if (params.tsi) {
      results.tsi = await calculateTSI(symbol, params.tsi, period, interval);
    }
    if (params.ultimate_oscillator) {
      results.ultimate_oscillator = await calculateUltimateOscillator(
        symbol,
        params.ultimate_oscillator,
        period,
        interval
      );
    }

    return results;
  } catch (error) {
    console.error('Error calculating momentum indicators:', error);
    throw error;
  }
}
