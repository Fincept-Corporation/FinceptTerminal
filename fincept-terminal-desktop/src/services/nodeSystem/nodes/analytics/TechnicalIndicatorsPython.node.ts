/**
 * Technical Indicators Node (Python-Powered)
 * Uses existing Python technical analysis scripts
 * Supports 50+ indicators across 5 categories
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class TechnicalIndicatorsPythonNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Technical Indicators (Python)',
    name: 'technicalIndicatorsPython',
    icon: 'fa:chart-line',
    iconColor: '#10b981',
    group: ['analytics'],
    version: 1,
    description: 'Calculate 50+ technical indicators using Python (pandas-ta)',
    defaults: {
      name: 'Technical Indicators',
      color: '#10b981',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Data Source',
        name: 'dataSource',
        type: NodePropertyType.Options,
        options: [
          { name: 'From Input Data', value: 'input' },
          { name: 'From Yahoo Finance', value: 'yfinance' },
        ],
        default: 'input',
        description: 'Where to get price data from',
      },
      {
        displayName: 'Symbol',
        name: 'symbol',
        type: NodePropertyType.String,
        default: 'AAPL',
        displayOptions: {
          show: {
            dataSource: ['yfinance'],
          },
        },
        description: 'Stock symbol to fetch from Yahoo Finance',
      },
      {
        displayName: 'Period',
        name: 'period',
        type: NodePropertyType.Options,
        options: [
          { name: '1 Day', value: '1d' },
          { name: '5 Days', value: '5d' },
          { name: '1 Month', value: '1mo' },
          { name: '3 Months', value: '3mo' },
          { name: '6 Months', value: '6mo' },
          { name: '1 Year', value: '1y' },
          { name: '2 Years', value: '2y' },
          { name: '5 Years', value: '5y' },
          { name: 'Max', value: 'max' },
        ],
        default: '1y',
        displayOptions: {
          show: {
            dataSource: ['yfinance'],
          },
        },
        description: 'Historical data period',
      },
      {
        displayName: 'Categories',
        name: 'categories',
        type: NodePropertyType.MultiOptions,
        options: [
          {
            name: 'Momentum Indicators',
            value: 'momentum',
            description: 'RSI, Stochastic, Williams %R, KAMA, ROC, TSI, Ultimate Oscillator, PPO, PVO',
          },
          {
            name: 'Volume Indicators',
            value: 'volume',
            description: 'ADI, OBV, CMF, Force Index, EoM, VPT, NVI, VWAP, MFI',
          },
          {
            name: 'Volatility Indicators',
            value: 'volatility',
            description: 'ATR, Bollinger Bands, Keltner Channel, Donchian Channel, Ulcer Index',
          },
          {
            name: 'Trend Indicators',
            value: 'trend',
            description: 'SMA, EMA, WMA, MACD, TRIX, Mass Index, Ichimoku, KST, DPO, CCI, ADX, Vortex, PSAR, STC, Aroon',
          },
          {
            name: 'Others',
            value: 'others',
            description: 'Daily Return, Daily Log Return, Cumulative Return',
          },
        ],
        default: ['momentum', 'trend', 'volatility'],
        description: 'Which categories of indicators to calculate',
      },
      {
        displayName: 'Output Format',
        name: 'outputFormat',
        type: NodePropertyType.Options,
        options: [
          { name: 'All Data with Indicators', value: 'full' },
          { name: 'Only Indicators', value: 'indicators_only' },
          { name: 'Latest Values Only', value: 'latest' },
        ],
        default: 'full',
        description: 'How to format the output data',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    // Import Tauri invoke at runtime
    const { invoke } = await import('@tauri-apps/api/core');

    const items = this.getInputData();
    const dataSource = this.getNodeParameter('dataSource', 0) as string;
    const categories = this.getNodeParameter('categories', 0) as string[];
    const outputFormat = this.getNodeParameter('outputFormat', 0) as string;

    try {
      let result: any;
      const categoriesStr = categories.join(',');

      if (dataSource === 'yfinance') {
        // Fetch from Yahoo Finance and calculate indicators
        const symbol = this.getNodeParameter('symbol', 0) as string;
        const period = this.getNodeParameter('period', 0) as string;

        const args = ['yfinance', symbol, period, categoriesStr];

        result = await invoke('execute_python_command', {
          script: 'technicals/technical_analysis.py',
          args,
        });
      } else {
        // Use input data
        if (items.length === 0) {
          throw new Error('No input data provided. Connect a data source node or use Yahoo Finance mode.');
        }

        // Convert input items to JSON format expected by Python script
        const priceData = items.map(item => ({
          timestamp: item.json.timestamp || item.json.date || Date.now() / 1000,
          open: item.json.open,
          high: item.json.high,
          low: item.json.low,
          close: item.json.close,
          volume: item.json.volume || 0,
        }));

        const jsonData = JSON.stringify(priceData);
        const args = ['json', jsonData, categoriesStr];

        result = await invoke('execute_python_command', {
          script: 'technicals/technical_analysis.py',
          args,
        });
      }

      // Parse result
      const data = typeof result === 'string' ? JSON.parse(result) : result;

      // Check for errors
      if (data.error) {
        throw new Error(data.error);
      }

      // Format output based on user preference
      if (outputFormat === 'latest') {
        // Return only the latest row with all indicator values
        const latest = Array.isArray(data) ? data[data.length - 1] : data;
        return [[{ json: latest }]];
      } else if (outputFormat === 'indicators_only') {
        // Return only indicator columns (exclude OHLCV)
        const baseColumns = ['timestamp', 'open', 'high', 'low', 'close', 'volume', 'date'];
        const indicators = Array.isArray(data) ? data.map((row: any) => {
          const indicatorData: any = {};
          for (const key in row) {
            if (!baseColumns.includes(key.toLowerCase())) {
              indicatorData[key] = row[key];
            }
          }
          return indicatorData;
        }) : [data];

        return [indicators.map((item: any) => ({ json: item }))];
      } else {
        // Return full data with all columns
        const fullData = Array.isArray(data) ? data : [data];
        return [fullData.map((item: any) => ({ json: item }))];
      }
    } catch (error) {
      if (this.continueOnFail()) {
        return [[{
          json: {
            error: error instanceof Error ? error.message : 'Unknown error',
            timestamp: new Date().toISOString(),
          },
        }]];
      }
      throw error;
    }
  }
}
