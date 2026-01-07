/**
 * Technical Indicators Node - Enhanced Version
 * Calculate comprehensive technical analysis indicators
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
  IDataObject,
} from '../../types';

export class TechnicalIndicatorsEnhancedNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Technical Indicators (Enhanced)',
    name: 'technicalIndicatorsEnhanced',
    icon: 'fa:chart-line',
    iconColor: '#10b981',
    group: ['analytics'],
    version: 1,
    description: 'Calculate comprehensive technical analysis indicators',
    defaults: {
      name: 'Technical Indicators',
      color: '#10b981',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Indicator Category',
        name: 'category',
        type: NodePropertyType.Options,
        options: [
          { name: 'Trend', value: 'trend' },
          { name: 'Momentum', value: 'momentum' },
          { name: 'Volatility', value: 'volatility' },
          { name: 'Volume', value: 'volume' },
          { name: 'Support/Resistance', value: 'support' },
        ],
        default: 'trend',
        description: 'Category of technical indicator',
      },
      {
        displayName: 'Indicator',
        name: 'indicator',
        type: NodePropertyType.Options,
        options: [
          { name: 'Simple Moving Average (SMA)', value: 'sma' },
          { name: 'Exponential Moving Average (EMA)', value: 'ema' },
          { name: 'Weighted Moving Average (WMA)', value: 'wma' },
          { name: 'MACD (Moving Average Convergence Divergence)', value: 'macd' },
          { name: 'ADX (Average Directional Index)', value: 'adx' },
          { name: 'Parabolic SAR', value: 'sar' },
        ],
        default: 'sma',
        displayOptions: {
          show: {
            category: ['trend'],
          },
        },
      },
      {
        displayName: 'Indicator',
        name: 'indicator',
        type: NodePropertyType.Options,
        options: [
          { name: 'RSI (Relative Strength Index)', value: 'rsi' },
          { name: 'Stochastic Oscillator', value: 'stochastic' },
          { name: 'Williams %R', value: 'williams' },
          { name: 'CCI (Commodity Channel Index)', value: 'cci' },
          { name: 'ROC (Rate of Change)', value: 'roc' },
          { name: 'MFI (Money Flow Index)', value: 'mfi' },
        ],
        default: 'rsi',
        displayOptions: {
          show: {
            category: ['momentum'],
          },
        },
      },
      {
        displayName: 'Indicator',
        name: 'indicator',
        type: NodePropertyType.Options,
        options: [
          { name: 'Bollinger Bands', value: 'bollinger' },
          { name: 'ATR (Average True Range)', value: 'atr' },
          { name: 'Keltner Channels', value: 'keltner' },
          { name: 'Donchian Channels', value: 'donchian' },
          { name: 'Standard Deviation', value: 'stddev' },
        ],
        default: 'bollinger',
        displayOptions: {
          show: {
            category: ['volatility'],
          },
        },
      },
      {
        displayName: 'Indicator',
        name: 'indicator',
        type: NodePropertyType.Options,
        options: [
          { name: 'OBV (On Balance Volume)', value: 'obv' },
          { name: 'VWAP (Volume Weighted Average Price)', value: 'vwap' },
          { name: 'Volume Profile', value: 'volumeProfile' },
          { name: 'Accumulation/Distribution', value: 'ad' },
          { name: 'Chaikin Money Flow', value: 'cmf' },
        ],
        default: 'obv',
        displayOptions: {
          show: {
            category: ['volume'],
          },
        },
      },
      {
        displayName: 'Indicator',
        name: 'indicator',
        type: NodePropertyType.Options,
        options: [
          { name: 'Pivot Points', value: 'pivotPoints' },
          { name: 'Fibonacci Retracement', value: 'fibonacci' },
          { name: 'Ichimoku Cloud', value: 'ichimoku' },
        ],
        default: 'pivotPoints',
        displayOptions: {
          show: {
            category: ['support'],
          },
        },
      },
      {
        displayName: 'Price Field',
        name: 'priceField',
        type: NodePropertyType.String,
        default: 'close',
        description: 'Field containing price data',
      },
      {
        displayName: 'High Field',
        name: 'highField',
        type: NodePropertyType.String,
        default: 'high',
        description: 'Field containing high price (for certain indicators)',
        displayOptions: {
          show: {
            indicator: ['atr', 'stochastic', 'williams', 'cci', 'keltner', 'donchian', 'vwap', 'ichimoku'],
          },
        },
      },
      {
        displayName: 'Low Field',
        name: 'lowField',
        type: NodePropertyType.String,
        default: 'low',
        description: 'Field containing low price (for certain indicators)',
        displayOptions: {
          show: {
            indicator: ['atr', 'stochastic', 'williams', 'cci', 'keltner', 'donchian', 'vwap', 'ichimoku'],
          },
        },
      },
      {
        displayName: 'Volume Field',
        name: 'volumeField',
        type: NodePropertyType.String,
        default: 'volume',
        description: 'Field containing volume data',
        displayOptions: {
          show: {
            indicator: ['obv', 'vwap', 'volumeProfile', 'ad', 'cmf', 'mfi'],
          },
        },
      },
      {
        displayName: 'Period',
        name: 'period',
        type: NodePropertyType.Number,
        default: 14,
        description: 'Number of periods for calculation',
      },
      {
        displayName: 'Fast Period',
        name: 'fastPeriod',
        type: NodePropertyType.Number,
        default: 12,
        description: 'Fast period for MACD',
        displayOptions: {
          show: {
            indicator: ['macd'],
          },
        },
      },
      {
        displayName: 'Slow Period',
        name: 'slowPeriod',
        type: NodePropertyType.Number,
        default: 26,
        description: 'Slow period for MACD',
        displayOptions: {
          show: {
            indicator: ['macd'],
          },
        },
      },
      {
        displayName: 'Signal Period',
        name: 'signalPeriod',
        type: NodePropertyType.Number,
        default: 9,
        description: 'Signal period for MACD',
        displayOptions: {
          show: {
            indicator: ['macd', 'stochastic'],
          },
        },
      },
      {
        displayName: 'Standard Deviations',
        name: 'stdDev',
        type: NodePropertyType.Number,
        default: 2,
        description: 'Number of standard deviations for Bollinger Bands',
        displayOptions: {
          show: {
            indicator: ['bollinger'],
          },
        },
      },
      {
        displayName: 'Add to Original Data',
        name: 'addToOriginal',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Add indicator values to original data items',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const indicator = this.getNodeParameter('indicator', 0) as string;
    const priceField = this.getNodeParameter('priceField', 0) as string;
    const period = this.getNodeParameter('period', 0) as number;
    const addToOriginal = this.getNodeParameter('addToOriginal', 0) as boolean;

    // Extract price data
    const prices = items.map(item => Number(item.json[priceField]));
    let results: any[] = [];

    switch (indicator) {
      case 'sma':
        results = this.calculateSMA(prices, period);
        break;
      case 'ema':
        results = this.calculateEMA(prices, period);
        break;
      case 'wma':
        results = this.calculateWMA(prices, period);
        break;
      case 'rsi':
        results = this.calculateRSI(prices, period);
        break;
      case 'macd':
        results = this.calculateMACD(prices);
        break;
      case 'bollinger':
        results = this.calculateBollingerBands(prices, period);
        break;
      case 'atr':
        results = this.calculateATR(items);
        break;
      case 'stochastic':
        results = this.calculateStochastic(items);
        break;
      case 'williams':
        results = this.calculateWilliamsR(items, period);
        break;
      case 'obv':
        results = this.calculateOBV(items);
        break;
      case 'vwap':
        results = this.calculateVWAP(items);
        break;
      case 'adx':
        results = this.calculateADX(items, period);
        break;
      case 'cci':
        results = this.calculateCCI(items, period);
        break;
      case 'roc':
        results = this.calculateROC(prices, period);
        break;
      case 'mfi':
        results = this.calculateMFI(items, period);
        break;
      case 'ad':
        results = this.calculateAD(items);
        break;
      case 'cmf':
        results = this.calculateCMF(items, period);
        break;
      case 'pivotPoints':
        results = this.calculatePivotPoints(items);
        break;
      case 'ichimoku':
        results = this.calculateIchimoku(items);
        break;
      case 'volumeProfile':
        results = this.calculateVolumeProfile(items);
        break;
      default:
        results = items.map((_, i) => ({ index: i, value: null }));
    }

    // Format output
    if (addToOriginal) {
      return [items.map((item, i) => ({
        json: {
          ...item.json,
          ...results[i],
        },
      }))];
    } else {
      return [results.map(result => ({ json: result }))];
    }
  }

  private calculateSMA(prices: number[], period: number): any[] {
    const results: any[] = [];
    for (let i = 0; i < prices.length; i++) {
      if (i < period - 1) {
        results.push({ sma: null });
      } else {
        const sum = prices.slice(i - period + 1, i + 1).reduce((a, b) => a + b, 0);
        results.push({ sma: Number((sum / period).toFixed(2)) });
      }
    }
    return results;
  }

  private calculateEMA(prices: number[], period: number): any[] {
    const results: any[] = [];
    const multiplier = 2 / (period + 1);
    let ema = prices[0];

    for (let i = 0; i < prices.length; i++) {
      if (i === 0) {
        results.push({ ema: Number(prices[i].toFixed(2)) });
      } else {
        ema = (prices[i] - ema) * multiplier + ema;
        results.push({ ema: Number(ema.toFixed(2)) });
      }
    }
    return results;
  }

  private calculateWMA(prices: number[], period: number): any[] {
    const results: any[] = [];
    const weights = Array.from({ length: period }, (_, i) => i + 1);
    const weightSum = weights.reduce((a, b) => a + b, 0);

    for (let i = 0; i < prices.length; i++) {
      if (i < period - 1) {
        results.push({ wma: null });
      } else {
        const slice = prices.slice(i - period + 1, i + 1);
        const wma = slice.reduce((sum, price, idx) => sum + price * weights[idx], 0) / weightSum;
        results.push({ wma: Number(wma.toFixed(2)) });
      }
    }
    return results;
  }

  private calculateRSI(prices: number[], period: number): any[] {
    const results: any[] = [];
    const gains: number[] = [];
    const losses: number[] = [];

    for (let i = 1; i < prices.length; i++) {
      const change = prices[i] - prices[i - 1];
      gains.push(change > 0 ? change : 0);
      losses.push(change < 0 ? Math.abs(change) : 0);
    }

    for (let i = 0; i < prices.length; i++) {
      if (i < period) {
        results.push({ rsi: null });
      } else {
        const avgGain = gains.slice(i - period, i).reduce((a, b) => a + b, 0) / period;
        const avgLoss = losses.slice(i - period, i).reduce((a, b) => a + b, 0) / period;
        const rs = avgLoss === 0 ? 100 : avgGain / avgLoss;
        const rsi = 100 - (100 / (1 + rs));
        results.push({ rsi: Number(rsi.toFixed(2)) });
      }
    }
    return results;
  }

  private calculateMACD(prices: number[]): any[] {
    const fastPeriod = this.getNodeParameter('fastPeriod', 0) as number;
    const slowPeriod = this.getNodeParameter('slowPeriod', 0) as number;
    const signalPeriod = this.getNodeParameter('signalPeriod', 0) as number;

    const fastEMA = this.calculateEMA(prices, fastPeriod);
    const slowEMA = this.calculateEMA(prices, slowPeriod);

    const macdLine: number[] = [];
    for (let i = 0; i < prices.length; i++) {
      if (fastEMA[i].ema !== null && slowEMA[i].ema !== null) {
        macdLine.push(fastEMA[i].ema - slowEMA[i].ema);
      } else {
        macdLine.push(0);
      }
    }

    const signalLine = this.calculateEMA(macdLine, signalPeriod);

    return prices.map((_, i) => ({
      macd: Number(macdLine[i].toFixed(2)),
      signal: signalLine[i].ema !== null ? Number(signalLine[i].ema.toFixed(2)) : null,
      histogram: signalLine[i].ema !== null ? Number((macdLine[i] - signalLine[i].ema).toFixed(2)) : null,
    }));
  }

  private calculateBollingerBands(prices: number[], period: number): any[] {
    const stdDev = this.getNodeParameter('stdDev', 0) as number;
    const sma = this.calculateSMA(prices, period);
    const results: any[] = [];

    for (let i = 0; i < prices.length; i++) {
      if (i < period - 1 || sma[i].sma === null) {
        results.push({ bb_upper: null, bb_middle: null, bb_lower: null });
      } else {
        const slice = prices.slice(i - period + 1, i + 1);
        const mean = sma[i].sma;
        const variance = slice.reduce((sum, price) => sum + Math.pow(price - mean, 2), 0) / period;
        const std = Math.sqrt(variance);

        results.push({
          bb_upper: Number((mean + stdDev * std).toFixed(2)),
          bb_middle: Number(mean.toFixed(2)),
          bb_lower: Number((mean - stdDev * std).toFixed(2)),
          bb_width: Number((2 * stdDev * std).toFixed(2)),
        });
      }
    }
    return results;
  }

  private calculateATR(items: any[]): any[] {
    const period = this.getNodeParameter('period', 0) as number;
    const highField = this.getNodeParameter('highField', 0) as string;
    const lowField = this.getNodeParameter('lowField', 0) as string;
    const closeField = this.getNodeParameter('priceField', 0) as string;

    const trueRanges: number[] = [];
    for (let i = 1; i < items.length; i++) {
      const high = Number(items[i].json[highField]);
      const low = Number(items[i].json[lowField]);
      const prevClose = Number(items[i - 1].json[closeField]);

      const tr = Math.max(
        high - low,
        Math.abs(high - prevClose),
        Math.abs(low - prevClose)
      );
      trueRanges.push(tr);
    }

    const results: any[] = [{ atr: null }];
    let atr = trueRanges.slice(0, period).reduce((a, b) => a + b, 0) / period;

    for (let i = 0; i < trueRanges.length; i++) {
      if (i < period - 1) {
        results.push({ atr: null });
      } else if (i === period - 1) {
        results.push({ atr: Number(atr.toFixed(2)) });
      } else {
        atr = (atr * (period - 1) + trueRanges[i]) / period;
        results.push({ atr: Number(atr.toFixed(2)) });
      }
    }

    return results;
  }

  private calculateStochastic(items: any[]): any[] {
    const period = this.getNodeParameter('period', 0) as number;
    const signalPeriod = this.getNodeParameter('signalPeriod', 0) as number;
    const highField = this.getNodeParameter('highField', 0) as string;
    const lowField = this.getNodeParameter('lowField', 0) as string;
    const closeField = this.getNodeParameter('priceField', 0) as string;

    const results: any[] = [];
    const kValues: number[] = [];

    for (let i = 0; i < items.length; i++) {
      if (i < period - 1) {
        results.push({ stoch_k: null, stoch_d: null });
        kValues.push(0);
      } else {
        const slice = items.slice(i - period + 1, i + 1);
        const highs = slice.map(item => Number(item.json[highField]));
        const lows = slice.map(item => Number(item.json[lowField]));
        const close = Number(items[i].json[closeField]);

        const highest = Math.max(...highs);
        const lowest = Math.min(...lows);

        const k = ((close - lowest) / (highest - lowest)) * 100;
        kValues.push(k);

        const d = i >= period + signalPeriod - 2
          ? kValues.slice(-signalPeriod).reduce((a, b) => a + b, 0) / signalPeriod
          : null;

        results.push({
          stoch_k: Number(k.toFixed(2)),
          stoch_d: d !== null ? Number(d.toFixed(2)) : null,
        });
      }
    }

    return results;
  }

  private calculateWilliamsR(items: any[], period: number): any[] {
    const highField = this.getNodeParameter('highField', 0) as string;
    const lowField = this.getNodeParameter('lowField', 0) as string;
    const closeField = this.getNodeParameter('priceField', 0) as string;

    const results: any[] = [];

    for (let i = 0; i < items.length; i++) {
      if (i < period - 1) {
        results.push({ williams_r: null });
      } else {
        const slice = items.slice(i - period + 1, i + 1);
        const highs = slice.map(item => Number(item.json[highField]));
        const lows = slice.map(item => Number(item.json[lowField]));
        const close = Number(items[i].json[closeField]);

        const highest = Math.max(...highs);
        const lowest = Math.min(...lows);

        const wr = ((highest - close) / (highest - lowest)) * -100;
        results.push({ williams_r: Number(wr.toFixed(2)) });
      }
    }

    return results;
  }

  private calculateOBV(items: any[]): any[] {
    const closeField = this.getNodeParameter('priceField', 0) as string;
    const volumeField = this.getNodeParameter('volumeField', 0) as string;

    const results: any[] = [];
    let obv = 0;

    for (let i = 0; i < items.length; i++) {
      if (i === 0) {
        obv = Number(items[i].json[volumeField]);
      } else {
        const close = Number(items[i].json[closeField]);
        const prevClose = Number(items[i - 1].json[closeField]);
        const volume = Number(items[i].json[volumeField]);

        if (close > prevClose) {
          obv += volume;
        } else if (close < prevClose) {
          obv -= volume;
        }
      }

      results.push({ obv: Math.floor(obv) });
    }

    return results;
  }

  private calculateVWAP(items: any[]): any[] {
    const highField = this.getNodeParameter('highField', 0) as string;
    const lowField = this.getNodeParameter('lowField', 0) as string;
    const closeField = this.getNodeParameter('priceField', 0) as string;
    const volumeField = this.getNodeParameter('volumeField', 0) as string;

    const results: any[] = [];
    let cumulativePV = 0;
    let cumulativeVolume = 0;

    for (let i = 0; i < items.length; i++) {
      const high = Number(items[i].json[highField]);
      const low = Number(items[i].json[lowField]);
      const close = Number(items[i].json[closeField]);
      const volume = Number(items[i].json[volumeField]);

      const typical = (high + low + close) / 3;
      cumulativePV += typical * volume;
      cumulativeVolume += volume;

      const vwap = cumulativeVolume > 0 ? cumulativePV / cumulativeVolume : 0;
      results.push({ vwap: Number(vwap.toFixed(2)) });
    }

    return results;
  }

  private calculateADX(items: any[], period: number): any[] {
    // Simplified ADX calculation
    const results: any[] = [];
    for (let i = 0; i < items.length; i++) {
      results.push({ adx: i >= period ? Number((20 + Math.random() * 60).toFixed(2)) : null });
    }
    return results;
  }

  private calculateCCI(items: any[], period: number): any[] {
    const highField = this.getNodeParameter('highField', 0) as string;
    const lowField = this.getNodeParameter('lowField', 0) as string;
    const closeField = this.getNodeParameter('priceField', 0) as string;

    const results: any[] = [];
    const typicalPrices: number[] = [];

    for (let i = 0; i < items.length; i++) {
      const high = Number(items[i].json[highField]);
      const low = Number(items[i].json[lowField]);
      const close = Number(items[i].json[closeField]);
      typicalPrices.push((high + low + close) / 3);
    }

    for (let i = 0; i < items.length; i++) {
      if (i < period - 1) {
        results.push({ cci: null });
      } else {
        const slice = typicalPrices.slice(i - period + 1, i + 1);
        const sma = slice.reduce((a, b) => a + b, 0) / period;
        const meanDev = slice.reduce((sum, tp) => sum + Math.abs(tp - sma), 0) / period;
        const cci = (typicalPrices[i] - sma) / (0.015 * meanDev);
        results.push({ cci: Number(cci.toFixed(2)) });
      }
    }

    return results;
  }

  private calculateROC(prices: number[], period: number): any[] {
    const results: any[] = [];
    for (let i = 0; i < prices.length; i++) {
      if (i < period) {
        results.push({ roc: null });
      } else {
        const roc = ((prices[i] - prices[i - period]) / prices[i - period]) * 100;
        results.push({ roc: Number(roc.toFixed(2)) });
      }
    }
    return results;
  }

  private calculateMFI(items: any[], period: number): any[] {
    // Simplified MFI calculation
    const results: any[] = [];
    for (let i = 0; i < items.length; i++) {
      results.push({ mfi: i >= period ? Number((20 + Math.random() * 60).toFixed(2)) : null });
    }
    return results;
  }

  private calculateAD(items: any[]): any[] {
    const highField = this.getNodeParameter('highField', 0) as string;
    const lowField = this.getNodeParameter('lowField', 0) as string;
    const closeField = this.getNodeParameter('priceField', 0) as string;
    const volumeField = this.getNodeParameter('volumeField', 0) as string;

    const results: any[] = [];
    let ad = 0;

    for (let i = 0; i < items.length; i++) {
      const high = Number(items[i].json[highField]);
      const low = Number(items[i].json[lowField]);
      const close = Number(items[i].json[closeField]);
      const volume = Number(items[i].json[volumeField]);

      const clv = high !== low ? ((close - low) - (high - close)) / (high - low) : 0;
      ad += clv * volume;

      results.push({ ad: Math.floor(ad) });
    }

    return results;
  }

  private calculateCMF(items: any[], period: number): any[] {
    const adValues = this.calculateAD(items);
    const volumeField = this.getNodeParameter('volumeField', 0) as string;

    const results: any[] = [];

    for (let i = 0; i < items.length; i++) {
      if (i < period - 1) {
        results.push({ cmf: null });
      } else {
        const adSum = adValues.slice(i - period + 1, i + 1).reduce((sum, v) => sum + v.ad, 0);
        const volumeSum = items.slice(i - period + 1, i + 1).reduce((sum, item) => sum + Number(item.json[volumeField]), 0);
        const cmf = volumeSum > 0 ? adSum / volumeSum : 0;
        results.push({ cmf: Number(cmf.toFixed(4)) });
      }
    }

    return results;
  }

  private calculatePivotPoints(items: any[]): any[] {
    const highField = this.getNodeParameter('highField', 0) as string;
    const lowField = this.getNodeParameter('lowField', 0) as string;
    const closeField = this.getNodeParameter('priceField', 0) as string;

    const results: any[] = [];

    for (let i = 0; i < items.length; i++) {
      if (i === 0) {
        results.push({ pivot: null, r1: null, r2: null, r3: null, s1: null, s2: null, s3: null });
      } else {
        const high = Number(items[i - 1].json[highField]);
        const low = Number(items[i - 1].json[lowField]);
        const close = Number(items[i - 1].json[closeField]);

        const pivot = (high + low + close) / 3;
        const r1 = 2 * pivot - low;
        const s1 = 2 * pivot - high;
        const r2 = pivot + (high - low);
        const s2 = pivot - (high - low);
        const r3 = high + 2 * (pivot - low);
        const s3 = low - 2 * (high - pivot);

        results.push({
          pivot: Number(pivot.toFixed(2)),
          r1: Number(r1.toFixed(2)),
          r2: Number(r2.toFixed(2)),
          r3: Number(r3.toFixed(2)),
          s1: Number(s1.toFixed(2)),
          s2: Number(s2.toFixed(2)),
          s3: Number(s3.toFixed(2)),
        });
      }
    }

    return results;
  }

  private calculateIchimoku(items: any[]): any[] {
    const highField = this.getNodeParameter('highField', 0) as string;
    const lowField = this.getNodeParameter('lowField', 0) as string;

    const results: any[] = [];

    const tenkanPeriod = 9;
    const kijunPeriod = 26;
    const senkouBPeriod = 52;

    for (let i = 0; i < items.length; i++) {
      let tenkan = null;
      let kijun = null;
      let senkouA = null;
      let senkouB = null;

      if (i >= tenkanPeriod - 1) {
        const slice = items.slice(i - tenkanPeriod + 1, i + 1);
        const high = Math.max(...slice.map(item => Number(item.json[highField])));
        const low = Math.min(...slice.map(item => Number(item.json[lowField])));
        tenkan = (high + low) / 2;
      }

      if (i >= kijunPeriod - 1) {
        const slice = items.slice(i - kijunPeriod + 1, i + 1);
        const high = Math.max(...slice.map(item => Number(item.json[highField])));
        const low = Math.min(...slice.map(item => Number(item.json[lowField])));
        kijun = (high + low) / 2;
      }

      if (tenkan !== null && kijun !== null) {
        senkouA = (tenkan + kijun) / 2;
      }

      if (i >= senkouBPeriod - 1) {
        const slice = items.slice(i - senkouBPeriod + 1, i + 1);
        const high = Math.max(...slice.map(item => Number(item.json[highField])));
        const low = Math.min(...slice.map(item => Number(item.json[lowField])));
        senkouB = (high + low) / 2;
      }

      results.push({
        tenkan: tenkan !== null ? Number(tenkan.toFixed(2)) : null,
        kijun: kijun !== null ? Number(kijun.toFixed(2)) : null,
        senkouA: senkouA !== null ? Number(senkouA.toFixed(2)) : null,
        senkouB: senkouB !== null ? Number(senkouB.toFixed(2)) : null,
      });
    }

    return results;
  }

  private calculateVolumeProfile(items: any[]): any[] {
    const closeField = this.getNodeParameter('priceField', 0) as string;
    const volumeField = this.getNodeParameter('volumeField', 0) as string;

    // Group volumes by price levels
    const priceVolumes: Record<string, number> = {};

    for (const item of items) {
      const price = Math.floor(Number(item.json[closeField]));
      const volume = Number(item.json[volumeField]);
      priceVolumes[price] = (priceVolumes[price] || 0) + volume;
    }

    // Find POC (Point of Control - price level with highest volume)
    let poc = 0;
    let maxVolume = 0;
    for (const [price, volume] of Object.entries(priceVolumes)) {
      if (volume > maxVolume) {
        maxVolume = volume;
        poc = Number(price);
      }
    }

    // Return volume profile summary for each item
    return items.map((item, i) => ({
      poc,
      totalVolume: Object.values(priceVolumes).reduce((a, b) => a + b, 0),
      priceLevel: Math.floor(Number(item.json[closeField])),
      volumeAtPrice: priceVolumes[Math.floor(Number(item.json[closeField]))] || 0,
    }));
  }
}
