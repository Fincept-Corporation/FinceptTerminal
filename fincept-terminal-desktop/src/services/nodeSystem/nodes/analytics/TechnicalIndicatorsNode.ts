/**
 * Technical Indicators Node
 *
 * Calculate 100+ technical indicators on price data.
 * Supports multiple data sources: yfinance, CSV, JSON, or input from previous nodes.
 *
 * Categories:
 * - Momentum: RSI, MACD, Stochastic, Williams %R, etc.
 * - Volume: OBV, Volume Profile, MFI, etc.
 * - Volatility: Bollinger Bands, ATR, Keltner Channels, etc.
 * - Trend: SMA, EMA, VWAP, Supertrend, etc.
 * - Others: Pivot Points, Fibonacci, Support/Resistance, etc.
 */

import { invoke } from '@tauri-apps/api/core';
import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class TechnicalIndicatorsNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Technical Analysis',
    name: 'technicalIndicators',
    icon: 'fa:chart-line',
    group: ['Analytics', 'Technical Analysis'],
    version: 1,
    description: 'Calculate 100+ technical indicators on price data',
    defaults: {
      name: 'Technical Analysis',
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
          { name: 'Yahoo Finance', value: 'yfinance' },
          { name: 'CSV File', value: 'csv' },
          { name: 'JSON Data', value: 'json' },
          { name: 'Input from Previous Node', value: 'input' },
        ],
        default: 'yfinance',
        description: 'Source of price data',
      },
      // Yahoo Finance options
      {
        displayName: 'Symbol',
        name: 'symbol',
        type: NodePropertyType.String,
        default: 'AAPL',
        required: true,
        displayOptions: {
          show: {
            dataSource: ['yfinance'],
          },
        },
        description: 'Stock symbol to analyze',
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
          { name: '10 Years', value: '10y' },
          { name: 'Year to Date', value: 'ytd' },
          { name: 'Max', value: 'max' },
        ],
        default: '1y',
        displayOptions: {
          show: {
            dataSource: ['yfinance'],
          },
        },
        description: 'Time period for historical data',
      },
      // CSV File options
      {
        displayName: 'CSV File Path',
        name: 'csvPath',
        type: NodePropertyType.String,
        default: '',
        required: true,
        displayOptions: {
          show: {
            dataSource: ['csv'],
          },
        },
        description: 'Path to CSV file with OHLCV data',
      },
      // JSON Data options
      {
        displayName: 'JSON Data',
        name: 'jsonData',
        type: NodePropertyType.Json,
        default: '[]',
        required: true,
        displayOptions: {
          show: {
            dataSource: ['json'],
          },
        },
        description: 'JSON array of OHLCV data',
      },
      // Indicator categories
      {
        displayName: 'Indicator Categories',
        name: 'categories',
        type: NodePropertyType.MultiOptions,
        options: [
          { name: 'Momentum', value: 'momentum' },
          { name: 'Volume', value: 'volume' },
          { name: 'Volatility', value: 'volatility' },
          { name: 'Trend', value: 'trend' },
          { name: 'Others', value: 'others' },
        ],
        default: ['momentum', 'volume', 'volatility', 'trend', 'others'],
        description: 'Which categories of indicators to calculate',
      },
      // Output format
      {
        displayName: 'Output Format',
        name: 'outputFormat',
        type: NodePropertyType.Options,
        options: [
          { name: 'Complete Dataset', value: 'complete' },
          { name: 'Latest Values Only', value: 'latest' },
          { name: 'Summary Statistics', value: 'summary' },
        ],
        default: 'complete',
        description: 'How to format the output data',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const items = this.getInputData();
    const returnData = [];

    // Get parameters
    const dataSource = this.getNodeParameter('dataSource', 0) as string;
    const categories = this.getNodeParameter('categories', 0) as string[];
    const outputFormat = this.getNodeParameter('outputFormat', 0) as string;

    try {
      let result: string;

      // Build categories string
      const categoriesStr = categories && categories.length > 0 ? categories.join(',') : undefined;

      // Execute based on data source
      switch (dataSource) {
        case 'yfinance': {
          const symbol = this.getNodeParameter('symbol', 0) as string;
          const period = this.getNodeParameter('period', 0) as string;

          result = await invoke('calculate_indicators_yfinance', {
            symbol,
            period,
            categories: categoriesStr,
          });
          break;
        }

        case 'csv': {
          const csvPath = this.getNodeParameter('csvPath', 0) as string;

          if (!csvPath) {
            throw new Error('CSV file path is required');
          }

          result = await invoke('calculate_indicators_csv', {
            filepath: csvPath,
            categories: categoriesStr,
          });
          break;
        }

        case 'json': {
          const jsonData = this.getNodeParameter('jsonData', 0) as string;

          if (!jsonData) {
            throw new Error('JSON data is required');
          }

          result = await invoke('calculate_indicators_json', {
            jsonData: typeof jsonData === 'string' ? jsonData : JSON.stringify(jsonData),
            categories: categoriesStr,
          });
          break;
        }

        case 'input': {
          // Get data from previous node
          if (items.length === 0) {
            throw new Error('No input data available from previous node');
          }

          const inputData = items[0].json;

          // Convert to JSON and pass to Python
          result = await invoke('calculate_indicators_json', {
            jsonData: JSON.stringify(inputData),
            categories: categoriesStr,
          });
          break;
        }

        default:
          throw new Error(`Unknown data source: ${dataSource}`);
      }

      // Parse result
      const parsedResult = JSON.parse(result);

      // Check for errors
      if (parsedResult.error) {
        throw new Error(parsedResult.error);
      }

      // Format output based on selected format
      let outputData = parsedResult;

      if (outputFormat === 'latest') {
        // Extract only the latest values from each indicator
        outputData = extractLatestValues(parsedResult);
      } else if (outputFormat === 'summary') {
        // Create summary statistics
        outputData = createSummary(parsedResult);
      }

      // Return result
      returnData.push({
        json: {
          dataSource,
          indicators: outputData,
          categories: categories,
          metadata: {
            symbol: dataSource === 'yfinance' ? String(this.getNodeParameter('symbol', 0)) : undefined,
            period: dataSource === 'yfinance' ? String(this.getNodeParameter('period', 0)) : undefined,
            calculatedAt: new Date().toISOString(),
            outputFormat: String(outputFormat),
          } as any,
        },
        pairedItem: 0,
      });
    } catch (error: any) {
      if (this.continueOnFail()) {
        returnData.push({
          json: {
            error: error.message,
            dataSource,
          },
          pairedItem: 0,
        });
      } else {
        throw error;
      }
    }

    return [returnData];
  }
}

/**
 * Extract latest values from indicator results
 */
function extractLatestValues(data: any): any {
  const latest: any = {};

  for (const [category, indicators] of Object.entries(data)) {
    if (typeof indicators === 'object' && indicators !== null) {
      latest[category] = {};
      for (const [indicator, values] of Object.entries(indicators as Record<string, any>)) {
        if (Array.isArray(values) && values.length > 0) {
          latest[category][indicator] = values[values.length - 1];
        } else {
          latest[category][indicator] = values;
        }
      }
    }
  }

  return latest;
}

/**
 * Create summary statistics from indicator results
 */
function createSummary(data: any): any {
  const summary: any = {
    categories: {},
    totalIndicators: 0,
  };

  for (const [category, indicators] of Object.entries(data)) {
    if (typeof indicators === 'object' && indicators !== null) {
      const indicatorCount = Object.keys(indicators).length;
      summary.categories[category] = {
        count: indicatorCount,
        indicators: Object.keys(indicators),
      };
      summary.totalIndicators += indicatorCount;
    }
  }

  return summary;
}
