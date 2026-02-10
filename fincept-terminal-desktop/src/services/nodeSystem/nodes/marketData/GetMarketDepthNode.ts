/**
 * Get Market Depth Node
 * Fetches order book / market depth data
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

export class GetMarketDepthNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Market Depth',
    name: 'getMarketDepth',
    icon: 'file:orderbook.svg',
    group: ['marketData'],
    version: 1,
    subtitle: '={{$parameter["symbol"]}} L{{$parameter["levels"]}}',
    description: 'Fetches order book / market depth data',
    defaults: {
      name: 'Market Depth',
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
        placeholder: 'AAPL or BTC-USD',
        description: 'Symbol to fetch order book for',
      },
      {
        displayName: 'Use Input Symbol',
        name: 'useInputSymbol',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use symbol from input data',
      },
      {
        displayName: 'Levels',
        name: 'levels',
        type: NodePropertyType.Options,
        default: '10',
        options: [
          { name: '5 Levels', value: '5' },
          { name: '10 Levels', value: '10' },
          { name: '20 Levels', value: '20' },
          { name: '50 Levels', value: '50' },
          { name: '100 Levels', value: '100' },
          { name: 'Full Book', value: 'full' },
        ],
        description: 'Number of price levels to fetch',
      },
      {
        displayName: 'Data Provider',
        name: 'provider',
        type: NodePropertyType.Options,
        default: 'binance',
        options: [
          { name: 'Binance', value: 'binance' },
          { name: 'Kraken', value: 'kraken' },
          { name: 'Coinbase', value: 'coinbase' },
          { name: 'IEX Cloud', value: 'iex' },
          { name: 'Zerodha', value: 'zerodha' },
          { name: 'Fyers', value: 'fyers' },
        ],
        description: 'Market data provider',
      },
      {
        displayName: 'Include Aggregated Stats',
        name: 'includeStats',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include aggregated order book statistics',
      },
      {
        displayName: 'Include Imbalance',
        name: 'includeImbalance',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Calculate bid/ask imbalance metrics',
      },
      {
        displayName: 'Group By Price',
        name: 'groupByPrice',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Aggregate orders at same price level',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const useInputSymbol = this.getNodeParameter('useInputSymbol', 0) as boolean;
    const levels = this.getNodeParameter('levels', 0) as string;
    const provider = this.getNodeParameter('provider', 0) as string;
    const includeStats = this.getNodeParameter('includeStats', 0) as boolean;
    const includeImbalance = this.getNodeParameter('includeImbalance', 0) as boolean;

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

    

    try {
      const depth = await MarketDataBridge.getMarketDepth(symbol, provider as any, parseInt(levels) || 10);

      const result: Record<string, any> = {
        symbol,
        levels: parseInt(levels) || 10,
        provider,
        bids: depth.bids,
        asks: depth.asks,
        timestamp: depth.timestamp,
        fetchedAt: new Date().toISOString(),
      };

      if (includeStats && depth.bids && depth.asks) {
        const totalBidVolume = depth.bids.reduce((sum: number, item: { price: number; quantity: number }) => sum + item.quantity, 0);
        const totalAskVolume = depth.asks.reduce((sum: number, item: { price: number; quantity: number }) => sum + item.quantity, 0);
        const bestBid = depth.bids[0]?.price || 0;
        const bestAsk = depth.asks[0]?.price || 0;
        const spread = bestAsk - bestBid;
        const spreadPercent = bestBid > 0 ? (spread / bestBid) * 100 : 0;
        const midPrice = (bestBid + bestAsk) / 2;

        result.stats = {
          bestBid,
          bestAsk,
          spread,
          spreadPercent: spreadPercent.toFixed(4),
          midPrice,
          totalBidVolume,
          totalAskVolume,
          bidLevels: depth.bids.length,
          askLevels: depth.asks.length,
        };
      }

      if (includeImbalance && depth.bids && depth.asks) {
        const bidVolume = depth.bids.slice(0, 5).reduce((sum: number, item: { price: number; quantity: number }) => sum + item.quantity, 0);
        const askVolume = depth.asks.slice(0, 5).reduce((sum: number, item: { price: number; quantity: number }) => sum + item.quantity, 0);
        const totalVolume = bidVolume + askVolume;

        const imbalance = totalVolume > 0 ? (bidVolume - askVolume) / totalVolume : 0;
        const bidPressure = totalVolume > 0 ? bidVolume / totalVolume : 0.5;

        result.imbalance = {
          ratio: imbalance.toFixed(4),
          bidPressure: (bidPressure * 100).toFixed(2) + '%',
          askPressure: ((1 - bidPressure) * 100).toFixed(2) + '%',
          signal: imbalance > 0.3 ? 'bullish' : imbalance < -0.3 ? 'bearish' : 'neutral',
        };
      }

      return [[{ json: result }]];
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
