/**
 * Technical Indicators Node
 * Calculate technical analysis indicators (SMA, EMA, RSI, MACD, etc.)
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

export class TechnicalIndicatorsNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Technical Indicators',
    name: 'technicalIndicators',
    icon: 'fa:chart-line',
    iconColor: '#10b981',
    group: ['analytics'],
    version: 1,
    description: 'Calculate technical analysis indicators',
    defaults: {
      name: 'Technical Indicators',
      color: '#10b981',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Indicator',
        name: 'indicator',
        type: NodePropertyType.Options,
        options: [
          { name: 'Simple Moving Average (SMA)', value: 'sma' },
          { name: 'Exponential Moving Average (EMA)', value: 'ema' },
          { name: 'Relative Strength Index (RSI)', value: 'rsi' },
          { name: 'MACD', value: 'macd' },
          { name: 'Bollinger Bands', value: 'bollinger' },
          { name: 'Average True Range (ATR)', value: 'atr' },
        ],
        default: 'sma',
      },
      {
        displayName: 'Price Field',
        name: 'priceField',
        type: NodePropertyType.String,
        default: 'close',
        description: 'Field containing price data',
      },
      {
        displayName: 'Period',
        name: 'period',
        type: NodePropertyType.Number,
        default: 14,
        description: 'Number of periods for calculation',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const indicator = this.getNodeParameter('indicator', 0) as string;
    const priceField = this.getNodeParameter('priceField', 0) as string;
    const period = this.getNodeParameter('period', 0) as number;

    const prices = items.map(item => Number(item.json[priceField]));
    let results: IDataObject[] = [];

    switch (indicator) {
      case 'sma':
        results = calculateSMA(prices, period);
        break;
      case 'ema':
        results = calculateEMA(prices, period);
        break;
      case 'rsi':
        results = calculateRSI(prices, period);
        break;
      default:
        results = items.map((item, i) => ({ ...item.json, indicator: null }));
    }

    return [this.helpers.returnJsonArray(results)];
  }
}

function calculateSMA(prices: number[], period: number): IDataObject[] {
  const results: IDataObject[] = [];
  for (let i = 0; i < prices.length; i++) {
    if (i < period - 1) {
      results.push({ index: i, sma: null });
    } else {
      const sum = prices.slice(i - period + 1, i + 1).reduce((a, b) => a + b, 0);
      results.push({ index: i, sma: sum / period });
    }
  }
  return results;
}

function calculateEMA(prices: number[], period: number): IDataObject[] {
  const results: IDataObject[] = [];
  const multiplier = 2 / (period + 1);
  let ema = prices[0];

  for (let i = 0; i < prices.length; i++) {
    if (i === 0) {
      results.push({ index: i, ema: prices[i] });
    } else {
      ema = (prices[i] - ema) * multiplier + ema;
      results.push({ index: i, ema });
    }
  }
  return results;
}

function calculateRSI(prices: number[], period: number): IDataObject[] {
  const results: IDataObject[] = [];
  const gains: number[] = [];
  const losses: number[] = [];

  for (let i = 1; i < prices.length; i++) {
    const change = prices[i] - prices[i - 1];
    gains.push(change > 0 ? change : 0);
    losses.push(change < 0 ? Math.abs(change) : 0);
  }

  for (let i = 0; i < prices.length; i++) {
    if (i < period) {
      results.push({ index: i, rsi: null });
    } else {
      const avgGain = gains.slice(i - period, i).reduce((a, b) => a + b, 0) / period;
      const avgLoss = losses.slice(i - period, i).reduce((a, b) => a + b, 0) / period;
      const rs = avgLoss === 0 ? 100 : avgGain / avgLoss;
      const rsi = 100 - (100 / (1 + rs));
      results.push({ index: i, rsi });
    }
  }
  return results;
}
