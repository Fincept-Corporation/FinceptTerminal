/**
 * Stream Quotes Node
 * Streams real-time quotes via WebSocket
 */

import {
  INodeType,
  INodeTypeDescription,
  IExecuteFunctions,
  NodeOutput,
  NodeConnectionType,
  NodePropertyType,
} from '../../types';

export class StreamQuotesNode implements INodeType {
  description: INodeTypeDescription = {
    displayName: 'Stream Quotes',
    name: 'streamQuotes',
    icon: 'file:stream.svg',
    group: ['marketData'],
    version: 1,
    subtitle: 'Real-time {{$parameter["symbols"]}}',
    description: 'Streams real-time quotes via WebSocket',
    defaults: {
      name: 'Stream Quotes',
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
        placeholder: 'AAPL, MSFT, BTC-USD',
        description: 'Comma-separated list of symbols to stream',
      },
      {
        displayName: 'Use Input Symbols',
        name: 'useInputSymbols',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Use symbols from input data',
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
          { name: 'Polygon.io', value: 'polygon' },
          { name: 'Finnhub', value: 'finnhub' },
          { name: 'Alpaca', value: 'alpaca' },
          { name: 'Zerodha', value: 'zerodha' },
          { name: 'Fyers', value: 'fyers' },
        ],
        description: 'Real-time data provider',
      },
      {
        displayName: 'Stream Type',
        name: 'streamType',
        type: NodePropertyType.Options,
        default: 'trades',
        options: [
          { name: 'Trades (Last Price)', value: 'trades' },
          { name: 'Quotes (Bid/Ask)', value: 'quotes' },
          { name: 'Aggregate Trades', value: 'aggTrades' },
          { name: 'Mini Ticker', value: 'miniTicker' },
          { name: 'Full Ticker', value: 'ticker' },
          { name: 'Order Book Updates', value: 'depth' },
        ],
        description: 'Type of streaming data',
      },
      {
        displayName: 'Trigger Mode',
        name: 'triggerMode',
        type: NodePropertyType.Options,
        default: 'onChange',
        options: [
          { name: 'On Every Update', value: 'everyUpdate' },
          { name: 'On Price Change', value: 'onChange' },
          { name: 'On Threshold Cross', value: 'threshold' },
          { name: 'Batch (Time Interval)', value: 'batch' },
        ],
        description: 'When to trigger the next node',
      },
      {
        displayName: 'Price Threshold',
        name: 'priceThreshold',
        type: NodePropertyType.Number,
        default: 0.1,
        description: 'Minimum price change % to trigger',
        displayOptions: {
          show: { triggerMode: ['onChange', 'threshold'] },
        },
      },
      {
        displayName: 'Batch Interval (ms)',
        name: 'batchInterval',
        type: NodePropertyType.Number,
        default: 1000,
        description: 'Milliseconds between batch outputs',
        displayOptions: {
          show: { triggerMode: ['batch'] },
        },
      },
      {
        displayName: 'Max Duration (seconds)',
        name: 'maxDuration',
        type: NodePropertyType.Number,
        default: 0,
        description: 'Maximum streaming duration (0 for unlimited)',
      },
      {
        displayName: 'Buffer Size',
        name: 'bufferSize',
        type: NodePropertyType.Number,
        default: 100,
        description: 'Number of updates to buffer before processing',
      },
      {
        displayName: 'Include Volume',
        name: 'includeVolume',
        type: NodePropertyType.Boolean,
        default: true,
        description: 'Include volume data in stream',
      },
      {
        displayName: 'Include VWAP',
        name: 'includeVwap',
        type: NodePropertyType.Boolean,
        default: false,
        description: 'Calculate and include VWAP',
      },
    ],
    hints: [
      {
        message: 'Streaming nodes run continuously until stopped or duration limit reached',
        type: 'info',
        location: 'outputPane',
        whenToDisplay: 'always',
      },
    ],
  };

  async execute(this: IExecuteFunctions): Promise<NodeOutput> {
    const useInputSymbols = this.getNodeParameter('useInputSymbols', 0) as boolean;
    const provider = this.getNodeParameter('provider', 0) as string;
    const streamType = this.getNodeParameter('streamType', 0) as string;
    const triggerMode = this.getNodeParameter('triggerMode', 0) as string;
    const maxDuration = this.getNodeParameter('maxDuration', 0) as number;
    const includeVolume = this.getNodeParameter('includeVolume', 0) as boolean;
    const includeVwap = this.getNodeParameter('includeVwap', 0) as boolean;

    let symbols: string[] = [];

    if (useInputSymbols) {
      const inputData = this.getInputData();
      for (const item of inputData) {
        if (item.json?.symbol) {
          symbols.push(String(item.json.symbol));
        }
      }
    } else {
      const symbolsStr = this.getNodeParameter('symbols', 0) as string;
      symbols = symbolsStr.split(',').map(s => s.trim()).filter(Boolean);
    }

    if (symbols.length === 0) {
      return [[{
        json: {
          error: 'No symbols provided for streaming',
          timestamp: new Date().toISOString(),
        },
      }]];
    }

    // Generate stream configuration
    // In production, this would establish a WebSocket connection
    const streamId = `stream_${Date.now().toString(36)}`;

    const streamConfig = {
      streamId,
      symbols,
      provider,
      streamType,
      triggerMode,
      maxDuration: maxDuration > 0 ? maxDuration : null,
      includeVolume,
      includeVwap,
      status: 'initialized',
    };

    // Return initial stream setup info
    // The actual streaming would be handled by the workflow executor
    return [[{
      json: {
        type: 'streamSetup',
        ...streamConfig,
        websocketUrl: StreamQuotesNode.getWebSocketUrl(provider, symbols, streamType),
        timestamp: new Date().toISOString(),
        executionId: this.getExecutionId(),
        message: `Streaming ${symbols.join(', ')} via ${provider}`,
      },
    }]];
  }

  private static getWebSocketUrl(provider: string, symbols: string[], streamType: string): string {
    switch (provider) {
      case 'binance':
        const streams = symbols.map(s => `${s.toLowerCase()}@${streamType}`).join('/');
        return `wss://stream.binance.com:9443/stream?streams=${streams}`;
      case 'kraken':
        return 'wss://ws.kraken.com';
      case 'coinbase':
        return 'wss://ws-feed.exchange.coinbase.com';
      case 'polygon':
        return 'wss://socket.polygon.io/stocks';
      case 'finnhub':
        return 'wss://ws.finnhub.io';
      case 'alpaca':
        return 'wss://stream.data.alpaca.markets/v2/iex';
      default:
        return `wss://${provider}.fincept.app/stream`;
    }
  }
}
