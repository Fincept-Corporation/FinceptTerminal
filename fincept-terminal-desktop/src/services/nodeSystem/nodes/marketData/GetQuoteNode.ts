/**
 * Get Quote Node
 * Fetches real-time or delayed quotes for one or more symbols
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';
import { MarketDataBridge, DataProvider } from '../../adapters/MarketDataBridge';

export class GetQuoteNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Stock Quote',
    name: 'getQuote',
    icon: 'file:quote.svg',
    group: ['marketData'],
    version: 1,
    subtitle: '={{$parameter["symbols"]}}',
    description: 'Fetches real-time or delayed quotes for symbols',
    defaults: {
      name: 'Stock Quote',
      color: '#10b981',
    },
    inputs: [NodeConnectionType.Main],
    outputs: [NodeConnectionType.Main],
    properties: [
      {
        displayName: 'Symbols',
        name: 'symbols',
        type: NodePropertyType.String,
        default: '',
        required: true,
        placeholder: 'AAPL, MSFT, GOOGL',
        description: 'Comma-separated list of symbols (stocks, crypto, forex)',
      },
      {
        displayName: 'Use Input Symbols',
        name: 'useInputSymbols',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use symbols from input data instead of static list',
      },
      {
        displayName: 'Input Symbol Field',
        name: 'inputSymbolField',
        type: NodePropertyType.String,
        default: 'symbol',
        description: 'Field name in input data containing symbols',
        displayOptions: {
          show: { useInputSymbols: [true] },
        },
      },
      {
        displayName: 'Asset Type',
        name: 'assetType',
        type: NodePropertyType.Options,
        default: 'stock',
        options: [
          { name: 'Stock', value: 'stock' },
          { name: 'Crypto', value: 'crypto' },
          { name: 'Forex', value: 'forex' },
          { name: 'ETF', value: 'etf' },
          { name: 'Index', value: 'index' },
          { name: 'Futures', value: 'futures' },
          { name: 'Options', value: 'options' },
        ],
        description: 'Type of asset to quote',
      },
      {
        displayName: 'Data Provider',
        name: 'provider',
        type: NodePropertyType.Options,
        default: 'yahoo',
        options: [
          { name: 'Yahoo Finance', value: 'yahoo' },
          { name: 'Alpha Vantage', value: 'alphavantage' },
          { name: 'Finnhub', value: 'finnhub' },
          { name: 'IEX Cloud', value: 'iex' },
          { name: 'Twelve Data', value: 'twelvedata' },
          { name: 'Binance', value: 'binance' },
          { name: 'CoinGecko', value: 'coingecko' },
          { name: 'Kraken', value: 'kraken' },
        ],
        description: 'Market data provider',
      },
      {
        displayName: 'Exchange',
        name: 'exchange',
        type: NodePropertyType.Options,
        default: 'auto',
        options: [
          { name: 'Auto-detect', value: 'auto' },
          { name: 'NYSE', value: 'NYSE' },
          { name: 'NASDAQ', value: 'NASDAQ' },
          { name: 'NSE (India)', value: 'NSE' },
          { name: 'BSE (India)', value: 'BSE' },
          { name: 'LSE (UK)', value: 'LSE' },
          { name: 'TSE (Tokyo)', value: 'TSE' },
          { name: 'HKEX', value: 'HKEX' },
        ],
        description: 'Specific exchange (for stocks with same ticker)',
        displayOptions: {
          show: { assetType: ['stock', 'etf'] },
        },
      },
      {
        displayName: 'Include Extended Hours',
        name: 'includeExtendedHours',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Include pre-market and after-hours data',
        displayOptions: {
          show: { assetType: ['stock', 'etf'] },
        },
      },
      {
        displayName: 'Include Details',
        name: 'includeDetails',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include additional details (day high/low, volume, etc.)',
      },
      {
        displayName: 'Cache Duration (seconds)',
        name: 'cacheDuration',
        type: NodePropertyType.Number,
        default: 15,
        description: 'How long to cache quotes (0 for no caching)',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const useInputSymbols = this.getNodeParameter('useInputSymbols', 0) as boolean;
    const assetType = this.getNodeParameter('assetType', 0) as string;
    const provider = this.getNodeParameter('provider', 0) as DataProvider;
    const includeDetails = this.getNodeParameter('includeDetails', 0) as boolean;

    let symbols: string[] = [];

    if (useInputSymbols) {
      const inputData = this.getInputData();
      const inputSymbolField = this.getNodeParameter('inputSymbolField', 0) as string;

      for (const item of inputData) {
        if (item.json && item.json[inputSymbolField]) {
          const sym = item.json[inputSymbolField];
          if (Array.isArray(sym)) {
            symbols.push(...sym.map(s => String(s).trim()));
          } else {
            symbols.push(String(sym).trim());
          }
        }
      }
    } else {
      const symbolsStr = this.getNodeParameter('symbols', 0) as string;
      symbols = symbolsStr.split(',').map(s => s.trim()).filter(Boolean);
    }

    if (symbols.length === 0) {
      return [[{
        json: {
          error: 'No symbols provided',
          timestamp: new Date().toISOString(),
        },
      }]];
    }

    // Fetch quotes using the bridge
    
    const results: Array<{ json: Record<string, any> }> = [];

    for (const symbol of symbols) {
      try {
        const quote = await MarketDataBridge.getQuote(symbol, provider);

        if (includeDetails) {
          results.push({
            json: {
              ...quote,
              assetType,
              provider,
              fetchedAt: new Date().toISOString(),
            },
          });
        } else {
          // Minimal quote data
          results.push({
            json: {
              symbol: quote.symbol,
              price: quote.price,
              change: quote.change,
              changePercent: quote.changePercent,
              fetchedAt: new Date().toISOString(),
            },
          });
        }
      } catch (error) {
        results.push({
          json: {
            symbol,
            error: error instanceof Error ? error.message : 'Unknown error',
            fetchedAt: new Date().toISOString(),
          },
        });
      }
    }

    return [results];
  }
}
