import { IndicatorDefinition } from '../types';

export const OPERATORS = [
  { value: '>', label: '>' },
  { value: '<', label: '<' },
  { value: '>=', label: '>=' },
  { value: '<=', label: '<=' },
  { value: '==', label: '==' },
  { value: 'crosses_above', label: 'Crosses Above' },
  { value: 'crosses_below', label: 'Crosses Below' },
  { value: 'between', label: 'Between' },
];

export const TIMEFRAMES = [
  { value: '1m', label: '1 Min' },
  { value: '3m', label: '3 Min' },
  { value: '5m', label: '5 Min' },
  { value: '10m', label: '10 Min' },
  { value: '15m', label: '15 Min' },
  { value: '30m', label: '30 Min' },
  { value: '1h', label: '1 Hour' },
  { value: '4h', label: '4 Hour' },
  { value: '1d', label: '1 Day' },
];

export const INDICATOR_DEFINITIONS: IndicatorDefinition[] = [
  // Momentum
  {
    name: 'RSI',
    label: 'RSI',
    category: 'momentum',
    fields: ['value'],
    defaultField: 'value',
    params: [{ key: 'period', label: 'Period', default: 14, min: 2, max: 100 }],
  },
  {
    name: 'STOCHASTIC',
    label: 'Stochastic',
    category: 'momentum',
    fields: ['k', 'd'],
    defaultField: 'k',
    params: [
      { key: 'period', label: 'Period', default: 14, min: 2, max: 100 },
      { key: 'smooth', label: 'Smooth', default: 3, min: 1, max: 20 },
    ],
  },
  {
    name: 'WILLIAMS_R',
    label: "Williams %R",
    category: 'momentum',
    fields: ['value'],
    defaultField: 'value',
    params: [{ key: 'period', label: 'Period', default: 14, min: 2, max: 100 }],
  },
  {
    name: 'ROC',
    label: 'ROC / Momentum',
    category: 'momentum',
    fields: ['value'],
    defaultField: 'value',
    params: [{ key: 'period', label: 'Period', default: 12, min: 1, max: 100 }],
  },

  // Trend
  {
    name: 'MACD',
    label: 'MACD',
    category: 'trend',
    fields: ['line', 'signal_line', 'histogram'],
    defaultField: 'histogram',
    params: [
      { key: 'fast', label: 'Fast', default: 12, min: 2, max: 50 },
      { key: 'slow', label: 'Slow', default: 26, min: 5, max: 100 },
      { key: 'signal', label: 'Signal', default: 9, min: 2, max: 50 },
    ],
  },
  {
    name: 'SMA',
    label: 'SMA',
    category: 'trend',
    fields: ['value'],
    defaultField: 'value',
    params: [{ key: 'period', label: 'Period', default: 20, min: 2, max: 500 }],
  },
  {
    name: 'EMA',
    label: 'EMA',
    category: 'trend',
    fields: ['value'],
    defaultField: 'value',
    params: [{ key: 'period', label: 'Period', default: 12, min: 2, max: 500 }],
  },
  {
    name: 'ADX',
    label: 'ADX',
    category: 'trend',
    fields: ['value', 'plus_di', 'minus_di'],
    defaultField: 'value',
    params: [{ key: 'period', label: 'Period', default: 14, min: 2, max: 100 }],
  },
  {
    name: 'CCI',
    label: 'CCI',
    category: 'trend',
    fields: ['value'],
    defaultField: 'value',
    params: [{ key: 'period', label: 'Period', default: 20, min: 2, max: 100 }],
  },
  {
    name: 'SUPERTREND',
    label: 'Supertrend',
    category: 'trend',
    fields: ['value', 'direction'],
    defaultField: 'direction',
    params: [
      { key: 'period', label: 'Period', default: 10, min: 2, max: 100 },
      { key: 'multiplier', label: 'Multiplier', default: 3, min: 1, max: 10 },
    ],
  },

  // Volatility
  {
    name: 'BOLLINGER',
    label: 'Bollinger Bands',
    category: 'volatility',
    fields: ['upper', 'middle', 'lower', 'pct_b'],
    defaultField: 'pct_b',
    params: [
      { key: 'period', label: 'Period', default: 20, min: 2, max: 100 },
      { key: 'std_dev', label: 'Std Dev', default: 2, min: 1, max: 5 },
    ],
  },
  {
    name: 'ATR',
    label: 'ATR',
    category: 'volatility',
    fields: ['value'],
    defaultField: 'value',
    params: [{ key: 'period', label: 'Period', default: 14, min: 2, max: 100 }],
  },
  {
    name: 'KELTNER',
    label: 'Keltner Channel',
    category: 'volatility',
    fields: ['upper', 'middle', 'lower'],
    defaultField: 'middle',
    params: [{ key: 'period', label: 'Period', default: 20, min: 2, max: 100 }],
  },
  {
    name: 'DONCHIAN',
    label: 'Donchian Channel',
    category: 'volatility',
    fields: ['upper', 'middle', 'lower'],
    defaultField: 'middle',
    params: [{ key: 'period', label: 'Period', default: 20, min: 2, max: 100 }],
  },

  // Volume
  {
    name: 'OBV',
    label: 'OBV',
    category: 'volume',
    fields: ['value'],
    defaultField: 'value',
    params: [],
  },
  {
    name: 'VWAP',
    label: 'VWAP',
    category: 'volume',
    fields: ['value'],
    defaultField: 'value',
    params: [],
  },

  // Price
  {
    name: 'PRICE',
    label: 'Price',
    category: 'price',
    fields: ['value'],
    defaultField: 'value',
    params: [],
  },
  {
    name: 'VOLUME',
    label: 'Volume',
    category: 'price',
    fields: ['value'],
    defaultField: 'value',
    params: [],
  },
  {
    name: 'CHANGE_PCT',
    label: 'Change %',
    category: 'price',
    fields: ['value'],
    defaultField: 'value',
    params: [],
  },
];

export function getIndicatorDef(name: string): IndicatorDefinition | undefined {
  return INDICATOR_DEFINITIONS.find(d => d.name === name);
}
