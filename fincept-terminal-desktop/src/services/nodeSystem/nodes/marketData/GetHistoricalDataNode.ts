/**
 * Get Historical Data Node
 * Fetches OHLCV historical price data for symbols
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';
import { MarketDataBridge } from '../../adapters/MarketDataBridge';

export class GetHistoricalDataNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Get Historical Data',
    name: 'getHistoricalData',
    icon: 'file:chart.svg',
    group: ['marketData'],
    version: 1,
    subtitle: '={{$parameter["symbol"]}} {{$parameter["interval"]}}',
    description: 'Fetches historical OHLCV price data',
    defaults: {
      name: 'Historical Data',
      color: '#10b981',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Symbol',
        name: 'symbol',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'AAPL',
        description: 'Symbol to fetch historical data for',
      },
      {
        displayName: 'Use Input Symbol',
        name: 'useInputSymbol',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use symbol from input data',
      },
      {
        displayName: 'Interval',
        name: 'interval',
        type: NodePropertyType.Options,
        default: '1d',
        options: [
          { name: '1 Minute', value: '1m' },
          { name: '5 Minutes', value: '5m' },
          { name: '15 Minutes', value: '15m' },
          { name: '30 Minutes', value: '30m' },
          { name: '1 Hour', value: '1h' },
          { name: '4 Hours', value: '4h' },
          { name: '1 Day', value: '1d' },
          { name: '1 Week', value: '1wk' },
          { name: '1 Month', value: '1mo' },
        ],
        description: 'Time interval for candles',
      },
      {
        displayName: 'Period Type',
        name: 'periodType',
        type: NodePropertyType.Options,
        default: 'preset',
        options: [
          { name: 'Preset Period', value: 'preset' },
          { name: 'Custom Date Range', value: 'custom' },
          { name: 'Last N Bars', value: 'bars' },
        ],
        description: 'How to specify the time period',
      },
      {
        displayName: 'Period',
        name: 'period',
        type: NodePropertyType.Options,
        default: '1mo',
        options: [
          { name: '1 Day', value: '1d' },
          { name: '5 Days', value: '5d' },
          { name: '1 Month', value: '1mo' },
          { name: '3 Months', value: '3mo' },
          { name: '6 Months', value: '6mo' },
          { name: '1 Year', value: '1y' },
          { name: '2 Years', value: '2y' },
          { name: '5 Years', value: '5y' },
          { name: '10 Years', value: '10y' },
          { name: 'Year to Date', value: 'ytd' },
          { name: 'Max', value: 'max' },
        ],
        description: 'Preset time period',
        displayOptions: {
          show: { periodType: ['preset'] },
        },
      },
      {
        displayName: 'Start Date',
        name: 'startDate',
        type: NodePropertyType.String,
        default: '',
        placeholder: '2024-01-01',
        description: 'Start date (YYYY-MM-DD)',
        displayOptions: {
          show: { periodType: ['custom'] },
        },
      },
      {
        displayName: 'End Date',
        name: 'endDate',
        type: NodePropertyType.String,
        default: '',
        placeholder: '2024-12-31',
        description: 'End date (YYYY-MM-DD)',
        displayOptions: {
          show: { periodType: ['custom'] },
        },
      },
      {
        displayName: 'Number of Bars',
        name: 'numBars',
        type: NodePropertyType.Number,
        default: 100,
        description: 'Number of historical bars to fetch',
        displayOptions: {
          show: { periodType: ['bars'] },
        },
      },
      {
        displayName: 'Data Provider',
        name: 'provider',
        type: NodePropertyType.Options,
        default: 'yahoo',
        options: [
          { name: 'Yahoo Finance', value: 'yahoo' },
          { name: 'Polygon.io', value: 'polygon' },
          { name: 'Alpha Vantage', value: 'alphavantage' },
          { name: 'Twelve Data', value: 'twelvedata' },
          { name: 'Binance', value: 'binance' },
          { name: 'Kraken', value: 'kraken' },
        ],
        description: 'Market data provider',
      },
      {
        displayName: 'Include Extended Hours',
        name: 'includeExtendedHours',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Include pre-market and after-hours data',
      },
      {
        displayName: 'Adjust for Splits',
        name: 'adjustForSplits',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Adjust prices for stock splits',
      },
      {
        displayName: 'Adjust for Dividends',
        name: 'adjustForDividends',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Adjust prices for dividends',
      },
      {
        displayName: 'Output Format',
        name: 'outputFormat',
        type: NodePropertyType.Options,
        default: 'array',
        options: [
          { name: 'Array of Candles', value: 'array' },
          { name: 'Single Object with Arrays', value: 'object' },
          { name: 'Latest Bar Only', value: 'latest' },
        ],
        description: 'How to format the output data',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const useInputSymbol = this.getNodeParameter('useInputSymbol', 0) as boolean;
    const interval = this.getNodeParameter('interval', 0) as string;
    const periodType = this.getNodeParameter('periodType', 0) as string;
    const provider = this.getNodeParameter('provider', 0) as string;
    const outputFormat = this.getNodeParameter('outputFormat', 0) as string;
    const adjustForSplits = this.getNodeParameter('adjustForSplits', 0) as boolean;
    const adjustForDividends = this.getNodeParameter('adjustForDividends', 0) as boolean;

    let symbol: string;

    if (useInputSymbol) {
      const inputData = this.getInputData();
      symbol = inputData[0]?.json?.symbol as string || '';
    } else {
      symbol = this.getNodeParameter('symbol', 0) as string;
    }

    if (!symbol) {
      return [[{
        json: {
          error: 'No symbol provided',
          timestamp: new Date().toISOString(),
        },
      }]];
    }

    // Build period configuration
    let period: string | undefined;
    let startDate: string | undefined;
    let endDate: string | undefined;
    let numBars: number | undefined;

    switch (periodType) {
      case 'preset':
        period = this.getNodeParameter('period', 0) as string;
        break;
      case 'custom':
        startDate = this.getNodeParameter('startDate', 0) as string;
        endDate = this.getNodeParameter('endDate', 0) as string;
        break;
      case 'bars':
        numBars = this.getNodeParameter('numBars', 0) as number;
        break;
    }

    // Fetch historical data using the bridge
    const bridge = new MarketDataBridge();

    try {
      const data = await bridge.getHistoricalData(symbol, {
        interval,
        period,
        startDate,
        endDate,
        limit: numBars,
        provider,
      });

      const candles = data.candles || [];

      // Format output based on preference
      switch (outputFormat) {
        case 'latest':
          if (candles.length === 0) {
            return [[{
              json: {
                symbol,
                error: 'No data available',
                timestamp: new Date().toISOString(),
              },
            }]];
          }
          const latest = candles[candles.length - 1];
          return [[{
            json: {
              symbol,
              ...latest,
              interval,
              provider,
              fetchedAt: new Date().toISOString(),
            },
          }]];

        case 'object':
          return [[{
            json: {
              symbol,
              interval,
              period: period || `${startDate} to ${endDate}`,
              totalBars: candles.length,
              timestamps: candles.map((c: any) => c.timestamp),
              open: candles.map((c: any) => c.open),
              high: candles.map((c: any) => c.high),
              low: candles.map((c: any) => c.low),
              close: candles.map((c: any) => c.close),
              volume: candles.map((c: any) => c.volume),
              provider,
              adjustedForSplits: adjustForSplits,
              adjustedForDividends: adjustForDividends,
              fetchedAt: new Date().toISOString(),
            },
          }]];

        case 'array':
        default:
          return [candles.map((candle: any) => ({
            json: {
              symbol,
              ...candle,
              interval,
              provider,
            },
          }))];
      }
    } catch (error) {
      return [[{
        json: {
          symbol,
          error: error instanceof Error ? error.message : 'Unknown error',
          timestamp: new Date().toISOString(),
        },
      }]];
    }
  }
}
