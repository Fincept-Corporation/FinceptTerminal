/**
 * YFinance Data Source Node
 * Comprehensive Yahoo Finance data fetching with all yfinance features
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class YFinanceNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Yahoo Finance',
    name: 'yfinance',
    icon: 'fa:yahoo',
    iconColor: '#7B0099',
    group: ['marketData'],
    version: 1,
    subtitle: '={{$parameter["operation"]}} - {{$parameter["symbol"]}}',
    description: 'Fetch comprehensive market data from Yahoo Finance',
    defaults: {
      name: 'Yahoo Finance',
      color: '#7B0099',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Operation',
        name: 'operation',
        type: NodePropertyType.Options,
        options: [
          { name: 'Get Historical Data', value: 'history' },
          { name: 'Get Quote', value: 'quote' },
          { name: 'Get Info', value: 'info' },
          { name: 'Get Financials', value: 'financials' },
          { name: 'Get Balance Sheet', value: 'balanceSheet' },
          { name: 'Get Cash Flow', value: 'cashFlow' },
          { name: 'Get Earnings', value: 'earnings' },
          { name: 'Get Dividends', value: 'dividends' },
          { name: 'Get Splits', value: 'splits' },
          { name: 'Get Actions', value: 'actions' },
          { name: 'Get Recommendations', value: 'recommendations' },
          { name: 'Get Options Chain', value: 'options' },
          { name: 'Get Institutional Holders', value: 'institutionalHolders' },
          { name: 'Get Major Holders', value: 'majorHolders' },
          { name: 'Get Sustainability', value: 'sustainability' },
        ],
        default: 'history',
        description: 'Type of data to fetch from Yahoo Finance',
      },
      {
        displayName: 'Symbol',
        name: 'symbol',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'AAPL',
        description: 'Stock ticker symbol',
      },
      {
        displayName: 'Use Input Symbol',
        name: 'useInputSymbol',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use symbol from input data instead',
      },

      // Historical Data Parameters
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
        default: '1mo',
        description: 'Historical period to fetch',
        displayOptions: {
          show: {
            operation: ['history'],
          },
        },
      },
      {
        displayName: 'Interval',
        name: 'interval',
        type: NodePropertyType.Options,
        options: [
          { name: '1 Minute', value: '1m' },
          { name: '2 Minutes', value: '2m' },
          { name: '5 Minutes', value: '5m' },
          { name: '15 Minutes', value: '15m' },
          { name: '30 Minutes', value: '30m' },
          { name: '60 Minutes', value: '60m' },
          { name: '90 Minutes', value: '90m' },
          { name: '1 Hour', value: '1h' },
          { name: '1 Day', value: '1d' },
          { name: '5 Days', value: '5d' },
          { name: '1 Week', value: '1wk' },
          { name: '1 Month', value: '1mo' },
          { name: '3 Months', value: '3mo' },
        ],
        default: '1d',
        description: 'Data interval/timeframe',
        displayOptions: {
          show: {
            operation: ['history'],
          },
        },
      },
      {
        displayName: 'Start Date',
        name: 'startDate',
        type: NodePropertyType.String,
        default: '',
        placeholder: '2024-01-01',
        description: 'Start date (YYYY-MM-DD) - overrides period',
        displayOptions: {
          show: {
            operation: ['history'],
          },
        },
      },
      {
        displayName: 'End Date',
        name: 'endDate',
        type: NodePropertyType.String,
        default: '',
        placeholder: '2024-12-31',
        description: 'End date (YYYY-MM-DD) - overrides period',
        displayOptions: {
          show: {
            operation: ['history'],
          },
        },
      },
      {
        displayName: 'Prepost',
        name: 'prepost',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Include pre and post market data',
        displayOptions: {
          show: {
            operation: ['history'],
          },
        },
      },
      {
        displayName: 'Auto Adjust',
        name: 'autoAdjust',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Adjust all OHLC automatically',
        displayOptions: {
          show: {
            operation: ['history'],
          },
        },
      },
      {
        displayName: 'Actions',
        name: 'actions',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Download stock dividends and stock splits events',
        displayOptions: {
          show: {
            operation: ['history'],
          },
        },
      },

      // Financials Parameters
      {
        displayName: 'Frequency',
        name: 'frequency',
        type: NodePropertyType.Options,
        options: [
          { name: 'Annual', value: 'annual' },
          { name: 'Quarterly', value: 'quarterly' },
        ],
        default: 'annual',
        description: 'Financial statement frequency',
        displayOptions: {
          show: {
            operation: ['financials', 'balanceSheet', 'cashFlow'],
          },
        },
      },

      // Options Parameters
      {
        displayName: 'Expiration Date',
        name: 'expirationDate',
        type: NodePropertyType.String,
        default: '',
        placeholder: '2024-12-20 or leave empty for nearest',
        description: 'Options expiration date (YYYY-MM-DD)',
        displayOptions: {
          show: {
            operation: ['options'],
          },
        },
      },
      {
        displayName: 'Options Type',
        name: 'optionsType',
        type: NodePropertyType.Options,
        options: [
          { name: 'Both', value: 'both' },
          { name: 'Calls', value: 'calls' },
          { name: 'Puts', value: 'puts' },
        ],
        default: 'both',
        description: 'Type of options to fetch',
        displayOptions: {
          show: {
            operation: ['options'],
          },
        },
      },

      // Output Format
      {
        displayName: 'Output Format',
        name: 'outputFormat',
        type: NodePropertyType.Options,
        options: [
          { name: 'Array (One Item Per Row)', value: 'array' },
          { name: 'Single Object', value: 'object' },
        ],
        default: 'array',
        description: 'How to format the output data',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    // Import Tauri invoke at runtime
    const { invoke } = await import('@tauri-apps/api/core');

    const items = this.getInputData();
    const operation = this.getNodeParameter('operation', 0) as string;
    const useInputSymbol = this.getNodeParameter('useInputSymbol', 0) as boolean;
    const outputFormat = this.getNodeParameter('outputFormat', 0) as string;

    let symbol: string;
    if (useInputSymbol && items.length > 0) {
      symbol = items[0].json.symbol as string || '';
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

    try {
      // Build Python script arguments based on operation
      const args: string[] = [symbol, operation];

      // Add operation-specific parameters
      if (operation === 'history') {
        const period = this.getNodeParameter('period', 0) as string;
        const interval = this.getNodeParameter('interval', 0) as string;
        const startDate = this.getNodeParameter('startDate', 0, '') as string;
        const endDate = this.getNodeParameter('endDate', 0, '') as string;
        const prepost = this.getNodeParameter('prepost', 0) as boolean;
        const autoAdjust = this.getNodeParameter('autoAdjust', 0) as boolean;
        const actions = this.getNodeParameter('actions', 0) as boolean;

        if (startDate && endDate) {
          args.push('--start', startDate, '--end', endDate);
        } else {
          args.push('--period', period);
        }
        args.push('--interval', interval);
        if (prepost) args.push('--prepost', 'true');
        if (!autoAdjust) args.push('--auto_adjust', 'false');
        if (!actions) args.push('--actions', 'false');
      } else if (['financials', 'balanceSheet', 'cashFlow'].includes(operation)) {
        const frequency = this.getNodeParameter('frequency', 0) as string;
        args.push(frequency);
      } else if (operation === 'options') {
        const expirationDate = this.getNodeParameter('expirationDate', 0, '') as string;
        if (expirationDate) args.push(expirationDate);
      }

      // Execute Python script
      const result = await invoke('execute_python_command', {
        script: 'yfinance_comprehensive.py',
        args,
      });

      // Parse result
      const data = typeof result === 'string' ? JSON.parse(result) : result;

      // Check for errors
      if (data.error) {
        throw new Error(data.error);
      }

      // Format output
      if (outputFormat === 'object' || !Array.isArray(data)) {
        return [[{ json: { symbol, operation, data, timestamp: new Date().toISOString() } }]];
      } else {
        return [data.map((item: any) => ({ json: { symbol, operation, ...item } }))];
      }
    } catch (error) {
      if (this.continueOnFail()) {
        return [[{
          json: {
            symbol,
            operation,
            error: error instanceof Error ? error.message : 'Unknown error',
            timestamp: new Date().toISOString(),
          },
        }]];
      }
      throw error;
    }
  }

}
