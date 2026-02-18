/**
 * Get Quote Node
 * Fetches real-time quotes via Rust → yfinance Python backend.
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

export class GetQuoteNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Stock Quote',
    name: 'getQuote',
    icon: 'file:quote.svg',
    group: ['marketData'],
    version: 1,
    subtitle: '={{$parameter["symbols"]}}',
    description: 'Fetches real-time quotes via yfinance',
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
        description: 'Comma-separated list of symbols. Append .NS/.BO for Indian stocks.',
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
        description: 'Field name in input data containing the symbol',
        displayOptions: {
          show: { useInputSymbols: [true] },
        },
      },
      {
        displayName: 'Include Details',
        name: 'includeDetails',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include open/high/low/volume alongside price',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const useInputSymbols = this.getNodeParameter('useInputSymbols', 0) as boolean;
    const includeDetails = this.getNodeParameter('includeDetails', 0) as boolean;

    let symbols: string[] = [];

    if (useInputSymbols) {
      const inputSymbolField = this.getNodeParameter('inputSymbolField', 0) as string;
      for (const item of this.getInputData()) {
        if (item.json?.[inputSymbolField]) {
          const sym = item.json[inputSymbolField];
          if (Array.isArray(sym)) {
            symbols.push(...sym.map((s: any) => String(s).trim()));
          } else {
            symbols.push(String(sym).trim());
          }
        }
      }
    } else {
      const symbolsStr = this.getNodeParameter('symbols', 0) as string;
      symbols = symbolsStr.split(',').map((s) => s.trim()).filter(Boolean);
    }

    if (symbols.length === 0) {
      return [[{ json: { error: 'No symbols provided', timestamp: new Date().toISOString() } }]];
    }

    const results: Array<{ json: Record<string, any> }> = [];

    for (const symbol of symbols) {
      try {
        const quote = await MarketDataBridge.getQuote(symbol);

        results.push({
          json: includeDetails
            ? { ...quote, fetchedAt: new Date().toISOString() }
            : {
                symbol: quote.symbol,
                price: quote.price,
                change: quote.change,
                changePercent: quote.changePercent,
                fetchedAt: new Date().toISOString(),
              },
        });
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
