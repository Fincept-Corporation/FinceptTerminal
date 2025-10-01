// Pre-built templates for Indian brokers

import { MappingTemplate, ParserEngine } from '../types';

export const INDIAN_BROKER_TEMPLATES: MappingTemplate[] = [
  // ========== UPSTOX TEMPLATES ==========

  {
    id: 'upstox_historical_ohlcv',
    name: 'Upstox Historical Candles → OHLCV',
    description: 'Map Upstox historical candle data to standard OHLCV format',
    category: 'broker',
    tags: ['upstox', 'historical', 'candles', 'ohlcv', 'india'],
    verified: true,
    official: true,

    mappingConfig: {
      name: 'Upstox Historical → OHLCV',
      description: 'Transform Upstox historical candle data',
      target: {
        schema: 'OHLCV',
      },
      extraction: {
        engine: ParserEngine.JSONATA,
        rootPath: 'data.candles',
        isArray: true,
        isArrayOfArrays: true,
      },
      fieldMappings: [
        { targetField: 'symbol', sourceExpression: 'symbol', required: true, parserEngine: ParserEngine.DIRECT, defaultValue: 'UNKNOWN' },
        { targetField: 'timestamp', sourceExpression: '$[0]', required: true, parserEngine: ParserEngine.JSONATA, transform: 'toISODate' },
        { targetField: 'open', sourceExpression: '$[1]', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'high', sourceExpression: '$[2]', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'low', sourceExpression: '$[3]', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'close', sourceExpression: '$[4]', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'volume', sourceExpression: '$[5]', required: true, parserEngine: ParserEngine.JSONATA },
      ],
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['upstox', 'ohlcv'],
        aiGenerated: false,
      },
    },

    instructions: `
1. Configure Upstox connection in Data Sources
2. Use endpoint: /historical-candle/{instrument_key}/{interval}/{to_date}/{from_date}
3. Parameters: instrument_key (e.g., NSE_EQ|INE062A01020), interval (1minute, day, etc.)
4. Sample response format: { "status": "success", "data": { "candles": [[timestamp, o, h, l, c, v, oi]] } }
    `,
  },

  {
    id: 'upstox_market_quote_full',
    name: 'Upstox Market Quote → QUOTE',
    description: 'Map Upstox market quote data to standard QUOTE format',
    category: 'broker',
    tags: ['upstox', 'quote', 'realtime', 'india'],
    verified: true,
    official: true,

    mappingConfig: {
      name: 'Upstox Quote → QUOTE',
      description: 'Transform Upstox market quote data',
      target: {
        schema: 'QUOTE',
      },
      extraction: {
        engine: ParserEngine.JSONATA,
        rootPath: 'data',
        isArray: false,
      },
      fieldMappings: [
        { targetField: 'symbol', sourceExpression: 'symbol', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'timestamp', sourceExpression: 'last_trade_time', required: true, parserEngine: ParserEngine.JSONATA, transform: 'toISODate' },
        { targetField: 'price', sourceExpression: 'last_price', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'bid', sourceExpression: 'depth.buy[0].price', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'ask', sourceExpression: 'depth.sell[0].price', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'bidSize', sourceExpression: 'depth.buy[0].quantity', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'askSize', sourceExpression: 'depth.sell[0].quantity', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'volume', sourceExpression: 'volume', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'open', sourceExpression: 'ohlc.open', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'high', sourceExpression: 'ohlc.high', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'low', sourceExpression: 'ohlc.low', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'previousClose', sourceExpression: 'ohlc.close', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'change', sourceExpression: 'net_change', required: false, parserEngine: ParserEngine.JSONATA },
      ],
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['upstox', 'quote'],
        aiGenerated: false,
      },
    },
  },

  // ========== FYERS TEMPLATES ==========

  {
    id: 'fyers_quotes',
    name: 'Fyers Quotes → QUOTE',
    description: 'Map Fyers quote data to standard QUOTE format',
    category: 'broker',
    tags: ['fyers', 'quote', 'realtime', 'india'],
    verified: true,
    official: true,

    mappingConfig: {
      name: 'Fyers Quote → QUOTE',
      description: 'Transform Fyers quote data',
      target: {
        schema: 'QUOTE',
      },
      extraction: {
        engine: ParserEngine.JSONATA,
        rootPath: 'd',
        isArray: true,
      },
      fieldMappings: [
        { targetField: 'symbol', sourceExpression: 'n', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'timestamp', sourceExpression: 'v.tt', required: true, parserEngine: ParserEngine.JSONATA, transform: 'toISODate' },
        { targetField: 'price', sourceExpression: 'v.lp', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'bid', sourceExpression: 'v.bid', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'ask', sourceExpression: 'v.ask', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'volume', sourceExpression: 'v.volume', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'open', sourceExpression: 'v.open_price', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'high', sourceExpression: 'v.high_price', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'low', sourceExpression: 'v.low_price', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'previousClose', sourceExpression: 'v.prev_close_price', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'change', sourceExpression: 'v.ch', required: false, parserEngine: ParserEngine.JSONATA },
        { targetField: 'changePercent', sourceExpression: 'v.chp', required: false, parserEngine: ParserEngine.JSONATA },
      ],
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['fyers', 'quote'],
        aiGenerated: false,
      },
    },

    instructions: `
1. Configure Fyers connection in Data Sources
2. Use endpoint: /data/quotes
3. Method: POST with symbols array in body
4. Sample response: { "s": "ok", "d": [{ "n": "NSE:SBIN-EQ", "v": {...} }] }
    `,
  },

  {
    id: 'fyers_positions',
    name: 'Fyers Positions → POSITION',
    description: 'Map Fyers position data to standard POSITION format',
    category: 'broker',
    tags: ['fyers', 'position', 'portfolio', 'india'],
    verified: true,
    official: true,

    mappingConfig: {
      name: 'Fyers Position → POSITION',
      description: 'Transform Fyers position data',
      target: {
        schema: 'POSITION',
      },
      extraction: {
        engine: ParserEngine.JSONATA,
        rootPath: 'netPositions',
        isArray: true,
      },
      fieldMappings: [
        { targetField: 'symbol', sourceExpression: 'symbol', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'quantity', sourceExpression: 'netQty', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'averagePrice', sourceExpression: 'avgPrice', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'currentPrice', sourceExpression: 'ltp', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'pnl', sourceExpression: 'pl', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'pnlPercent', sourceExpression: 'plPercent', required: true, parserEngine: ParserEngine.JSONATA },
        { targetField: 'productType', sourceExpression: 'productType', required: false, parserEngine: ParserEngine.JSONATA },
      ],
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['fyers', 'position'],
        aiGenerated: false,
      },
    },
  },

  // ========== DHAN TEMPLATES ==========

  {
    id: 'dhan_positions',
    name: 'Dhan Positions → POSITION',
    description: 'Map Dhan position data to standard POSITION format',
    category: 'broker',
    tags: ['dhan', 'position', 'portfolio', 'india'],
    verified: true,
    official: true,

    mappingConfig: {
      name: 'Dhan Position → POSITION',
      description: 'Transform Dhan position data',
      target: {
        schema: 'POSITION',
      },
      extraction: {
        engine: ParserEngine.JSONPATH,
        rootPath: '$.data[*]',
        isArray: true,
      },
      fieldMappings: [
        { targetField: 'symbol', sourceExpression: 'tradingSymbol', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'quantity', sourceExpression: 'netQty', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'averagePrice', sourceExpression: 'costPrice', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'currentPrice', sourceExpression: 'ltp', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'pnl', sourceExpression: 'realizedProfit', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'exchange', sourceExpression: 'exchangeSegment', required: false, parserEngine: ParserEngine.JSONPATH },
      ],
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['dhan', 'position'],
        aiGenerated: false,
      },
    },
  },

  {
    id: 'dhan_orders',
    name: 'Dhan Orders → ORDER',
    description: 'Map Dhan order data to standard ORDER format',
    category: 'broker',
    tags: ['dhan', 'order', 'trading', 'india'],
    verified: true,
    official: true,

    mappingConfig: {
      name: 'Dhan Order → ORDER',
      description: 'Transform Dhan order data',
      target: {
        schema: 'ORDER',
      },
      extraction: {
        engine: ParserEngine.JSONPATH,
        rootPath: '$.data[*]',
        isArray: true,
      },
      fieldMappings: [
        { targetField: 'orderId', sourceExpression: 'orderId', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'symbol', sourceExpression: 'tradingSymbol', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'side', sourceExpression: 'transactionType', required: true, parserEngine: ParserEngine.JSONPATH, transform: 'uppercase' },
        { targetField: 'type', sourceExpression: 'orderType', required: true, parserEngine: ParserEngine.JSONPATH, transform: 'uppercase' },
        { targetField: 'quantity', sourceExpression: 'quantity', required: true, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'filledQuantity', sourceExpression: 'tradedQuantity', required: false, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'price', sourceExpression: 'price', required: false, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'averagePrice', sourceExpression: 'tradedPrice', required: false, parserEngine: ParserEngine.JSONPATH },
        { targetField: 'status', sourceExpression: 'orderStatus', required: true, parserEngine: ParserEngine.JSONPATH, transform: 'uppercase' },
        { targetField: 'timestamp', sourceExpression: 'createTime', required: true, parserEngine: ParserEngine.JSONPATH, transform: 'toISODate' },
        { targetField: 'exchange', sourceExpression: 'exchangeSegment', required: false, parserEngine: ParserEngine.JSONPATH },
      ],
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['dhan', 'order'],
        aiGenerated: false,
      },
    },
  },

  // ========== ZERODHA (KITE) TEMPLATES ==========

  {
    id: 'zerodha_ohlc',
    name: 'Zerodha OHLC → OHLCV',
    description: 'Map Zerodha/Kite OHLC data to standard OHLCV format',
    category: 'broker',
    tags: ['zerodha', 'kite', 'ohlc', 'india'],
    verified: true,
    official: true,

    mappingConfig: {
      name: 'Zerodha OHLC → OHLCV',
      description: 'Transform Zerodha OHLC data',
      target: {
        schema: 'OHLCV',
      },
      extraction: {
        engine: ParserEngine.CUSTOM_JS,
        rootPath: 'data',
        isArray: false,
      },
      fieldMappings: [
        {
          targetField: 'all',
          sourceExpression: `
            return Object.entries(data).map(([symbol, ohlc]) => ({
              symbol: symbol,
              timestamp: new Date().toISOString(),
              open: ohlc.open,
              high: ohlc.high,
              low: ohlc.low,
              close: ohlc.close || ohlc.last_price,
              volume: ohlc.volume || 0
            }))
          `,
          parserEngine: ParserEngine.CUSTOM_JS,
          required: true,
        },
      ],
      validation: {
        enabled: true,
        strictMode: false,
        rules: [],
      },
      metadata: {
        createdAt: new Date().toISOString(),
        updatedAt: new Date().toISOString(),
        version: 1,
        tags: ['zerodha', 'ohlc'],
        aiGenerated: false,
      },
    },

    instructions: `
1. Configure Zerodha/Kite connection
2. Use endpoint: /quote/ohlc
3. Query params: i=NSE:SBIN&i=NSE:RELIANCE
4. Response: { "data": { "NSE:SBIN": { "open": 590, ... } } }
    `,
  },
];

// Helper to get template by ID
export function getTemplateById(id: string): MappingTemplate | undefined {
  return INDIAN_BROKER_TEMPLATES.find((t) => t.id === id);
}

// Helper to get templates by broker
export function getTemplatesByBroker(broker: string): MappingTemplate[] {
  return INDIAN_BROKER_TEMPLATES.filter((t) => t.tags.includes(broker.toLowerCase()));
}

// Helper to get all broker names
export function getAllBrokers(): string[] {
  const brokers = new Set<string>();
  INDIAN_BROKER_TEMPLATES.forEach((t) => {
    t.tags.forEach((tag) => {
      if (['upstox', 'fyers', 'dhan', 'zerodha', 'angelone'].includes(tag)) {
        brokers.add(tag);
      }
    });
  });
  return Array.from(brokers);
}
